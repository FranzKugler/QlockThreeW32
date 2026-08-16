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

// debug library
#include <RemoteDebug.h>          //https://github.com/JoaoLopesF/RemoteDebug

// needed for NTP
#include <TimeLib.h>
#include <Timezone.h>               // https://github.com/JChristensen/Timezone
#include <esp_sntp.h>               // the core's own SNTP client, see below

// JSON library
#include "ArduinoJson.h"

#include "LDR.h"
#include "LedDriverWS2812FastLED.h"
#include "Renderer.h"
#include "Staben.h"
#include "Settings.h"
// Credentials of this particular clock, not in version control. Without it the
// build still works; see Secrets.example.h.
#if __has_include("Secrets.h")
#include "Secrets.h"
#endif
#include "Version.h"
#include "Zahlen.h"
#include "Woerter_DE.h"

#define WAIT_BEFORE_SETTINGS_WRITE 20

// Remote Debug server
RemoteDebug Debug;

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

// ------ Switching WiFi networks from the web UI ------
// A wrong password would otherwise lock us out of the network for good, so the
// switch runs as a state machine in loop(): try the new credentials, and fall
// back to the previous ones if they don't come up within WIFI_SWITCH_TIMEOUT.
#define WIFI_SWITCH_TIMEOUT 20000

enum WifiSwitchState {
    WIFI_SWITCH_IDLE,
    WIFI_SWITCH_START,      // request accepted, not acted on yet
    WIFI_SWITCH_WAIT_NEW,   // waiting for the new network
    WIFI_SWITCH_WAIT_OLD    // new one failed, waiting for the old one again
};

WifiSwitchState wifiSwitchState = WIFI_SWITCH_IDLE;
String wifiPendingSsid, wifiPendingPass;
String wifiPreviousSsid, wifiPreviousPass;
unsigned long wifiSwitchDeadline = 0;
// Empty unless the last switch failed; shown by the web UI.
String wifiLastError;

// --- over the air update through the web UI -------------------------------
// Milliseconds to wait between answering a finished upload and restarting, so
// the HTTP response is on the wire before the socket dies with the reboot.
#define OTA_REBOOT_DELAY 1000

// Empty unless the last upload failed; shown by the web UI.
String otaError;
// Version of the filesystem image, read from /version.json at boot.
String otaFsVersion;
// millis() at which to restart, 0 when no restart is pending.
unsigned long otaRebootAt = 0;
// U_FLASH or U_SPIFFS, decided from the first byte of the uploaded image.
int otaCommand = -1;

// --- update from the release channel ---------------------------------------
// The clocks never talk to the GitHub API, they poll one of two fixed URLs.
// Change these when forking; see .github/workflows/release.yml for how they
// are produced.
#define OTA_REPO "FranzKugler/QlockThreeW32"
#define OTA_MANIFEST_STABLE "https://github.com/" OTA_REPO "/releases/latest/download/manifest.json"
#define OTA_MANIFEST_EDGE   "https://github.com/" OTA_REPO "/releases/download/edge/manifest.json"

#define OTA_CHANNEL_STABLE 0
#define OTA_CHANNEL_EDGE   1

// Auto updates only run in this window, so the clock never goes dark in the
// evening for the minute an update takes.
#define OTA_AUTO_HOUR_FROM 2
#define OTA_AUTO_HOUR_TO   5

enum OtaState {
    OTA_STATE_IDLE,
    OTA_STATE_CHECKING,
    OTA_STATE_AVAILABLE,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_FAILED,
    OTA_STATE_INSTALLED       // waiting for the restart
};

// Written by the download task on the other core, read by the web handlers.
volatile OtaState otaState = OTA_STATE_IDLE;
volatile int otaProgress = 0;

// What the manifest of the selected channel offers.
String otaAvailableVersion;
String otaAvailableNotes;
String otaFirmwareUrl, otaFirmwareSha;
String otaFilesystemUrl, otaFilesystemSha;
size_t otaFirmwareSize = 0;
size_t otaFilesystemSize = 0;
time_t otaLastCheck = 0;
time_t otaNextCheck = 0;

// Set web server port number to 80
WebServer server(80);

// Variable to store the HTTP request
String header;

// The persistent settings, stored in flash.
Settings settings;

// The renderer that puts the words onto the matrix.
Renderer renderer;

// LED driver. A value here has no effect - it has to be a constant in LedDriverWS2812FastLED.
LedDriverWS2812FastLED ledDriver; 

// The light sensor
//LDR ldr;
unsigned long lastBrightnessCheck;
bool brightnessChanged = false;
float lastDisplayBrightness;
int   lastControllerBrightness;

// mark for initial update
bool needsUpdateFromRtc = true;

// The standard modes.
#define STD_MODE_BLANK      0
#define STD_MODE_NIGHT      0
#define STD_MODE_NORMAL     1
#define STD_MODE_SECONDS    2
#define EXT_MODE_TEST       3
#define EXT_MODE_UPTIME     4
#define EXT_MODE_DCF_DEBUG  5
#define EXT_MODE_NORMAL_WIFISTATUS 6

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
time_t startTime = 0;

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
            Serial.println("WiFi Protected Setup (WPS): failed in enrollee mode");
            break;
        case ARDUINO_EVENT_WPS_ER_TIMEOUT:
            Serial.println("WiFi Protected Setup (WPS): timeout in enrollee mode");
            break;
        case ARDUINO_EVENT_WPS_ER_PIN:
            debugI("WiFi Protected Setup (WPS): pin code in enrollee mode");
            break;
        case ARDUINO_EVENT_WIFI_AP_START:
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
            Serial.println("Assigned IP address to client");
            break;
        case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
            Serial.println("Received probe request");
            break;
        case ARDUINO_EVENT_WIFI_AP_GOT_IP6:
            Serial.println("AP IPv6 is preferred");
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
            Serial.println("Ethernet disconnected");
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

