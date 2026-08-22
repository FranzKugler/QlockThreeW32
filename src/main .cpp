/**
 * QlockThreeW32
 * Main program of the word clock: drives the LED matrix, keeps the time in sync
 * over NTP and serves the configuration web UI from LittleFS.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 *
 * Version history:
 * V 2.0:  - Consolidated for ESP32-S3 / WS2812B, comments translated to English.
 */
#include <Arduino.h>
#include <LittleFS.h>

#include <WiFi.h>

// needed for WifiConfig library
//#include <DNSServer.h>
//#include <mDNS.h> 
//#include <WebServer.h>

#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESPmDNS.h>
#include <WebServer.h>
#include <uri/UriBraces.h>

// over the air updates
#include <ArduinoOTA.h>           // flashing from PlatformIO over the network
#include <Update.h>               // writing an image uploaded through the web UI
#include <HTTPClient.h>           // fetching the manifest and the images
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>       // checking a download against the manifest
#include <esp_ota_ops.h>          // which slot we are running from

// debug library, and the in-memory ring the debug tab reads it out of
#include "LogBuffer.h"           //RemoteDebug: https://github.com/JoaoLopesF/RemoteDebug

// needed for NTP
#include <TimeLib.h>
#include <Timezone.h>               // https://github.com/JChristensen/Timezone
#include <esp_sntp.h>               // the core's own SNTP client, see below

// JSON library
#include "ArduinoJson.h"

#include "LedDriverWS2812FastLED.h"
#include "Renderer.h"
#include "Settings.h"
#include "LightSensor.h"
#include "OtaUpdate.h"
#include "DisplayModes.h"
#include "WebRoutes.h"
#include "Expert.h"
#include "languages/Language.h"
// Credentials of this particular clock, not in version control. Without it the
// build still works; see Secrets.example.h.
#if __has_include("Secrets.h")
#include "Secrets.h"
#endif
#include "Version.h"
#include "Zahlen.h"

#define WAIT_BEFORE_SETTINGS_WRITE 20

// Remote Debug server. A subclass, so that everything said through it lands
// in the ring as well - see LogBuffer.h.
DebugLog Debug;

// NTP specific values (not needed anymore)
int8_t timeZone = 0; //1
int8_t minutesTimeZone = 0;

// Central European Time (Frankfurt, Paris) to start with
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Standard Time

Timezone ActiveTimezone(CEST, CET);
String NTPServerName = String("pool.ntp.org");

// ------ Time synchronisation ------
// The core's SNTP client keeps the system clock in UTC; TimeLib reads it
// through the sync provider below, and the Timezone rules turn UTC into local
// time where it is displayed. NtpClientLib used to do this, but it depends on
// an old release of the Time library whose deprecated Time.h shadows <time.h>
// for every other source in the build, which made HTTPClient impossible to
// compile.

// A system clock still sitting near the epoch has never been set. Anything
// after 2023-11 is a real time.
#define TIME_LOOKS_VALID 1700000000

// Set from the SNTP callback, which runs in another task - hence volatile.
volatile time_t ntpLastSync = 0;
volatile bool ntpSyncPending = false;

bool wifiConnected = false;
bool wifiFirstConnected = true;

// Set web server port number to 80
WebServer server(80);

// Variable to store the HTTP request
String header;

// The persistent settings, stored in flash.
Settings settings;

// Measures the ambient light in the background. Nothing regulates on it yet -
// it is reported through /light so the sensor's placement behind the front
// panel can be judged before a curve is designed around it.
AmbientLight ambientLight;

// The renderer that puts the words onto the matrix.
Renderer renderer;

// LED driver. A value here has no effect - it has to be a constant in LedDriverWS2812FastLED.
LedDriverWS2812FastLED ledDriver;

// WiFiManager. It lives here rather than as a local in setup(), which is what
// the library's own example shows and what this used to do, with the comment
// "once its business is done, there is no need to keep it around".
//
// That is true on the ESP8266 and false here. On the ESP32, WiFi_autoReconnect()
// hands the Arduino core a callback bound to this object:
//
//     wm_event_id = WiFi.onEvent(std::bind(&WiFiManager::WiFiEvent,this,_1,_2));
//
// A std::bind of a member function with two placeholders does not fit in
// std::function's small buffer, so the target is on the heap, and the core keeps
// it in its event list. The object therefore has to outlive every WiFi event -
// which means the whole run of the program, not the run of setup().
WiFiManager wifiManager;

