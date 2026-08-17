/**
 * WebRoutes
 * The clock's HTTP interface: every handler the web UI talks to, the styling
 * of WiFiManager's setup portal, and the state machine behind a network
 * switch.
 *
 * That last one is not a route, but it belongs with them: POST /wifi only
 * records the request and answers immediately, because the response would
 * never leave the old network otherwise, and the switch itself then runs step
 * by step from loop(). Splitting the two halves apart would hide that.
 *
 * The /ota routes are not here - those come with the module that serves them,
 * see OtaUpdate.h.
 *
 * What this reaches for and does not own is listed below. It is a longer list
 * than OtaUpdate's, which is in the nature of the thing: these handlers exist
 * to read and write the clock's state.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  17.8.2026
 * @updated  17.8.2026
 */
#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <ArduinoJson.h>

#include "WebRoutes.h"
#include "OtaUpdate.h"   // a rename asks for a restart
#include "Settings.h"
#include "LightSensor.h"
#include "DisplayModes.h"
#include "Renderer.h"

#include <RemoteDebug.h>
extern RemoteDebug Debug;

extern WebServer server;
extern Settings settings;
extern AmbientLight ambientLight;
extern bool wifiConnected;
// Cleared once the first connection has been dealt with; a network switch
// sets it again so loop() redoes what it does on a fresh connection.
extern bool wifiFirstConnected;

// The display mode in force, which POST /display sets. isKnownMode() comes
// from DisplayModes.h and is defined next to the render switch it guards.
extern byte mode;

// Set to have the face redrawn on the next pass through loop().
extern bool needsUpdateFromRtc;

// Rebuilds the Timezone object after POST /timezone changed the rules.
void applyTimezoneFromSettings();

// Asks for the deferred settings write.
void scheduleSettingsSave();

// The NTP server in use, and the call that points SNTP at a new one.
extern String NTPServerName;
void startNtp();

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
// Empty unless the last switch failed. Holds a code, not a sentence: the web
// UI speaks six languages and translates it - see errorText() in errors.js.
String wifiLastError;
// The variable part of that message, e.g. the SSID. Never translated.
String wifiErrorDetail;


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

/**
 * True for a mode the render switch still has a case for.
 *
 * Guards both the request and the stored value: a clock updating from a build
 * that still offered the uptime display has 4 in NVS, and without this it
 * would land in the switch, match nothing, and leave whatever was in the frame
 * buffer on the face.
 */

void updateDisplay()
{
    JsonDocument doc;
    deserializeJson(doc, server.arg(0));
    byte wanted = doc["display"] | STD_MODE_NORMAL;
    mode = isKnownMode(wanted) ? wanted : STD_MODE_NORMAL;
    settings.setMode(mode);

    needsUpdateFromRtc = true;
    scheduleSettingsSave();

    server.send(200, "application/json", "{msg: ''}");
}

// switch on / off auto luminance functionality
void updateAutoLuminance()
{
    JsonDocument doc;
    deserializeJson(doc, server.arg(0));
    settings.setUseLdr(doc["automaticLum"].as<int>());

    
    needsUpdateFromRtc = true;
    scheduleSettingsSave();

    server.send(200, "application/json", "{msg: ''}");
}

void updateColor()
{
    JsonDocument doc;
    deserializeJson(doc, server.arg(0));
    // Stored in the units they arrive in; the renderer scales them to 8 bit.
    settings.setColorHue((uint16_t)doc["hue"]);
    settings.setColorSat((byte)doc["sat"]);
    settings.setBrightness(doc["lum"]);

    needsUpdateFromRtc = true;
    scheduleSettingsSave();
    
    server.send(200, "application/json", "{msg: ''}");
}

void updateConfiguration()
{
    JsonDocument doc;
    deserializeJson(doc, server.arg(0));
    settings.setLanguage(doc["language"].as<int>());
    settings.setRenderCornersCw(doc["cornerDirection"].as<int>());
    settings.setRenderColorCorner(doc["cornerColor"].as<int>());
    
    needsUpdateFromRtc = true;
    scheduleSettingsSave();
    
    server.send(200, "application/json", "{msg: ''}");
}

/**
 * One changeover rule, built from the stored fields.
 *
 * TimeChangeRule is {abbrev, week, dow, month, hour, offset}. Filling it
 * positionally is what the six call sites used to do, with month and dow the
 * wrong way round - a month number landed in the day-of-week field, where 10
 * is not a weekday at all, and the resulting changeover date was meaningless.
 * Naming the fields here means the order is stated once.
 */