// ------ Webserver related functions ------

// convert the file extension to the MIME type
String getContentType(String filename) 
{ 
    if (filename.endsWith(".html"))     return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js"))  return "application/javascript";
    else if (filename.endsWith(".json")) return "application/json";
    else if (filename.endsWith(".ico")) return "image/vnd.microsoft.icon";
    return "text/plain";
}

// send the right file to the client (if it exists)
bool handleFileRead(String path) 
{ 
    //Serial.println("handleFileRead: " + path);
    debugI("handleFileRead: %s", path.c_str());
    if (path.endsWith("/")) path += "index.html";           // If a folder is requested, send the index file
    String contentType = getContentType(path);              // Get the MIME type
    if (LittleFS.exists(path)) 
    {   // If the file exists
        File file = LittleFS.open(path, "r");                 // Open it
        server.streamFile(file, contentType);               // And send it to the client
        file.close();                                       // Then close the file again
        return true;
    }
    debugE("\tFile Not Found");
    return false;                                           // If the file doesn't exist, return false
}

// when the index.html page is loaded, this is called to send the actual settings for a correct display
void sendCurrentState()
{
    server.send(200, "application/json", settings.getJSONSettings());
}

void updateDisplay()
{
    StaticJsonDocument<200> doc;
    deserializeJson(doc, server.arg(0));
    mode = doc["display"];
    settings.setMode(mode);

    needsUpdateFromRtc = true;
    timeToSaveToFLASH = now() + WAIT_BEFORE_SETTINGS_WRITE;

    server.send(200, "application/json", "{msg: ''}");
}

// switch on / off auto luminance functionality
void updateAutoLuminance()
{
    StaticJsonDocument<200> doc;
    deserializeJson(doc, server.arg(0));
    settings.setUseLdr(doc["automaticLum"].as<int>());

    //lastDisplayBrightness = ldr.lightLevel(); //ldr.getLDRValue();
    lastControllerBrightness = settings.getBrightness();
    
    needsUpdateFromRtc = true;
    brightnessChanged = true;
    timeToSaveToFLASH = now() + WAIT_BEFORE_SETTINGS_WRITE;

    server.send(200, "application/json", "{msg: ''}");
}

void updateColor()
{
    StaticJsonDocument<200> doc;
    deserializeJson(doc, server.arg(0));
    settings.setColorHue((byte)((int)doc["hue"]*255 / 359));
    settings.setColorSat((byte)((int)doc["sat"]*255 / 100));
    settings.setBrightness(doc["lum"]);

    needsUpdateFromRtc = true;
    timeToSaveToFLASH = now() + WAIT_BEFORE_SETTINGS_WRITE;
    
    server.send(200, "application/json", "{msg: ''}");
}

void updateConfiguration()
{
    StaticJsonDocument<200> doc;
    deserializeJson(doc, server.arg(0));
    settings.setLanguage(doc["language"].as<int>());
    settings.setRenderCornersCw(doc["cornerDirection"].as<int>());
    settings.setRenderColorCorner(doc["cornerColor"].as<int>());
    
    needsUpdateFromRtc = true;
    timeToSaveToFLASH = now() + WAIT_BEFORE_SETTINGS_WRITE;
    
    server.send(200, "application/json", "{msg: ''}");
}

void updateTimezone()
{
    StaticJsonDocument<200> doc;
    deserializeJson(doc, server.arg(0));
    settings.setNTPServer(doc["ntpServer"].as<const char*>());
    settings.setUseDs(doc["useDs"].as<bool>());
    settings.setTzName(doc["tzName"].as<const char*>());
    settings.setTzWeek(doc["tzWeek"].as<unsigned char>());
    settings.setTzMonth(doc["tzMonth"].as<unsigned char>());
    settings.setTzDoW(doc["tzDoW"].as<unsigned char>());
    settings.setTzHour(doc["tzHour"].as<unsigned char>());
    settings.setTzOffset(doc["tzOffset"].as<int>());
    settings.setTzDsName(doc["tzDsName"].as<const char*>());
    settings.setTzDsWeek(doc["tzDsWeek"].as<unsigned char>());
    settings.setTzDsMonth(doc["tzDsMonth"].as<unsigned char>());
    settings.setTzDsDoW(doc["tzDsDoW"].as<unsigned char>());
    settings.setTzDsHour(doc["tzDsHour"].as<unsigned char>());
    settings.setTzDsOffset(doc["tzDsOffset"].as<int>());   
    
    if (settings.getUseDs())
    {
        // set new Daylight saving timezone
        TimeChangeRule SummerTime = {"     ", settings.getTzDsWeek(), settings.getTzDsMonth(), settings.getTzDsDoW(), settings.getTzDsHour(), settings.getTzDsOffset()};
        strncpy(SummerTime.abbrev, settings.getTzDsName(), 6);
        TimeChangeRule StandardTime  = {"     ", settings.getTzWeek(), settings.getTzMonth(), settings.getTzDoW(), settings.getTzHour(), settings.getTzOffset()};
        strncpy(StandardTime.abbrev, settings.getTzName(), 6);
        ActiveTimezone = Timezone(SummerTime, StandardTime);

    }
    else
    {
        // use only Standard time
        TimeChangeRule StandardTime  = {"     ", settings.getTzWeek(), settings.getTzMonth(), settings.getTzDoW(), settings.getTzHour(), settings.getTzOffset()};
        strncpy(StandardTime.abbrev, settings.getTzName(), 6); 
        ActiveTimezone = Timezone(StandardTime);      
    }
    
    // if timeserver address changed
    if (NTPServerName != String(settings.getNTPServer()))
    {
        debugI("Timeserver changed from %s to %s", NTPServerName.c_str(), settings.getNTPServer());
        NTPServerName = String(settings.getNTPServer());
        startNtp();
    }

    needsUpdateFromRtc = true;
    timeToSaveToFLASH = now() + WAIT_BEFORE_SETTINGS_WRITE;
    
    server.send(200, "application/json", "{msg: ''}");       
}

