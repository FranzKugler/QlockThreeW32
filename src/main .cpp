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
#include <ArduinoOTA.h>

// debug library
#include <RemoteDebug.h>          //https://github.com/JoaoLopesF/RemoteDebug
//#include <RemoteDebugger.h>       //https://github.com/JoaoLopesF/RemoteDebugger    

// needed for NTP
#include <TimeLib.h>
#include <Timezone.h>               // https://github.com/JChristensen/Timezone
#include <NtpClientLib.h>

// JSON library
#include "ArduinoJson.h"

#include "LDR.h"
#include "LedDriver.h"
#include "LedDriverWS2812FastLED.h"
#include "Renderer.h"
#include "Staben.h"
#include "Settings.h"
#include "Zahlen.h"
#include "Woerter_DE.h"

#define FIRMWARE_VERSION "V 2.1.0"
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

boolean syncEventTriggered = false;     // True if a time event has been triggered
NTPSyncEvent_t ntpEvent;   

bool wifiConnected = false;
bool wifiFirstConnected = true;

// Set web server port number to 80
WebServer server(80);

// Variable to store the HTTP request
String header;

// Die persistenten (im EEPROM gespeicherten) Einstellungen.
Settings settings;

// Der Renderer, der die Woerter auf die Matrix ausgibt.
Renderer renderer;

// Leuchtdioden Treiber. 10 macht hier nichts - muss als Konstante in LedDriverWS2812FastLED definiert sein
LedDriverWS2812FastLED ledDriver; 

// Der Helligkeitssensor
//LDR ldr;
unsigned long lastBrightnessCheck;
bool brightnessChanged = false;
float lastDisplayBrightness;
int   lastControllerBrightness;

// mark for initial update
bool needsUpdateFromRtc = true;

// Die Standard-Modi.
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

// Die Matrix, eine Art Bildschirmspeicher
word matrix[16];

// Aktuelle Spalte beim Bildschirmtest
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
            NTP.stop(); // NTP sync can be disabled to avoid sync errors
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
void processSyncEvent (NTPSyncEvent_t ntpEvent) {
    if (ntpEvent < 0) {
        debugE("Time Sync error %d:", ntpEvent);
        if (ntpEvent == noResponse)
            debugE("NTP server not reachable");
        else if (ntpEvent == invalidAddress)
            debugE("Invalid NTP server address");
    } else if (!ntpEvent) {
        debugI("Got NTP time: %s", NTP.getTimeDateString(NTP.getLastNTPSync()).c_str() );
         if(!startTime) startTime = now();
    } else {
        debugI("NTP request Sent");
    }
}

// ------ Webserver related functions ------

// convert the file extension to the MIME type
String getContentType(String filename) 
{ 
    if (filename.endsWith(".html"))     return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js"))  return "application/javascript";
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
    server.send(200, "application/json", server.arg(0) + "(" + settings.getJSONSettings() + ");");
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
        NTP.setNtpServerName(NTPServerName);
    }

    needsUpdateFromRtc = true;
    timeToSaveToFLASH = now() + WAIT_BEFORE_SETTINGS_WRITE;
    
    server.send(200, "application/json", "{msg: ''}");       
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
        // load settings from FLASH
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

    // Handler für WiFi Events registrieren
    WiFi.onEvent(WiFiEvent);

    // LED-Treiber initialisieren
	ledDriver.init();
	// Inhalt des Led-Treibers loeschen...
	ledDriver.clearData();
	// und Inhalt des Bildspeichers loeschen
	renderer.clearScreenBuffer(matrix);
	// wir brauchen nur 10 Zeilen...
	ledDriver.setLinesToWrite(10);

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    // register handler for NTP events
    NTP.onNTPSyncEvent ([](NTPSyncEvent_t event) 
    {
        ntpEvent = event;
        syncEventTriggered = true;
    });

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
    server.on("/color", updateColor);
    server.on("/configuration", updateConfiguration);
    server.on("/timezone", updateTimezone);
    server.begin();
/*
    // over the air update
    ArduinoOTA.setHostname("QlockThreeW32");

    // No authentication by default
    // ArduinoOTA.setPassword("admin");

    // Password can be set with it's md5 value as well
    // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
    // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

    ArduinoOTA.onStart([]() 
    {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) 
        {
            type = "sketch";
        } 
        else 
        { // U_FS
            type = "filesystem";
        }

        // NOTE: if updating FS this would be the place to unmount FS using FS.end()
        LittleFS.end();
        debugA("Start updating %s", type.c_str());
    });

    ArduinoOTA.onEnd([]() 
    {
        debugA("\nEnd\n");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) 
    {
        debugA("Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) 
    {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            debugE("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            debugE("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            debugE("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            debugE("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            debugE("End Failed");
        }
    });
    
    ArduinoOTA.begin();
*/

ArduinoOTA.setPort(8266);
ArduinoOTA.setHostname("QlockThreeW32");
ArduinoOTA.setPassword("admin");

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
    // Werte vom LDR einlesen und vermuellen, da die ersten nichts taugen...
	for (int i = 0; i < 1000; i++)
	{
		analogRead(PIN_LDR);
	}
*/
	// ein paar Infos ausgeben
	debugA("Initialisation done and ready to rock!");

	Serial.flush();

    // no need to save something yet
    timeToSaveToFLASH = std::numeric_limits<time_t>::max();

	// switch on display...
	ledDriver.wakeUp();
}

void loop() 
{
    // this is to check if we have a connection to the WLAN 
    if (!wifiConnected)
    {
        debugI("Wifi lost");
        NTP.stop();
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
        
        // get time again
        NTP.setInterval (300);
        NTP.begin ("pool.ntp.org", timeZone, true, minutesTimeZone);
        
        // update the display too
        needsUpdateFromRtc = true;
    }

    // this is true when we got a new time from the ntp server
    if (syncEventTriggered) 
    {
        processSyncEvent (ntpEvent);
        syncEventTriggered = false;
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
	// Dimmung.
	//
	if (settings.getUseLdr())
	{
        if (millis() < lastBrightnessCheck)
		{
			// wir hatten einen Ueberlauf...
			lastBrightnessCheck = millis();
		}
		if (((lastBrightnessCheck + LDR_CHECK_RATE) < millis()) && !brightnessChanged)
		{ // langsam nachsehen...
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
        // Bildschirmpuffer beschreiben...
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
                int hoursSinceLastSync = (now() - NTP.getLastNTPSync()) / 3600;
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
    	
        // Update mit onChange = true, weil sich hier (aufgrund needsUpdateFromRtc) immer was geaendert hat.
		// Entweder weil wir eine Sekunde weiter sind, oder weil eine Taste gedrueckt wurde.
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