// The light sensor

// mark for initial update
bool needsUpdateFromRtc = true;

#define WIFISTATUS_OFF   0
#define WIFISTATUS_GREEN 1
#define WIFISTATUS_RED   2


// Startmode...
byte mode = STD_MODE_NORMAL;

// The matrix, a kind of frame buffer
word matrix[16];

// Current column during the display test
byte x=0;

// some time markers
time_t lastTick;            // to trigger every second
time_t timeToSaveToFLASH;   // time when we have to save settings to the eeprom

// Transmit power. The part comes up at its maximum, 19.5 dBm, and a transmit
// burst at that level pulls 350-500 mA for a moment. A board fed through a USB
// cable, with a strip of 114 WS2812B on the same 5 V, does not always have that
// to give: the 3.3 V rail dips, the radio drops out mid-burst, and what you see
// is an access point that appears for a second and is gone again - a symptom
// that looks like anything except a supply problem.
//
// 13 dBm is 6 dB down, a quarter of the power, and still far more than a room
// needs. Raise it if the clock ends up somewhere with a weak signal; that is
// the trade this number makes.
#define WIFI_TX_POWER WIFI_POWER_13dBm

/**
 * Sets the transmit power on whichever interface has just come up.
 *
 * It has to be done per interface and after that interface has started, not
 * once in setup(): the value lives in the driver and starting STA or AP puts it
 * back to the maximum. Hence the call from the event handler, which is the only
 * place that knows the radio is up.
 */
static void applyTxPower(const char *which)
{
    WiFi.setTxPower(WIFI_TX_POWER);
    debugI("%s: Sendeleistung auf %.1f dBm gesetzt", which,
           WiFi.getTxPower() * 0.25f);
}

/**
 * Asks for the deferred settings write.
 *
 * Writing on every request would cost a flash erase per slider move, so
 * changes are collected and written WAIT_BEFORE_SETTINGS_WRITE seconds after
 * the last one. Exposed as a function because OtaUpdate has to ask for it too,
 * and that module should not have to know the delay.
 */
void scheduleSettingsSave()
{
    timeToSaveToFLASH = now() + WAIT_BEFORE_SETTINGS_WRITE;
}

// ------ WIFI connection functions ------
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
    debugI("[WiFi-event] event: %d", event);

    switch (event) {
        case ARDUINO_EVENT_WIFI_READY: 
            debugI("WiFi interface ready");
            break;
        case ARDUINO_EVENT_WIFI_SCAN_DONE:
            debugI("Completed scan for access points");
            break;
        case ARDUINO_EVENT_WIFI_STA_START:
            applyTxPower("STA");
            debugI("WiFi client started");
            break;
        case ARDUINO_EVENT_WIFI_STA_STOP:
            debugI("WiFi clients stopped");
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            debugI("Connected to access point %s", (char*)info.wifi_sta_connected.ssid);
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            debugW("Disconnected from SSID: %s", (char*)info.wifi_sta_disconnected.ssid);
            debugW("Reason: %d", (int)event);
            // SNTP is left running: it retries by itself once the network is
            // back, and the system clock keeps counting in the meantime.
            wifiConnected = false;
            break;
        case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
            debugI("Authentication mode of access point has changed");
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            debugI("Obtained IP address: %s", WiFi.localIP().toString().c_str());
            debugI("Connected: %s", WiFi.status () == WL_CONNECTED ? "yes" : "no");
            wifiConnected = true;            
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            debugI("Lost IP address and IP address is reset to 0");
            break;
        case ARDUINO_EVENT_WPS_ER_SUCCESS:
            debugI("WiFi Protected Setup (WPS): succeeded in enrollee mode");
            break;
        case ARDUINO_EVENT_WPS_ER_FAILED:
            debugI("WiFi Protected Setup (WPS): failed in enrollee mode");
            break;
        case ARDUINO_EVENT_WPS_ER_TIMEOUT:
            debugI("WiFi Protected Setup (WPS): timeout in enrollee mode");
            break;
        case ARDUINO_EVENT_WPS_ER_PIN:
            debugI("WiFi Protected Setup (WPS): pin code in enrollee mode");
            break;
        case ARDUINO_EVENT_WIFI_AP_START:
            applyTxPower("AP");
            debugI("WiFi access point started");
            break;
        case ARDUINO_EVENT_WIFI_AP_STOP:
            debugI("WiFi access point  stopped");
            break;
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            debugI("Client connected");
            break;
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            debugI("Client disconnected");
            break;
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
            debugI("Assigned IP address to client");
            break;
        case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
            debugI("Received probe request");
            break;
        case ARDUINO_EVENT_WIFI_AP_GOT_IP6:
            debugI("AP IPv6 is preferred");
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
            debugI("STA IPv6 is preferred");
            break;
        case ARDUINO_EVENT_ETH_GOT_IP6:
            debugI("Ethernet IPv6 is preferred");
            break;
        case ARDUINO_EVENT_ETH_START:
            debugI("Ethernet started");
            break;
        case ARDUINO_EVENT_ETH_STOP:
            debugI("Ethernet stopped");
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            debugI("Ethernet connected");
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            debugI("Ethernet disconnected");
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            debugI("Obtained IP address");
            break;
        default: break;
    }
}