// Restyles the WiFiManager config portal to match the SPA in LittleFS. The
// portal is only reachable in AP mode, where the SPA does not exist, so this is
// the only way to give first-time setup the same look. Follows the system
// light/dark preference, like the SPA does.
static const char PORTAL_STYLE[] = R"CSS(<style>
:root{--bg:#f4f5f7;--surface:#fff;--text:#1c1f23;--muted:#6b7280;--border:#d9dce1;--accent:#3b6ea5}
@media(prefers-color-scheme:dark){:root{--bg:#16181c;--surface:#1f2228;--text:#e6e8eb;--muted:#9aa1ab;--border:#343941;--accent:#6ea8dc}}
body{background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
.wrap{max-width:26rem;padding:0 1rem}
h1,h3{font-weight:600;letter-spacing:.02em}
button,.b{background:var(--accent);border:0;border-radius:7px;color:#fff;font-size:1rem;padding:.65rem}
input,select{background:var(--surface);color:var(--text);border:1px solid var(--border);border-radius:7px;padding:.45rem .55rem;font-size:1rem}
a,a:hover{color:var(--accent)}
.q{color:var(--muted)}
.msg{background:var(--surface);border:1px solid var(--border);border-radius:10px;padding:.6rem .8rem}
</style>)CSS";

// ------ WiFi endpoints for the web UI ------

// Current connection, plus the outcome of a switch requested via POST /wifi.
void sendWifiStatus()
{
    JsonDocument doc;
    doc["connected"] = (WiFi.status() == WL_CONNECTED);
    doc["ssid"]      = WiFi.SSID();
    doc["ip"]        = WiFi.localIP().toString();
    doc["rssi"]      = WiFi.RSSI();
    doc["mac"]       = WiFi.macAddress();
    doc["hostname"]  = "QlockThreeW32";
    doc["switching"] = (wifiSwitchState != WIFI_SWITCH_IDLE);
    doc["error"]     = wifiLastError;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

// Async scan: the first call kicks one off, the web UI polls until it is done.
void sendWifiScan()
{
    JsonDocument doc;
    int found = WiFi.scanComplete();

    if (found == WIFI_SCAN_RUNNING)
    {
        doc["scanning"] = true;
    }
    else if (found == WIFI_SCAN_FAILED)
    {
        // Nothing running and nothing cached: start one.
        WiFi.scanNetworks(true);
        doc["scanning"] = true;
    }
    else
    {
        doc["scanning"] = false;
        JsonArray networks = doc["networks"].to<JsonArray>();
        for (int i = 0; i < found && i < 20; i++)
        {
            JsonObject net = networks.add<JsonObject>();
            net["ssid"]   = WiFi.SSID(i);
            net["rssi"]   = WiFi.RSSI(i);
            net["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        }
        // Drop the cache so the next poll starts a fresh scan.
        WiFi.scanDelete();
    }

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

// Accepts new credentials and answers immediately - the actual switch happens
// in loop(), otherwise the response would never make it out of the old network.
void updateWifi()
{
    JsonDocument doc;
    deserializeJson(doc, server.arg("plain"));

    wifiPendingSsid = doc["ssid"].as<const char*>();
    wifiPendingPass = doc["password"] | "";

    if (wifiPendingSsid.length() == 0)
    {
        server.send(400, "application/json", "{\"error\":\"ssid missing\"}");
        return;
    }

    // Remember what we are on now, so we can come back to it.
    wifiPreviousSsid = WiFi.SSID();
    wifiPreviousPass = WiFi.psk();
    wifiLastError = "";
    wifiSwitchState = WIFI_SWITCH_START;

    debugI("WiFi switch requested: %s", wifiPendingSsid.c_str());
    server.send(200, "application/json", "{msg: ''}");
}

// Drives the switch started by updateWifi(), including the fallback.
void handleWifiSwitch()
{
    switch (wifiSwitchState)
    {
        case WIFI_SWITCH_START:
        {
            WiFi.disconnect();
            WiFi.begin(wifiPendingSsid.c_str(), wifiPendingPass.c_str());
            wifiSwitchDeadline = millis() + WIFI_SWITCH_TIMEOUT;
            wifiSwitchState = WIFI_SWITCH_WAIT_NEW;
            break;
        }
        case WIFI_SWITCH_WAIT_NEW:
        {
            if (WiFi.status() == WL_CONNECTED)
            {
                debugI("WiFi switch done: %s", WiFi.SSID().c_str());
                wifiSwitchState = WIFI_SWITCH_IDLE;
                wifiConnected = true;
                wifiFirstConnected = true;  // re-init NTP and the servers
            }
            else if ((long)(millis() - wifiSwitchDeadline) >= 0)
            {
                debugW("WiFi switch failed, falling back to %s", wifiPreviousSsid.c_str());
                wifiLastError = "Verbindung zu '" + wifiPendingSsid + "' fehlgeschlagen";
                WiFi.disconnect();
                WiFi.begin(wifiPreviousSsid.c_str(), wifiPreviousPass.c_str());
                wifiSwitchDeadline = millis() + WIFI_SWITCH_TIMEOUT;
                wifiSwitchState = WIFI_SWITCH_WAIT_OLD;
            }
            break;
        }
        case WIFI_SWITCH_WAIT_OLD:
        {
            if (WiFi.status() == WL_CONNECTED)
            {
                wifiSwitchState = WIFI_SWITCH_IDLE;
                wifiConnected = true;
                wifiFirstConnected = true;
            }
            else if ((long)(millis() - wifiSwitchDeadline) >= 0)
            {
                // Both networks are gone; the normal reconnect logic takes over.
                debugE("Fallback to %s failed too", wifiPreviousSsid.c_str());
                wifiLastError += " - Rückfall auf '" + wifiPreviousSsid + "' ebenfalls fehlgeschlagen";
                wifiSwitchState = WIFI_SWITCH_IDLE;
            }
            break;
        }
        default:
            break;
    }
}

// Version of the web UI currently in flash. The Vite build writes version.json
// into the filesystem image (see vite.config.js), so the two halves of an
// update can be told apart: firmware and web UI are flashed separately and can
// legitimately differ.
String readFsVersion()
{
    File file = LittleFS.open("/version.json", "r");
    if (!file) return String("");

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) return String("");

    return String(doc["version"] | "");
}

// Defined further down, with the manifest client.
bool otaUpdateAvailable();
bool otaFetchManifest();
bool otaStartInstall();

static const char *OTA_STATE_NAMES[] = {
    "idle", "checking", "available", "downloading", "failed", "installed"
};

void sendOtaStatus()
{
    JsonDocument doc;
    doc["firmwareVersion"] = FIRMWARE_VERSION;
    doc["fsVersion"] = otaFsVersion;
    doc["sketchSize"] = ESP.getSketchSize();
    // Size of the inactive OTA slot, i.e. the largest image that would fit.
    doc["freeSpace"] = ESP.getFreeSketchSpace();
    doc["error"] = otaError;

    // Which slot we booted from. Without this an update to the same version is
    // indistinguishable from nothing having happened at all.
    const esp_partition_t *running = esp_ota_get_running_partition();
    doc["partition"] = running ? running->label : "?";

    doc["channel"] = settings.getOtaChannel();
    doc["autoUpdate"] = (bool)settings.getOtaAutoUpdate();
    doc["checkInterval"] = settings.getOtaCheckInterval();

    doc["state"] = OTA_STATE_NAMES[otaState];
    doc["progress"] = otaProgress;
    doc["availableVersion"] = otaAvailableVersion;
    doc["availableNotes"] = otaAvailableNotes;
    doc["updateAvailable"] = otaUpdateAvailable();
    // Seconds since the last successful check, -1 if there has not been one.
    doc["lastCheck"] = otaLastCheck ? (long)(now() - otaLastCheck) : -1;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

/** Asks the release channel what it has, then answers with the full status. */
void handleOtaCheck()
{
    otaFetchManifest();
    sendOtaStatus();
}

/** Accepts the request and lets the task do the work; answers immediately. */
void handleOtaInstall()
{
    if (!otaUpdateAvailable())
    {
        server.send(409, "application/json", "{\"error\":\"Kein Update verfügbar\"}");
        return;
    }
    if (!otaStartInstall())
    {
        server.send(409, "application/json", "{\"error\":\"Update läuft bereits\"}");
        return;
    }
    sendOtaStatus();
}

void handleOtaConfig()
{
    JsonDocument doc;
    deserializeJson(doc, server.arg("plain"));

    if (!doc["channel"].isNull())
    {
        byte channel = doc["channel"];
        if (channel != settings.getOtaChannel())
        {
            settings.setOtaChannel(channel);
            // What the other channel offers says nothing about this one.
            otaAvailableVersion = "";
            otaAvailableNotes = "";
            otaLastCheck = 0;
            otaState = OTA_STATE_IDLE;
        }
    }
    if (!doc["autoUpdate"].isNull()) settings.setOtaAutoUpdate(doc["autoUpdate"]);
    if (!doc["checkInterval"].isNull()) settings.setOtaCheckInterval(doc["checkInterval"]);

    // Next check is due relative to the new interval.
    otaNextCheck = 0;
    timeToSaveToFLASH = now() + WAIT_BEFORE_SETTINGS_WRITE;

    sendOtaStatus();
}

// Streaming half of POST /ota/upload: the web server calls this for every chunk
// of the multipart body, so the image goes straight to flash as it arrives and
// never has to be held in RAM (it is several times larger than the heap).
void handleOtaUploadData()
{
    HTTPUpload &upload = server.upload();

    switch (upload.status)
    {
        case UPLOAD_FILE_START:
        {
            otaError = "";
            otaCommand = -1;    // decided below, from the first byte
            debugA("OTA upload started: %s", upload.filename.c_str());
            break;
        }
        case UPLOAD_FILE_WRITE:
        {
            if (otaError.length()) break;

            if (otaCommand < 0)
            {
                // ESP32 application images start with the magic byte 0xE9;
                // anything else is taken to be the filesystem image holding
                // the web UI. That saves the user from picking the target.
                otaCommand = (upload.currentSize > 0 && upload.buf[0] == 0xE9) ? U_FLASH : U_SPIFFS;
                debugA("OTA target: %s", otaCommand == U_FLASH ? "firmware" : "filesystem");

                if (otaCommand == U_SPIFFS)
                {
                    // Unmount before writing: LittleFS caches writes, and
                    // flushing them after the image has been written would
                    // corrupt it. The settings are unaffected - they live in
                    // NVS, which is a partition of its own.
                    LittleFS.end();
                }

                if (!Update.begin(UPDATE_SIZE_UNKNOWN, otaCommand))
                {
                    otaError = String("Update abgelehnt: ") + Update.errorString();
                    debugE("OTA begin failed: %s", Update.errorString());
                    break;
                }
            }

            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            {
                otaError = String("Schreibfehler: ") + Update.errorString();
                debugE("OTA write failed: %s", Update.errorString());
            }
            break;
        }
        case UPLOAD_FILE_END:
        {
            if (otaError.length()) break;

            // end(true) verifies the image and only then switches the boot
            // partition, so a truncated upload leaves the old one in charge.
            if (Update.end(true))
            {
                debugA("OTA upload finished: %u bytes", upload.totalSize);
            }
            else
            {
                otaError = String("Image unvollständig: ") + Update.errorString();
                debugE("OTA end failed: %s", Update.errorString());
            }
            break;
        }
        case UPLOAD_FILE_ABORTED:
        {
            Update.abort();
            otaError = "Upload abgebrochen";
            debugW("OTA upload aborted");
            break;
        }
        default:
            break;
    }
}

// Answers POST /ota/upload once the whole body has been through
// handleOtaUploadData(), and schedules the restart on success.
void handleOtaUploadDone()
{
    if (otaError.length())
    {
        JsonDocument doc;
        doc["error"] = otaError;
        String out;
        serializeJson(doc, out);
        server.send(500, "application/json", out);
        return;
    }

    // Nothing was written at all: the request carried no file part.
    if (otaCommand < 0)
    {
        server.send(400, "application/json", "{\"error\":\"Kein Image empfangen\"}");
        return;
    }

    server.sendHeader("Connection", "close");
    server.send(200, "application/json", "{\"msg\":\"\",\"reboot\":true}");
    otaRebootAt = millis() + OTA_REBOOT_DELAY;
}

// --- update from the release channel ---------------------------------------

/**
 * TLS without certificate verification, on purpose.
 *
 * The SHA-256 in the manifest travels over the same connection as the image,
 * so it catches a truncated or corrupted download - which is the realistic
 * failure - but not somebody who can rewrite both. Pinning a root certificate
 * would look better than it is: /ota/upload next door accepts any image from
 * anyone on the network without asking. Worth revisiting the day that endpoint
 * gets authentication, and not before.
 */
void otaPrepareClient(WiFiClientSecure &client, HTTPClient &http)
{
    client.setInsecure();
    client.setTimeout(20);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(20000);
    http.setUserAgent("QlockThreeW32/" FIRMWARE_VERSION);
}

const char *otaManifestUrl()
{
    return settings.getOtaChannel() == OTA_CHANNEL_EDGE ? OTA_MANIFEST_EDGE : OTA_MANIFEST_STABLE;
}

/** Compares major.minor.patch, ignoring anything after a '-'. Only meaningful on stable. */
int otaCompareVersions(const String &left, const String &right)
{
    int a[3] = {0, 0, 0};
    int b[3] = {0, 0, 0};
    sscanf(left.c_str(), "%d.%d.%d", &a[0], &a[1], &a[2]);
    sscanf(right.c_str(), "%d.%d.%d", &b[0], &b[1], &b[2]);
    for (int i = 0; i < 3; i++)
    {
        if (a[i] != b[i]) return a[i] > b[i] ? 1 : -1;
    }
    return 0;
}

/**
 * Whether an offered version should replace an installed one.
 *
 * On stable that means strictly newer. On edge it means merely different:
 * `git describe` output like 2.0.0-4-gabc123 has no order, so "newer" is not
 * defined - and after a reverted commit the clock would otherwise never offer
 * anything again.
 */
bool otaShouldReplace(const String &offered, const String &installed)
{
    if (offered.length() == 0) return false;
    if (settings.getOtaChannel() == OTA_CHANNEL_EDGE) return offered != installed;
    return otaCompareVersions(offered, installed) > 0;
}

bool otaFirmwareOutdated() { return otaShouldReplace(otaAvailableVersion, String(FIRMWARE_VERSION)); }
bool otaFilesystemOutdated() { return otaShouldReplace(otaAvailableVersion, otaFsVersion); }
bool otaUpdateAvailable() { return otaFirmwareOutdated() || otaFilesystemOutdated(); }

/** Fetches and parses the manifest of the selected channel. Blocks for a second or two. */
bool otaFetchManifest()
{
    if (!wifiConnected) return false;

    otaState = OTA_STATE_CHECKING;
    otaError = "";

    WiFiClientSecure client;
    HTTPClient http;
    otaPrepareClient(client, http);

    const char *url = otaManifestUrl();
    debugI("Checking for updates: %s", url);

    if (!http.begin(client, url))
    {
        otaError = "Update-Server nicht erreichbar";
        otaState = OTA_STATE_FAILED;
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        // 404 on stable simply means no tagged release exists yet.
        otaError = String("Manifest nicht abrufbar (HTTP ") + code + ")";
        debugW("Manifest fetch failed: HTTP %d", code);
        http.end();
        otaState = OTA_STATE_FAILED;
        return false;
    }

    JsonDocument doc;
    DeserializationError parseError = deserializeJson(doc, http.getStream());
    http.end();

    if (parseError)
    {
        otaError = "Manifest unlesbar";
        debugE("Manifest parse failed: %s", parseError.c_str());
        otaState = OTA_STATE_FAILED;
        return false;
    }

    otaAvailableVersion = String(doc["version"] | "");
    otaAvailableNotes = String(doc["notes"] | "");
    otaFirmwareUrl = String(doc["firmware"]["url"] | "");
    otaFirmwareSha = String(doc["firmware"]["sha256"] | "");
    otaFirmwareSize = doc["firmware"]["size"] | 0;
    otaFilesystemUrl = String(doc["filesystem"]["url"] | "");
    otaFilesystemSha = String(doc["filesystem"]["sha256"] | "");
    otaFilesystemSize = doc["filesystem"]["size"] | 0;

    otaLastCheck = now();
    otaState = otaUpdateAvailable() ? OTA_STATE_AVAILABLE : OTA_STATE_IDLE;
    debugA("Manifest says %s, %s", otaAvailableVersion.c_str(),
           otaUpdateAvailable() ? "update available" : "up to date");
    return true;
}

/** Lower-case hex of a 32 byte digest. */
String otaHex(const unsigned char *digest)
{
    static const char nibble[] = "0123456789abcdef";
    String out;
    out.reserve(64);
    for (int i = 0; i < 32; i++)
    {
        out += nibble[digest[i] >> 4];
        out += nibble[digest[i] & 0x0f];
    }
    return out;
}

/**
 * Streams one image into its partition, hashing as it goes. The boot partition
 * is only switched once the digest matches, so a corrupted download cannot
 * take the clock down.
 */
bool otaDownloadImage(const String &url, const String &expectedSha, size_t expectedSize, int command)
{
    WiFiClientSecure client;
    HTTPClient http;
    otaPrepareClient(client, http);

    debugA("Downloading %s (%u bytes)", url.c_str(), (unsigned)expectedSize);

    if (!http.begin(client, url))
    {
        otaError = "Download nicht möglich";
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        otaError = String("Download fehlgeschlagen (HTTP ") + code + ")";
        http.end();
        return false;
    }

    if (command == U_SPIFFS)
    {
        // Cached writes would otherwise be flushed over the new image.
        LittleFS.end();
    }

    if (!Update.begin(expectedSize, command))
    {
        otaError = String("Update abgelehnt: ") + Update.errorString();
        http.end();
        return false;
    }

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer[1024];
    size_t written = 0;
    unsigned long lastData = millis();

    while (written < expectedSize)
    {
        size_t available = stream->available();
        if (available)
        {
            int read = stream->readBytes(buffer, min(available, sizeof(buffer)));
            if (read <= 0) break;

            mbedtls_sha256_update(&sha, buffer, read);
            if (Update.write(buffer, read) != (size_t)read)
            {
                otaError = String("Schreibfehler: ") + Update.errorString();
                break;
            }

            written += read;
            otaProgress = (written * 100) / expectedSize;
            lastData = millis();
        }
        else
        {
            if (!http.connected() || millis() - lastData > 20000)
            {
                otaError = "Verbindung während des Downloads abgerissen";
                break;
            }
            delay(5);
        }
    }

    unsigned char digest[32];
    mbedtls_sha256_finish(&sha, digest);
    mbedtls_sha256_free(&sha);
    http.end();

    if (otaError.length())
    {
        Update.abort();
        return false;
    }

    if (written != expectedSize)
    {
        otaError = "Download unvollständig";
        Update.abort();
        return false;
    }

    if (otaHex(digest) != expectedSha)
    {
        otaError = "Prüfsumme stimmt nicht";
        debugE("SHA-256 mismatch: got %s, expected %s", otaHex(digest).c_str(), expectedSha.c_str());
        Update.abort();
        return false;
    }

    if (!Update.end(true))
    {
        otaError = String("Image unvollstaendig: ") + Update.errorString();
        return false;
    }

    debugA("Installed %u bytes, digest verified", (unsigned)written);
    return true;
}

/**
 * Runs the whole install off the loop, so the web server keeps answering and
 * the UI can show progress during the minute or so this takes.
 */
void otaInstallTask(void *unused)
{
    bool ok = true;

    // Filesystem first: a firmware that expects a newer web UI is the less
    // awkward mismatch of the two, should the second half fail.
    if (ok && otaFilesystemOutdated() && otaFilesystemUrl.length())
    {
        ok = otaDownloadImage(otaFilesystemUrl, otaFilesystemSha, otaFilesystemSize, U_SPIFFS);
    }
    if (ok && otaFirmwareOutdated() && otaFirmwareUrl.length())
    {
        ok = otaDownloadImage(otaFirmwareUrl, otaFirmwareSha, otaFirmwareSize, U_FLASH);
    }

    if (ok)
    {
        otaState = OTA_STATE_INSTALLED;
        otaProgress = 100;
        otaRebootAt = millis() + OTA_REBOOT_DELAY;
    }
    else
    {
        otaState = OTA_STATE_FAILED;
        debugE("Update failed: %s", otaError.c_str());
    }

    vTaskDelete(NULL);
}

/** Starts the install unless one is already running. */
bool otaStartInstall()
{
    if (otaState == OTA_STATE_DOWNLOADING) return false;
    if (!otaUpdateAvailable()) return false;

    otaError = "";
    otaProgress = 0;
    otaState = OTA_STATE_DOWNLOADING;

    // Pinned to core 0; loop() and the web server have core 1 to themselves.
    return xTaskCreatePinnedToCore(otaInstallTask, "otaInstall", 10240, NULL, 1, NULL, 0) == pdPASS;
}

void setup()
{
    // setup serial
    Serial.begin(115200);
    Serial.println();

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
        otaFsVersion = readFsVersion();
        // load settings from NVS (and take over an old qlockconf.json once)
        settings.loadSettings();
        // set NTPServerName and timezones right away
        NTPServerName = String(settings.getNTPServer());
        if (settings.getUseDs())
        {
            // set new Daylight saving timezone
            TimeChangeRule SummerTime = {"     ", settings.getTzDsWeek(), settings.getTzDsMonth(), settings.getTzDsDoW(), settings.getTzDsHour(), settings.getTzDsOffset()};
            strncpy(SummerTime.abbrev, settings.getTzDsName(), 6);
            TimeChangeRule StandardTime  = {"     ", settings.getTzWeek(), settings.getTzMonth(), settings.getTzDoW(), settings.getTzHour(), settings.getTzOffset()};
            strncpy(StandardTime.abbrev, settings.getTzName(), 6);
            ActiveTimezone = Timezone(SummerTime, StandardTime);
        }
        else
        {
            // use only Standard time
            TimeChangeRule StandardTime  = {"     ", settings.getTzWeek(), settings.getTzMonth(), settings.getTzDoW(), settings.getTzHour(), settings.getTzOffset()};
            strncpy(StandardTime.abbrev, settings.getTzName(), 6); 
            ActiveTimezone = Timezone(StandardTime);      
        }        
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

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    // Feed TimeLib from the system clock, and let SNTP tell us when it has set
    // it. The interval is how often TimeLib re-reads the system clock, not how
    // often SNTP asks the server - that stays at the core's default.
    setSyncProvider(syncFromSystemClock);
    setSyncInterval(60);
    sntp_set_time_sync_notification_cb(onNtpSync);

    // give the config portal the same look as the SPA
    wifiManager.setTitle("QlockThreeW32");
    wifiManager.setCustomHeadElement(PORTAL_STYLE);

    //tries to connect to last known settings
    //if it does not connect it starts an access point with the specified name
    //and goes into a blocking loop awaiting configuration
    wifiManager.setConfigPortalTimeout(5*60);
    if (!wifiManager.autoConnect("QlockThreeW32")) 
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
    Debug.begin("QlockThreeW32", RemoteDebug::INFO);   
    Debug.setSerialEnabled(true);
    //Debug.initDebugger(debugGetDebuggerEnabled, debugHandleDebugger, debugGetHelpDebugger, debugProcessCmdDebugger);
    //debugInitDebugger(&Debug);

    //if you get here you have connected to the WiFi
    debugA("Connected - Local IP: %s", WiFi.localIP().toString().c_str());  
    debugA("Compiled: %s / %s", __DATE__, __TIME__);


    debugA("Version: %s", FIRMWARE_VERSION);
	ledDriver.printSignature();

    // Start the mDNS responder for qlockthreew.local
    if (MDNS.begin("QlockThreeW32")) 
    {
        debugA("MDNS responder started");
        MDNS.addService("http", "tcp", 80);
    } else {
        // It didn't work
        debugE("Error setting up MDNS responder!");
    }

    // if something is requested that is not in our file system
    server.onNotFound([]() 
    {   // If the client requests any URI
        if (!handleFileRead(server.uri()))
        {   
            server.send(404, "text/plain", "404: Not Found");   // otherwise, respond with a 404 (Not Found) error
        }
    });
    
    // register the different requests to our server 
    server.enableCORS();
    server.on("/currentState", sendCurrentState);
    server.on("/color", updateColor);
    server.on("/display", updateDisplay);
    server.on("/autoluminance", updateAutoLuminance);
    server.on("/configuration", updateConfiguration);
    server.on("/timezone", updateTimezone);
    server.on("/wifi", HTTP_GET, sendWifiStatus);
    server.on("/wifi", HTTP_POST, updateWifi);
    server.on("/wifi/scan", HTTP_GET, sendWifiScan);
    // Firmware update from the browser. The second handler receives the body in
    // chunks, the first one answers once it is through. Deliberately without
    // authentication - if that ever changes, a server.authenticate() at the top
    // of both handlers is all it takes.
    server.on("/ota/status", HTTP_GET, sendOtaStatus);
    server.on("/ota/upload", HTTP_POST, handleOtaUploadDone, handleOtaUploadData);
    server.on("/ota/check", HTTP_GET, handleOtaCheck);
    server.on("/ota/install", HTTP_POST, handleOtaInstall);
    server.on("/ota/config", HTTP_POST, handleOtaConfig);
    server.begin();

ArduinoOTA.setPort(8266);
ArduinoOTA.setHostname("QlockThreeW32");
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

/*
    // read and discard LDR values, as the first ones are useless...
	for (int i = 0; i < 1000; i++)
	{
		analogRead(PIN_LDR);
	}
*/
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
    // An OTA update is through and the clock is about to restart. Nothing else
    // may run in the meantime: on a filesystem update LittleFS is unmounted, so
    // the deferred settings write would land in the image just flashed.
    if (otaRebootAt)
    {
        if ((long)(millis() - otaRebootAt) >= 0)
        {
            debugA("Restarting after OTA update");
            server.stop();
            ESP.restart();
        }
        return;
    }

    // drive a network switch requested through the web UI
    handleWifiSwitch();

    // this is to check if we have a connection to the WLAN
    // (skipped while switching networks, which manages the connection itself)
    if (!wifiConnected && wifiSwitchState == WIFI_SWITCH_IDLE)
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
    // is switched on. Needs the time, both for the interval and for the night
    // window - before the first NTP answer now() is meaningless.
    if (wifiConnected && otaState != OTA_STATE_DOWNLOADING && otaRebootAt == 0 &&
        settings.getOtaCheckInterval() > 0 && timeStatus() != timeNotSet)
    {
        if (otaNextCheck == 0)
        {
            // Not immediately after boot: the clock has better things to do.
            otaNextCheck = now() + 120;
        }
        else if (now() >= otaNextCheck)
        {
            otaNextCheck = now() + (time_t)settings.getOtaCheckInterval() * 3600;
            otaFetchManifest();

            if (settings.getOtaAutoUpdate() && otaUpdateAvailable())
            {
                int localHour = hour(ActiveTimezone.toLocal(now()));
                if (localHour >= OTA_AUTO_HOUR_FROM && localHour < OTA_AUTO_HOUR_TO)
                {
                    debugA("Installing %s automatically", otaAvailableVersion.c_str());
                    otaStartInstall();
                }
                else
                {
                    debugI("Update %s waiting for the night window", otaAvailableVersion.c_str());
                }
            }
        }
    }

    // this is true when SNTP has just set the system clock
    if (ntpSyncPending)
    {
        ntpSyncPending = false;
        debugI("Got NTP time, epoch %lu", (unsigned long)ntpLastSync);
        // The uptime display counts from the first time we knew the time.
        if (!startTime) startTime = now();
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
    
    //
	// Dimming.
	//
	if (settings.getUseLdr())
	{
        if (millis() < lastBrightnessCheck)
		{
			// we had an overflow...
			lastBrightnessCheck = millis();
		}
		if (((lastBrightnessCheck + LDR_CHECK_RATE) < millis()) && !brightnessChanged)
		{ // check slowly...
			//byte lv = ldr.value();
			//byte lv = ldr.calculateDisplayBrightness();
            /*
            if (ledDriver.getBrightness() > lv)
			{
				ledDriver.setBrightness(ledDriver.getBrightness() - 1);
			}
			else if (ledDriver.getBrightness() < lv)
			{
				ledDriver.setBrightness(ledDriver.getBrightness() + 1);
			}
			lastBrightnessCheck = millis();
            */
		}
	}

	// we have to change something at the display
    if (needsUpdateFromRtc)
	{
		needsUpdateFromRtc = false;

        // convert time to correct timezone and store in actual
        time_t actual = ActiveTimezone.toLocal(now());

        // set color and brightness first
        ledDriver.setColorHS(settings.getColorHue(), settings.getColorSat());
        // brightness only if we're in manual mode or slider changed during automatic mode
        if(!settings.getUseLdr() || brightnessChanged) 
            ledDriver.setBrightness(settings.getBrightness());

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
            case EXT_MODE_DCF_DEBUG:
            {
                int hoursSinceLastSync = ntpLastSync ? (now() - ntpLastSync) / 3600 : 99;
                renderer.clearScreenBuffer(matrix);
                for (byte i = 0; i < 7; i++)
                {
                    matrix[1 + i] |= pgm_read_byte_near(&(ziffern[hoursSinceLastSync / 10][i])) << 11;
                    matrix[1 + i] |= pgm_read_byte_near(&(ziffern[hoursSinceLastSync % 10][i])) << 5;
                }
                break;
            }
            case EXT_MODE_UPTIME:
            {
                int hoursSinceStart = (now() - startTime) / 3600;
                renderer.clearScreenBuffer(matrix);
                for (byte i = 0; i < 7; i++)
                {
                    matrix[1 + i] |= pgm_read_byte_near(&(ziffern[hoursSinceStart / 10][i])) << 11;
                    matrix[1 + i] |= pgm_read_byte_near(&(ziffern[hoursSinceStart % 10][i])) << 5;
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

        if (settings.getUseLdr() && brightnessChanged)
        {
            //ldr.setNewReference(lastDisplayBrightness, lastControllerBrightness);
            brightnessChanged = false;
        }
        
        // o.k. done - so nothing to save 
        timeToSaveToFLASH = std::numeric_limits<time_t>::max();
    }
}