void updateTimezone()
{
    JsonDocument doc;
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

    // Which entry of the zone list the rules came from, so the web UI can show
    // it again after a reload. Purely a label - the rules above are what the
    // clock runs on. Absent in a request from an older web UI, which must not
    // clear a name that is already stored.
    if (doc["tzZone"].is<const char*>()) settings.setTzZone(doc["tzZone"].as<const char*>());

    applyTimezoneFromSettings();

    // if timeserver address changed
    if (NTPServerName != String(settings.getNTPServer()))
    {
        debugI("Timeserver changed from %s to %s", NTPServerName.c_str(), settings.getNTPServer());
        NTPServerName = String(settings.getNTPServer());
        startNtp();
    }

    needsUpdateFromRtc = true;
    scheduleSettingsSave();
    
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

/**
 * Reduces a requested name to something usable as a DNS label: letters, digits
 * and hyphens, no hyphen at either end, at most 32 characters. Returns an empty
 * string when nothing is left, which the caller rejects.
 *
 * Done here rather than trusted from the browser - the endpoint is reachable
 * without going through the UI, and a name with a dot or a space in it would
 * produce an mDNS record nobody can reach.
 */
String sanitizeHostname(const String& wanted)
{
    String clean;
    for (unsigned int i = 0; i < wanted.length() && clean.length() < 32; i++)
    {
        char c = wanted.charAt(i);
        if (isalnum((unsigned char)c) || c == '-') clean += c;
    }
    while (clean.length() && clean.charAt(0) == '-') clean.remove(0, 1);
    while (clean.length() && clean.charAt(clean.length() - 1) == '-') clean.remove(clean.length() - 1);
    return clean;
}

/**
 * Renames the clock and restarts it.
 *
 * A restart because the name is read in six places and only mDNS can be
 * changed while the clock runs - the DHCP name, the OTA name and the setup
 * access point are all read as the interface comes up. Renaming without one
 * left the clock answering to two different names depending on who was asking.
 *
 * The settings go to NVS here and now rather than through the deferred write,
 * which would still be pending when the restart happens and would lose the new
 * name. The answer carries the name that was actually stored, which is not
 * necessarily the one that was asked for.
 */
void updateHostname()
{
    JsonDocument doc;
    deserializeJson(doc, server.arg(0));

    String wanted = sanitizeHostname(doc["hostname"] | "");
    if (wanted.length() == 0)
    {
        server.send(400, "application/json", "{\"error\":\"hostnameInvalid\"}");
        return;
    }

    JsonDocument answer;
    answer["hostname"] = wanted;
    answer["restarting"] = (wanted != String(settings.getHostname()));
    String out;
    serializeJson(answer, out);

    if (wanted != String(settings.getHostname()))
    {
        settings.setHostname(wanted.c_str());
        settings.storeSettings();
        debugA("Renamed to %s, restarting", settings.getHostname());

        // Answer first, then restart from loop() - the response would never
        // reach the browser otherwise. Nothing else is pending: the write above
        // has already happened.
        server.send(200, "application/json", out);
        Ota::scheduleRestart();
        return;
    }

    server.send(200, "application/json", out);
}

/**
 * The web app manifest, which is how Android decides what to put on a home
 * screen - it ignores the apple-touch-icon iOS uses.
 *
 * Served from here rather than shipped as a file in the filesystem image,
 * because the manifest carries the app name and that is per clock. The
 * alternative, building it in the browser and handing Chrome a blob: URL,
 * works but depends on behaviour that has changed between versions; a real
 * response from the clock does not.
 *
 * The icons themselves are static files and do come from the image.
 */
void sendManifest()
{
    JsonDocument doc;
    doc["name"] = settings.getHostname();
    doc["short_name"] = settings.getHostname();
    doc["start_url"] = "/";
    doc["display"] = "standalone";
    doc["background_color"] = "#0d0d0d";
    doc["theme_color"] = "#0d0d0d";

    JsonArray icons = doc["icons"].to<JsonArray>();

    JsonObject small = icons.add<JsonObject>();
    small["src"] = "/icon-192.png";
    small["sizes"] = "192x192";
    small["type"] = "image/png";

    JsonObject large = icons.add<JsonObject>();
    large["src"] = "/icon-512.png";
    large["sizes"] = "512x512";
    large["type"] = "image/png";

    // Declared separately because it is drawn with a wider margin: a launcher
    // may crop a maskable icon to any shape inside the middle 80 %.
    JsonObject maskable = icons.add<JsonObject>();
    maskable["src"] = "/icon-512-maskable.png";
    maskable["sizes"] = "512x512";
    maskable["type"] = "image/png";
    maskable["purpose"] = "maskable";

    String out;
    serializeJson(doc, out);
    server.send(200, "application/manifest+json", out);
}

/**
 * What the light sensor sees. Polled by the colour tab while it is open, which
 * is why it carries the raw reading as well as the smoothed one: placing the
 * sensor behind the front panel is a matter of watching the raw number while
 * moving it, and the smoothed one lags half a minute behind on purpose.
 */
void sendLight()
{
    JsonDocument doc;
    doc["sensor"]    = ambientLight.name();
    doc["present"]   = ambientLight.present();
    doc["available"] = ambientLight.available();
    doc["lux"]       = ambientLight.lux();
    doc["raw"]       = ambientLight.raw();

    // The curve, and what it makes of the current reading. The UI shows that
    // number beside the slider: with the automatic on the slider is not what
    // the display is doing, and a control that lies is worse than no control.
    doc["luxLow"]      = settings.getAutoLuxLow();
    doc["brightLow"]   = settings.getAutoBrightLow();
    doc["luxHigh"]     = settings.getAutoLuxHigh();
    doc["brightHigh"]  = settings.getAutoBrightHigh();
    doc["brightness"]  = brightnessForLux(ambientLight.lux(),
                                          settings.getAutoLuxLow(), settings.getAutoBrightLow(),
                                          settings.getAutoLuxHigh(), settings.getAutoBrightHigh());
    doc["minRatio"]    = CALIBRATION_MIN_RATIO;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

/**
 * Writes the automatic brightness curve: two points of "this much light, this
 * much display".
 *
 * The clock validates rather than trusts. The UI captures both points from a
 * live reading and will not offer to save a bad pair, but this endpoint is
 * reachable without it, and a curve whose points sit on top of each other makes
 * brightnessForLux() swing across its whole range on sensor noise. Rejected
 * with a code the UI can translate, not with a sentence - see errors.js.
 */
void updateLight()
{
    JsonDocument doc;
    deserializeJson(doc, server.arg(0));

    // Back to the cautious default curve, without naming it a second time
    // here: a freshly constructed Settings carries it, and Settings.cpp stays
    // the one place those four numbers are written down.
    if (doc["reset"] | false)
    {
        Settings defaults;
        settings.setAutoLuxLow(defaults.getAutoLuxLow());
        settings.setAutoBrightLow(defaults.getAutoBrightLow());
        settings.setAutoLuxHigh(defaults.getAutoLuxHigh());
        settings.setAutoBrightHigh(defaults.getAutoBrightHigh());
        needsUpdateFromRtc = true;
        scheduleSettingsSave();
        sendLight();
        return;
    }

    float luxLow     = doc["luxLow"]  | 0.0f;
    float luxHigh    = doc["luxHigh"] | 0.0f;
    int   brightLow  = doc["brightLow"]  | 0;
    int   brightHigh = doc["brightHigh"] | 0;

    if (!(luxHigh > luxLow * CALIBRATION_MIN_RATIO))
    {
        server.send(400, "application/json", "{\"error\":\"calibrationTooClose\"}");
        return;
    }
    if (brightLow < 1 || brightLow > 100 || brightHigh < 1 || brightHigh > 100)
    {
        server.send(400, "application/json", "{\"error\":\"calibrationRange\"}");
        return;
    }

    settings.setAutoLuxLow(luxLow);
    settings.setAutoBrightLow((byte)brightLow);
    settings.setAutoLuxHigh(luxHigh);
    settings.setAutoBrightHigh((byte)brightHigh);

    debugA("Brightness curve: %.2f lx -> %d %%, %.2f lx -> %d %%",
           luxLow, brightLow, luxHigh, brightHigh);

    needsUpdateFromRtc = true;
    scheduleSettingsSave();

    sendLight();
}

// Current connection, plus the outcome of a switch requested via POST /wifi.
void sendWifiStatus()
{
    JsonDocument doc;
    doc["connected"] = (WiFi.status() == WL_CONNECTED);
    doc["ssid"]      = WiFi.SSID();
    doc["ip"]        = WiFi.localIP().toString();
    doc["rssi"]      = WiFi.RSSI();
    doc["mac"]       = WiFi.macAddress();
    doc["hostname"]  = settings.getHostname();
    doc["switching"] = (wifiSwitchState != WIFI_SWITCH_IDLE);
    doc["error"]       = wifiLastError;
    doc["errorDetail"] = wifiErrorDetail;

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
    wifiErrorDetail = "";
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
                wifiLastError = "wifiConnect";
                wifiErrorDetail = wifiPendingSsid;
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
                wifiLastError = "wifiFallback";
                wifiErrorDetail = wifiPreviousSsid;
                wifiSwitchState = WIFI_SWITCH_IDLE;
            }
            break;
        }
        default:
            break;
    }
}