// ------ NTP functions ------

/**
 * Where TimeLib gets its time from. Returning 0 leaves timeStatus() at
 * timeNotSet, which is what we want until SNTP has answered for the first time
 * - otherwise the clock would confidently display 1970.
 */
time_t syncFromSystemClock()
{
    time_t systemTime = time(nullptr);
    return systemTime > TIME_LOOKS_VALID ? systemTime : 0;
}

/** Called by the SNTP task whenever it has set the system clock. */
void onNtpSync(struct timeval *received)
{
    ntpLastSync = received->tv_sec;
    ntpSyncPending = true;
}

/**
 * Points SNTP at the configured server. Safe to call again later: that is how
 * a server changed through the web UI takes effect.
 */
void startNtp()
{
    // UTC, both offsets zero - the Timezone rules do the local conversion.
    configTime(0, 0, NTPServerName.c_str());
    debugI("SNTP started, server %s", NTPServerName.c_str());
}

bool isKnownMode(byte candidate)
{
    switch (candidate)
    {
        case STD_MODE_BLANK:   // and STD_MODE_NIGHT, which is the same number
        case STD_MODE_NORMAL:
        case STD_MODE_SECONDS:
        case EXT_MODE_TEST:
        case EXT_MODE_NORMAL_WIFISTATUS:
            return true;
        default:
            return false;
    }
}

TimeChangeRule tzRuleFrom(const char* abbrev, uint8_t week, uint8_t dow,
                          uint8_t month, uint8_t hour, int offset)
{
    TimeChangeRule rule;
    // abbrev holds five characters plus the terminator, while the settings
    // field is wider - so copy defensively rather than assume it fits.
    strncpy(rule.abbrev, abbrev, sizeof(rule.abbrev) - 1);
    rule.abbrev[sizeof(rule.abbrev) - 1] = '\0';
    rule.week = week;
    rule.dow = dow;
    rule.month = month;
    rule.hour = hour;
    rule.offset = offset;
    return rule;
}

/** In the order of the LANGUAGE_* defines, for the log line below. */
static const char *LANGUAGE_NAMES[] = {
    "DE", "DE-SW", "DE-BA", "DE-SA", "CH", "EN", "FR", "IT", "NL", "ES"
};

/**
 * The letters on the front panel, for the serial log.
 *
 * The panel is a physical German one, and the renderer lights positions, not
 * letters - so setting a language whose grid differs puts words where this one
 * has different letters, and the face really does spell nonsense. English at
 * 00:20 lights IT IS TWENTY PAST TWELVE, which on these letters reads
 * "ES IS DREIVI HALB NZWOLF". That is why the language is logged next to the
 * sentence: it is the only thing that explains such a line.
 *
 * Written without umlauts because it goes to a serial console. Matches the
 * listing in Woerter_DE.h.
 */
String displayedWords(byte language)
{
    const Language *panel = Languages::find(language);
    if (panel == nullptr) return String();

    String out;
    for (uint8_t row = 0; row < PANEL_ROWS; row++)
    {
        bool inWord = false;
        for (uint8_t col = 0; col < PANEL_COLS; col++)
        {
            if (matrix[row] & (1 << (15 - col)))
            {
                if (!inWord && out.length()) out += ' ';
                Languages::appendCell(out, panel->rows[row], col);
                inWord = true;
            }
            else
            {
                inWord = false;
            }
        }
    }
    return out;
}

/**
 * Rebuilds ActiveTimezone from the settings. Without daylight saving the
 * library takes the standard rule on its own.
 */
void applyTimezoneFromSettings()
{
    TimeChangeRule standardTime = tzRuleFrom(settings.getTzName(), settings.getTzWeek(),
                                             settings.getTzDoW(), settings.getTzMonth(),
                                             settings.getTzHour(), settings.getTzOffset());

    if (settings.getUseDs())
    {
        TimeChangeRule summerTime = tzRuleFrom(settings.getTzDsName(), settings.getTzDsWeek(),
                                               settings.getTzDsDoW(), settings.getTzDsMonth(),
                                               settings.getTzDsHour(), settings.getTzDsOffset());
        ActiveTimezone = Timezone(summerTime, standardTime);
    }
    else
    {
        ActiveTimezone = Timezone(standardTime);
    }
}

/**
 * The brightness to drive the LEDs with, this second.
 *
 * With automatic brightness off, or before the sensor has answered once, that
 * is simply the setting - immediately, because someone dragging the slider
 * wants to see the effect while dragging.
 *
 * With it on, the reading is put through the calibrated curve and the result
 * is approached rather than jumped to. The reading is already smoothed over
 * half a minute, so this is not about noise: it is about the step when a lamp
 * is switched on, which arrives as a genuine jump the eye would otherwise
 * catch. An eighth of the remaining distance per tick fades over some twenty
 * seconds, quickly enough not to feel broken and slowly enough not to be the
 * thing you look at.
 *
 * The manual setting is never overwritten while this runs. Switching the
 * automatic off has to give back the brightness the user chose, and the
 * calibration needs it as the "how bright I want it here" half of a point.
 */
byte brightnessToApply()
{
    static int applied = -1;

    if (!settings.getUseLdr() || !ambientLight.available())
    {
        // Remembered, so switching the automatic on fades from what is lit now
        // instead of stepping.
        applied = settings.getBrightness();
        return settings.getBrightness();
    }

    byte target = brightnessForLux(ambientLight.lux(),
                                   settings.getAutoLuxLow(), settings.getAutoBrightLow(),
                                   settings.getAutoLuxHigh(), settings.getAutoBrightHigh());

    if (applied < 0) applied = target;

    int distance = (int)target - applied;
    if (distance != 0)
    {
        int step = abs(distance) / 8;
        if (step < 1) step = 1;
        applied += (distance > 0) ? step : -step;
    }
    return (byte)applied;
}