// ------ the interface the rest of the program sees ------

void Web::begin()
{
    // Anything not matched below is looked for in the filesystem, which is
    // where the web UI itself lives.
    server.onNotFound([]()
    {
        if (!handleFileRead(server.uri()))
        {
            server.send(404, "text/plain", "404: Not Found");
        }
    });

    // Answers cross-origin so the Vite dev server can drive a real clock.
    server.enableCORS();

    server.on("/currentState", sendCurrentState);
    server.on("/color", updateColor);
    server.on("/display", updateDisplay);
    server.on("/autoluminance", updateAutoLuminance);
    server.on("/configuration", updateConfiguration);
    server.on("/timezone", updateTimezone);
    server.on("/light", HTTP_GET, sendLight);
    server.on("/light", HTTP_POST, updateLight);
    server.on("/manifest.webmanifest", HTTP_GET, sendManifest);
    server.on("/hostname", HTTP_POST, updateHostname);
    server.on("/wifi", HTTP_GET, sendWifiStatus);
    server.on("/wifi", HTTP_POST, updateWifi);
    server.on("/wifi/scan", HTTP_GET, sendWifiScan);
}

void Web::poll()
{
    handleWifiSwitch();
}

bool Web::switchingNetwork()
{
    return wifiSwitchState != WIFI_SWITCH_IDLE;
}

const char *Web::portalStyle()
{
    return PORTAL_STYLE;
}