void setup()
{
    // setup serial
    Serial.begin(115200);
    Serial.println();

    // Start capturing before anything has something to say. Two halves:
    // Log::begin() takes over ESP-IDF's own logging, and setSerialEnabled()
    // opens RemoteDebug's gate - isActive() answers false until either the
    // serial echo is on or a telnet client has connected, so every debugX
    // between here and Debug.begin() used to be discarded. That is the whole
    // of the boot: mounting the filesystem, loading the settings, and
    // WiFiManager deciding between the stored network and its own portal.
    // None of it ever reached the cable either.
    Log::begin();
    Debug.setSerialEnabled(true);

    //reset settings - for testing
    //wifiManager.resetSettings();

    if (!LittleFS.begin())
    {
        // Serious problem
        debugE("LittleFS Mount failed");
    } 
    else 
    {
        debugA("LittleFS Mount succesfull");
        // version of the web UI in flash, for the update tab
        // Reads the web UI's version out of the filesystem and hangs the
        // /ota routes on the server.
        Ota::begin();
        // load settings from NVS (and take over an old qlockconf.json once)
        settings.loadSettings();

        // Apply the stored display mode. Persisting it without this made the
        // web UI show one thing and the face do another: getMode() was never
        // read, so the clock always came up in normal display no matter what
        // the settings said was selected.
        mode = isKnownMode(settings.getMode()) ? settings.getMode() : STD_MODE_NORMAL;

        // set NTPServerName and timezones right away
        NTPServerName = String(settings.getNTPServer());
        applyTimezoneFromSettings();
    }

    // register the WiFi event handler
    WiFi.onEvent(WiFiEvent);

    // initialise the LED driver
	ledDriver.init();
	// clear the LED driver's contents...
	ledDriver.clearData();
	// and clear the frame buffer
	renderer.clearScreenBuffer(matrix);
	// we only need 10 rows...
	ledDriver.setLinesToWrite(10);


    // Feed TimeLib from the system clock, and let SNTP tell us when it has set
    // it. The interval is how often TimeLib re-reads the system clock, not how
    // often SNTP asks the server - that stays at the core's default.
    setSyncProvider(syncFromSystemClock);
    setSyncInterval(60);
    sntp_set_time_sync_notification_cb(onNtpSync);

    // give the config portal the same look as the SPA
    wifiManager.setTitle(settings.getHostname());
    wifiManager.setCustomHeadElement(Web::portalStyle());

    // The name the router lists the clock under. Only read while the interface
    // comes up, so it has to be set before the connection is made.
    WiFi.setHostname(settings.getHostname());

    //tries to connect to last known settings
    //if it does not connect it starts an access point with the specified name
    //and goes into a blocking loop awaiting configuration
    wifiManager.setConfigPortalTimeout(5*60);
    if (!wifiManager.autoConnect(settings.getHostname()))
    {
        debugE("failed to connect, we should reset and see if it connects");
        delay(3000);
        ESP.restart();
        delay(5000);
    }
    else
    {
        wifiConnected = true;
    }
    
    // start debug server since we now have IP connection
    Debug.begin(settings.getHostname(), RemoteDebug::INFO);
    Debug.setSerialEnabled(true);
    //Debug.initDebugger(debugGetDebuggerEnabled, debugHandleDebugger, debugGetHelpDebugger, debugProcessCmdDebugger);
    //debugInitDebugger(&Debug);

    //if you get here you have connected to the WiFi
    debugA("Connected - Local IP: %s", WiFi.localIP().toString().c_str());  
    debugA("Compiled: %s / %s", __DATE__, __TIME__);


    debugA("Version: %s", FIRMWARE_VERSION);
    // Read back rather than repeat what was asked for: the value is set from
    // the event handler, before RemoteDebug exists, so its own line is swallowed
    // - and a transmit power that quietly stayed at the maximum is exactly the
    // kind of thing worth seeing in the boot log.
    debugA("Sendeleistung: %.2f dBm, RSSI %d dBm",
           WiFi.getTxPower() * 0.25f, WiFi.RSSI());
	ledDriver.printSignature();

    // Starts its own sampling task, and says so if no sensor answers.
    ambientLight.begin();

    // Automatic brightness needs something to measure. Left switched on with
    // no sensor it would be a setting that reads "on" and does nothing, so it
    // is cleared here - the web UI hides the section entirely in that case,
    // and hiding a switch that is still on would leave no way to turn it off.
    // Written straight out rather than deferred: the deferred write is armed
    // at the end of setup() and would drop this.
    if (!ambientLight.present() && settings.getUseLdr())
    {
        debugA("No light sensor, switching automatic brightness off");
        settings.setUseLdr(false);
        settings.storeSettings();
    }

    // Start the mDNS responder for <hostname>.local
    if (MDNS.begin(settings.getHostname()))
    {
        debugA("MDNS responder started");
        MDNS.addService("http", "tcp", 80);
    } else {
        // It didn't work
        debugE("Error setting up MDNS responder!");
    }

    // The settings, WLAN and light endpoints, plus the fallback that serves
    // the web UI out of the filesystem. The /ota routes came earlier, with
    // Ota::begin().
    // Confirms that the letters under every word really spell what the word
    // says. Microseconds, and it catches the one mistake a new panel invites:
    // a word one column out, which renders something plausible and wrong.
    Languages::selfCheck();

    // Reads the stored lock state and opens the reset window if this run
    // started at the plug. Before the server, so no request can arrive while
    // the answer to "is this clock unlocked" is still the default.
    Expert::begin();

    Web::begin();
    server.begin();

ArduinoOTA.setPort(8266);
ArduinoOTA.setHostname(settings.getHostname());
#ifdef OTA_PASSWORD
ArduinoOTA.setPassword(OTA_PASSWORD);
#else
// No Secrets.h: espota accepts anyone who can reach the clock.
#warning "No OTA_PASSWORD set - copy src/Secrets.example.h to src/Secrets.h"
#endif

ArduinoOTA
    .onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        debugA("Start updating %s", type);
    })
    .onEnd([]() {
        debugA("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
        debugA("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
        debugE("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) debugE("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) debugE("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) debugE("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) debugE("Receive Failed");
        else if (error == OTA_END_ERROR) debugE("End Failed");
    });

ArduinoOTA.begin();

	// print some info
	debugA("Initialisation done and ready to rock!");

	Serial.flush();

    // no need to save something yet
    timeToSaveToFLASH = std::numeric_limits<time_t>::max();

	// switch the display on...
	ledDriver.wakeUp();
}

void loop()
{
    // A restart is pending - after an update, or after a rename. Nothing else
    // may run in the meantime, because on a filesystem update LittleFS is
    // unmounted underneath us.
    //
    // The settings are the exception, and they have to be: they are what the
    // deferred write still owes, and skipping it loses every change made in
    // the twenty seconds before the restart. That used to be the right call
    // when they lived in qlockconf.json, where writing would have landed in
    // the image just flashed. They live in NVS now, in a partition no update
    // touches, so flushing here is safe.
    if (Ota::restartPending())
    {
        if (Ota::restartDue())
        {
            if (timeToSaveToFLASH != std::numeric_limits<time_t>::max())
            {
                debugA("Flushing pending settings before the restart");
                settings.storeSettings();
            }
            debugA("Restarting");
            server.stop();
            ESP.restart();
        }
        return;
    }

    // drive a network switch requested through the web UI
    Web::poll();

    // this is to check if we have a connection to the WLAN
    // (skipped while switching networks, which manages the connection itself)
    if (!wifiConnected && !Web::switchingNetwork())
    {
        debugI("Wifi lost");
        // we hopefully have a valid SSID stored
        if (WiFi.SSID()) 
        {
            WiFi.begin();
            // wait if we get a connection
            if (WiFi.waitForConnectResult() == WL_CONNECTED)
            {
                // success - yeah!
                debugI("Reconnected - yeah!!!!");
                wifiConnected = true;
                wifiFirstConnected = true; 
            }
        }
    }
    
    // check if we have to get the time for the first time
    if (wifiConnected && wifiFirstConnected) 
    {
        debugI("wifiFirstConnected");
        wifiFirstConnected = false;
        // re-activate Web Server
        server.begin();
        
        // re-activate Over The Air functionality
        //ArduinoOTA.setHostname("QlockThreeW32");
        //ArduinoOTA.begin();
        
        // get time again - now from the server the settings name, where the
        // old code always asked pool.ntp.org whatever was configured
        startNtp();

        // update the display too
        needsUpdateFromRtc = true;
    }

    // Periodic check of the release channel, and the automatic install if it
    // is switched on. The local hour decides the night window; a negative one
    // says the time is not known yet, before the first NTP answer.
    Ota::poll(wifiConnected,
              timeStatus() == timeNotSet ? -1 : hour(ActiveTimezone.toLocal(now())));

    // this is true when SNTP has just set the system clock
    if (ntpSyncPending)
    {
        ntpSyncPending = false;
        debugI("Got NTP time, epoch %lu", (unsigned long)ntpLastSync);
    }

    // things to do when we have WiFi
    if (wifiConnected)
    {
        // let the webserver do its thing
        server.handleClient();
        
        // handle debug requests
        Debug.handle();
    
        // check if there is a firmware update on the line
        ArduinoOTA.handle();
    }

	// check if we have to update the display - set every second
    if (now() > lastTick)
    {
        lastTick = now();
        needsUpdateFromRtc = true;
    }
    
	// we have to change something at the display
    if (needsUpdateFromRtc)
	{
		needsUpdateFromRtc = false;

        // convert time to correct timezone and store in actual. The rule that
        // applied comes back too, so the log below can name it - which is what
        // tells standard time from summer time at a glance.
        TimeChangeRule *activeRule = NULL;
        time_t actual = ActiveTimezone.toLocal(now(), &activeRule);

        // set color and brightness first
        // The only place the user's units meet FastLED's 8 bit ones. Rounded
        // rather than truncated, so the LEDs get the nearest hue the hardware
        // can actually produce - 360 hues do not fit into 256 steps.
        ledDriver.setColorHS((byte)((settings.getColorHue() * 255L + 179) / 359),
                             (byte)((settings.getColorSat() * 255L + 50) / 100));
        ledDriver.setBrightness(brightnessToApply());

        //
        // fill the frame buffer...
        //
        switch (mode)
        {
            case STD_MODE_BLANK:
            {
                renderer.clearScreenBuffer(matrix);
                break;
            }
            case STD_MODE_NORMAL:
            case EXT_MODE_NORMAL_WIFISTATUS:
            {
                debugV("Time %02d:%02d:%02d", hour(actual), minute(actual), second(actual));
                renderer.clearScreenBuffer(matrix);
                renderer.setMinutes(hour(actual), minute(actual), settings.getLanguage(), matrix);
                renderer.setCorners(minute(actual), settings.getRenderCornersCw(), matrix);

                // Once a minute, what the face actually reads - the sentence is
                // read back out of the frame buffer, so it is the letters that
                // are lit and not a second guess at the renderer's rules.
                // Together with the local time, UTC and the name of the rule in
                // force, this is enough to check a timezone without looking at
                // the clock itself.
                static int loggedMinute = -1;
                if (minute(actual) != loggedMinute)
                {
                    loggedMinute = minute(actual);
                    byte lang = settings.getLanguage();
                    debugA("Display %02d:%02d %s (UTC %02d:%02d) [%s] | %s | corners +%d",
                           hour(actual), minute(actual),
                           activeRule ? activeRule->abbrev : "?",
                           hour(now()), minute(now()),
                           lang < LANGUAGE_COUNT ? LANGUAGE_NAMES[lang] : "?",
                           displayedWords(lang).c_str(),
                           minute(actual) % 5);
                }
                if (mode == EXT_MODE_NORMAL_WIFISTATUS)
                {
                    if (wifiConnected)  ledDriver.updateFunkStatus(WIFISTATUS_GREEN);
                    else                ledDriver.updateFunkStatus(WIFISTATUS_RED);   
                } 
                else
                    ledDriver.updateFunkStatus(WIFISTATUS_OFF);  
                break;
            }
            case STD_MODE_SECONDS:
            {
                renderer.clearScreenBuffer(matrix);
                for (byte i = 0; i < 7; i++)
                {
                    matrix[1 + i] |= pgm_read_byte_near(&(ziffern[second() / 10][i])) << 11;
                    matrix[1 + i] |= pgm_read_byte_near(&(ziffern[second() % 10][i])) << 5;
                }
                break;
            }
            case EXT_MODE_TEST:
            {
                renderer.clearScreenBuffer(matrix);
                renderer.setCorners(second() % 5, settings.getRenderCornersCw(), matrix);
                for (int i = 0; i < 11; i++)
                {
                    ledDriver.setPixelInScreenBuffer(x, i, matrix);
                }
                x++;
                if (x > 10)
                {
                    x = 0;
                }
                break;
            }
        }
    	
        // Update with onChange = true, because something always changed here (due to needsUpdateFromRtc).
		// Either a second has passed, or a button was pressed.
		if (mode == STD_MODE_NORMAL && settings.getRenderColorCorner())
		{
			ledDriver.setColorCorners(true, settings.getRenderCornersCw());
			ledDriver.setTimeForCorners(minute(actual), second(actual));
		}
		else
		{
			ledDriver.setColorCorners(false, settings.getRenderCornersCw());
		}
		ledDriver.writeScreenBufferToMatrix(matrix, true);
    }

    // check if we have to save something to the EEPROM
    if (now() >= timeToSaveToFLASH)
    {
        settings.storeSettings();
        debugI("Settings saved to Flash\n");

        // o.k. done - so nothing to save
        timeToSaveToFLASH = std::numeric_limits<time_t>::max();
    }
}
