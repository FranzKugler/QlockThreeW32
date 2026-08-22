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
#include <esp_system.h>   // esp_reset_reason(), for the debug tab

#include "WebRoutes.h"
#include "OtaUpdate.h"   // a rename asks for a restart
#include "Settings.h"
#include "LightSensor.h"
#include "Expert.h"
#include "languages/Language.h"
#include "DisplayModes.h"
#include "LedDriverWS2812FastLED.h"   // the corner colours in sendPanel()
#include "Renderer.h"

// Debug and the debugX macros, plus the ring the web UI reads them out of.
#include "LogBuffer.h"

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

// The frame buffer itself, and the sentence read back out of it. Both are
// owned by the render loop; /panel only ever looks.
extern word matrix[16];

// Only for the corner colours in sendPanel(): the coloured corners bypass
// the frame buffer, so the driver is the only place that knows them.
extern LedDriverWS2812FastLED ledDriver;
String displayedWords(byte language);

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

/**
 * Language and the corner LED options.
 *
 * Once the clock is set up, the language may only move within the panel it
 * already has. The panel is a milled sheet of letters, not a setting: on an
 * Italian clock every other language is a wall of letters that spells nothing,
 * and whoever changed it by accident has no way of knowing what went wrong.
 * Expert mode is where a clock is set up; normal mode is where it can no
 * longer be set up wrongly.
 *
 * "Set up" means enrolled - a password has been chosen. A clock with none is
 * left alone, and that is not the same inconsistency it looks like next to
 * Expert::guard(), which closes /log and /ota/* on such a clock too. Those two
 * keep a stranger on the network out. This one keeps the owner from breaking
 * their own face, and someone who has not yet declared the clock set up is
 * still setting it up. Locking them out of the one setting that has to match
 * the hardware would be the worst moment to start.
 *
 * Guarded here rather than only in the browser, for the same reason the expert
 * tabs are: the endpoint is reachable without the UI. And guarded by the
 * *stored* language, not by anything the request says about it.
 */
void updateConfiguration()
{
    JsonDocument doc;
    deserializeJson(doc, server.arg(0));

    byte wanted = (byte)doc["language"].as<int>();
    if (Expert::enrolled() && !Expert::unlocked() && wanted != settings.getLanguage())
    {
        const Language *have = Languages::find(settings.getLanguage());

        // A stored language this firmware does not know says nothing about
        // which panel is on the wall, so it cannot be grounds for a refusal.
        if (have != nullptr && !Languages::samePanel(have, Languages::find(wanted)))
        {
            server.send(403, "application/json", "{\"error\":\"languageNotOnPanel\"}");
            return;
        }
    }

    settings.setLanguage(wanted);
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
 * Writes the automatic brightness curve, three ways:
 *
 *   {luxLow, brightLow, luxHigh, brightHigh}  the two calibration points
 *   {want: 1..100}                            shift the level, keep the slope
 *   {reset: true}                             back to the defaults
 *
 * The clock validates rather than trusts. The UI will not offer to save a bad
 * pair, but this endpoint is reachable without it, and a curve whose points sit
 * on top of each other makes brightnessForLux() swing across its whole range on
 * sensor noise. Rejected with a code the UI can translate, not with a sentence
 * - see errors.js.
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

    // "At the light there is right now, I want this much display." The whole
    // curve is shifted to satisfy that, keeping its slope: the two calibration
    // points say how hard the clock reacts to a change in light, and this says
    // at what level - two different questions that deserve two controls.
    //
    // The current light is taken from the sensor here rather than from the
    // request: the browser polls every two seconds and would be shifting
    // against a reading that has already moved on.
    //
    // This is also the signal a learning version has to collect. It stores one
    // adjustment now, replacing the last; keeping them and fitting a line
    // through the samples is what turns this into the learning step.
    if (doc["want"].is<int>())
    {
        int want = doc["want"];
        if (want < 1 || want > 100)
        {
            server.send(400, "application/json", "{\"error\":\"calibrationRange\"}");
            return;
        }

        float lux = ambientLight.lux();
        int low  = settings.getAutoBrightLow();
        int high = settings.getAutoBrightHigh();
        int delta = want - brightnessForLux(lux,
                                            settings.getAutoLuxLow(), (byte)low,
                                            settings.getAutoLuxHigh(), (byte)high);

        // Moving both ends by the same amount keeps the slope, which is what
        // the two calibration points are for. That is the normal case and the
        // one to preserve.
        int newLow  = low + delta;
        int newHigh = high + delta;

        if (newLow < 1 || newLow > 100 || newHigh < 1 || newHigh > 100)
        {
            // No room to translate: the display cannot go past 100 %, so a
            // curve already reaching it cannot be lifted as a whole. The
            // instruction still has to be carried out - a slider that silently
            // does nothing is the fault this control was built to fix - so the
            // end that would overflow is pinned and the other one solved to
            // put the curve through the requested point exactly. The slope
            // gives way, and only at the extremes.
            //
            // Which end moves is decided by the reading, and the halves are
            // split at the middle so the divisor below can never approach
            // zero: the far end is always at least half a span away.
            float position = luxPosition(lux, settings.getAutoLuxLow(), settings.getAutoLuxHigh());

            newLow  = constrain(newLow, 1, 100);
            newHigh = constrain(newHigh, 1, 100);

            if (position <= 0.5f)
                newLow = lroundf((want - position * newHigh) / (1.0f - position));
            else
                newHigh = lroundf((want - (1.0f - position) * newLow) / position);

            newLow  = constrain(newLow, 1, 100);
            newHigh = constrain(newHigh, 1, 100);
        }

        settings.setAutoBrightLow((byte)newLow);
        settings.setAutoBrightHigh((byte)newHigh);

        debugI("Brightness nudged to %d %% at %.2f lx: curve %d..%d -> %d..%d",
               want, lux, low, high, newLow, newHigh);

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



/** Reset reasons, short enough to put in a table cell and specific enough to matter. */
static const char *resetReasonName()
{
    switch (esp_reset_reason())
    {
        case ESP_RST_POWERON:  return "power-on";
        case ESP_RST_EXT:      return "external";
        case ESP_RST_SW:       return "software";    // our own ESP.restart()
        case ESP_RST_PANIC:    return "panic";       // the interesting one
        case ESP_RST_INT_WDT:  return "watchdog-int";
        case ESP_RST_TASK_WDT: return "watchdog-task";
        case ESP_RST_WDT:      return "watchdog";
        case ESP_RST_BROWNOUT: return "brownout";    // the other interesting one
        case ESP_RST_SDIO:     return "sdio";
        case ESP_RST_DEEPSLEEP: return "deep-sleep";
        // The five below are ESP32-S3 additions, and leaving them out is how
        // this first reported "unknown" for the most ordinary restart there
        // is: a XIAO has no USB-serial converter, so the reset that ends a
        // flash comes from the chip's own USB peripheral and lands on
        // ESP_RST_USB. CPU_LOCKUP is the double exception - worth telling
        // apart from a plain panic, since it usually means the panic handler
        // itself fell over.
        case ESP_RST_USB:        return "usb";
        case ESP_RST_JTAG:       return "jtag";
        case ESP_RST_EFUSE:      return "efuse";
        case ESP_RST_PWR_GLITCH: return "power-glitch";
        case ESP_RST_CPU_LOCKUP: return "cpu-lockup";
        default:               return "unknown";
    }
}

/** Appends a string as a JSON string literal, quotes and all. */
static void appendJsonString(String &out, const char *text)
{
    out += '"';
    for (const char *at = text; *at; at++)
    {
        unsigned char c = (unsigned char)*at;
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += (char)c;
        }
        else if (c < 0x20)
        {
            // Should not occur - the ring stores whole lines with the control
            // characters already taken out - but a JSON document with a raw
            // control character in it is not JSON, and the tab would show
            // nothing at all rather than one odd line.
            char escaped[7];
            snprintf(escaped, sizeof(escaped), "\\u%04x", c);
            out += escaped;
        }
        else
        {
            out += (char)c;   // UTF-8 passes through; JSON is UTF-8
        }
    }
    out += '"';
}

/**
 * The clock's log, and enough of its state to know what it was doing.
 *
 * Polled by the debug tab with the sequence number it last saw, so the usual
 * answer carries nothing but the handful of lines that have appeared since.
 * A freshly opened tab asks with `since=0` and gets the oldest batch first;
 * `more` then says whether another round is worth it, which is what fills the
 * window with the boot in one go rather than one screen every two seconds.
 *
 * `oldest` is not decoration: it says which line the ring still starts at, so
 * the browser can tell "nothing new" from "the ring wrapped and you missed
 * 300 lines". Dropping that silently is exactly what makes a log window
 * untrustworthy.
 *
 * Built into one String with the room reserved up front rather than through
 * ArduinoJson: a hundred lines would put every one of them into a document
 * first and serialise that into a second buffer, and this runs on the same
 * heap an update wants.
 */
void sendLog()
{
    // The log is behind the lock: it carries whatever the clock has said about
    // itself, and that is not for everyone who can reach port 80.
    if (!Expert::guard()) return;

    uint32_t since = 0;
    if (server.hasArg("since")) since = strtoul(server.arg("since").c_str(), nullptr, 10);

    static Log::Line batch[LOG_BATCH];
    size_t count = Log::collect(since, batch, LOG_BATCH);

    uint32_t oldest = Log::oldestSeq();
    uint32_t next = Log::nextSeq();
    // What the caller should ask for next time. With nothing to send that is
    // where it already stood - clamped to the ring, so a restart hands back a
    // smaller number than was asked for instead of one that never comes round.
    uint32_t served = since;
    if (count > 0)        served = batch[count - 1].seq + 1;
    else if (served < oldest) served = oldest;
    else if (served > next)   served = next;

    String out;
    out.reserve(count * (LOG_LINE_MAX + 40) + 400);

    out += "{\"oldest\":";  out += oldest;
    out += ",\"seq\":";     out += served;
    out += ",\"more\":";    out += (served < next) ? "true" : "false";

    // What to look at first when something has gone wrong, and cheap enough to
    // send on every poll. The heap numbers are here because they are the ones
    // the intermittent "could not activate the firmware" points at.
    out += ",\"uptime\":";  out += millis();
    out += ",\"heap\":";    out += ESP.getFreeHeap();
    out += ",\"heapMin\":"; out += ESP.getMinFreeHeap();
    out += ",\"heapBlock\":"; out += ESP.getMaxAllocHeap();
    out += ",\"reset\":\""; out += resetReasonName(); out += '"';

    out += ",\"lines\":[";
    for (size_t i = 0; i < count; i++)
    {
        if (i > 0) out += ',';
        out += "{\"s\":";  out += batch[i].seq;
        out += ",\"t\":";  out += batch[i].ms;
        out += ",\"l\":";  out += batch[i].level;
        out += ",\"m\":";
        appendJsonString(out, batch[i].text);
        out += '}';
    }
    out += "]}";

    server.send(200, "application/json", out);
}


/**
 * Whether the clock is unlocked, and what it would take to unlock it.
 *
 * Carries no secret: the flag, whether a password has ever been set, and how
 * long the reset window is still open. The web UI needs all three before it
 * can decide between "enter your password", "choose one" and "you have five
 * minutes to start over".
 */
void sendExpert()
{
    JsonDocument doc;
    doc["enrolled"]  = Expert::enrolled();
    doc["unlocked"]  = Expert::unlocked();
    doc["grace"]     = Expert::graceRemaining();
    doc["lockedOut"] = Expert::lockedOut();

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

/**
 * Sets, checks or clears the password.
 *
 *   {password}       on a clock with none: stores it and unlocks
 *                    otherwise: unlocks if it matches
 *   {off: true}      locks again - no password needed, see Expert.h
 *   {reset: true}    forgets the password, only inside the grace window
 *
 * Answers with the same shape GET does, so the UI has the new state without a
 * second request, or with a code from the err_* set.
 */
void updateExpert()
{
    JsonDocument doc;
    deserializeJson(doc, server.arg(0));

    if (doc["off"] | false)
    {
        Expert::lock();
        sendExpert();
        return;
    }

    if (doc["reset"] | false)
    {
        if (!Expert::reset())
        {
            server.send(403, "application/json", "{\"error\":\"expertNoGrace\"}");
            return;
        }
        sendExpert();
        return;
    }

    const char *password = doc["password"] | "";

    if (Expert::lockedOut())
    {
        server.send(429, "application/json", "{\"error\":\"expertLockedOut\"}");
        return;
    }

    // Enrolling and unlocking are the same request on purpose. Which one it is
    // depends on the clock, not on what the browser believes about it, and a
    // browser holding a stale answer would otherwise ask for the wrong one.
    bool ok = Expert::enrolled() ? Expert::unlock(password) : Expert::enroll(password);

    if (!ok)
    {
        // "Too short" is only meaningful while enrolling; afterwards saying it
        // would tell whoever is guessing how long the real one is.
        const char *code = Expert::enrolled() ? "expertWrongPassword" : "expertPasswordShort";
        server.send(403, "application/json", String("{\"error\":\"") + code + "\"}");
        return;
    }

    sendExpert();
}


/**
 * Every language this firmware can render, in the order of the numbers stored
 * in NVS - so the index in the array is the value to POST to /configuration.
 *
 * The names used to live in the web UI, ten of them in each of six locale
 * files, in an order that had to agree with the LANGUAGE_* defines without
 * anything checking that it did. Adding a language cost one file here and six
 * there. It costs one file here now: `name` and `uiLocale` have always been
 * part of a Language, this is only the endpoint that hands them over.
 *
 * `panel` groups the ones cut into the same sheet of letters: it is the number
 * of the first language using that panel, so German, Swabian, Bavarian and
 * Saxon all report 0 and Italian reports itself. A clock has exactly one panel
 * and no setting changes that, so outside expert mode the picker offers only
 * the group the clock is already in - see updateConfiguration(), which is
 * where it is actually enforced.
 *
 * The grouping is worked out from the letters every time rather than stored
 * beside them, because a stored group number would be the same fact written
 * twice and the two would drift.
 *
 * Static for a given firmware - the lock state is deliberately *not* folded in
 * here - so the browser asks once and does not poll. It is deliberately not
 * part of /currentState, which is settings.
 */
void sendLanguages()
{
    JsonDocument doc;
    JsonArray list = doc.to<JsonArray>();

    for (byte i = 0; i < LANGUAGE_COUNT; i++)
    {
        const Language *language = Languages::find(i);
        if (language == nullptr) continue;   // a gap in the table, not an error

        byte panel = i;
        for (byte j = 0; j < i; j++)
        {
            if (Languages::samePanel(Languages::find(j), language)) { panel = j; break; }
        }

        JsonObject entry = list.add<JsonObject>();
        entry["value"]    = i;
        entry["code"]     = language->code;
        entry["name"]     = language->name;
        entry["uiLocale"] = language->uiLocale;
        entry["panel"]    = panel;
    }

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}


/**
 * The face, as it is at this moment.
 *
 * Both halves come out of the same place the LEDs do: `rows` is the panel of
 * the language that is running, `on` is read straight off the frame buffer.
 * That is the whole point - the browser is not a second opinion about what the
 * clock ought to be showing, it shows what the clock *is* showing, a wrong
 * render included. Rebuilding the grammar in JavaScript would have been a
 * second implementation to keep in step, and it would have hidden exactly the
 * faults worth seeing.
 *
 * `on` is written as `#` and `.` rather than as a bit mask so that the answer
 * can be read with curl: two aligned grids, the letters and what is lit.
 *
 * The four corners are reported separately, in reading order rather than in
 * the order the strip is soldered. Which row is which corner is written down
 * at Renderer::setCorners, and comes from the clock rather than from the code:
 * following the pixel remapping through the driver gives the wrong answer.
 *
 * With the coloured corners switched on they are not in the display colour at
 * all - the newest one walks the hue wheel with the seconds - so `cornerColors`
 * carries what each is actually showing. The browser cannot work that out and
 * must not try: it asks the driver, through here. The colours are the pure hue
 * at full value; how bright the clock is running is the preview's own business,
 * as it already is for the letters.
 *
 * The field is absent, not empty, when the mode is off. An older web UI then
 * behaves exactly as it did, and a newer one can tell "no colours" from "four
 * dark corners" without a second flag.
 */
void sendPanel()
{
    const Language *language = Languages::find(settings.getLanguage());
    if (language == nullptr)
    {
        server.send(404, "application/json", "{\"error\":\"noPanel\"}");
        return;
    }

    JsonDocument doc;
    doc["language"] = settings.getLanguage();
    doc["code"]     = language->code;
    doc["name"]     = language->name;
    doc["uiLocale"] = language->uiLocale;
    doc["mode"]     = mode;

    JsonArray rows = doc["rows"].to<JsonArray>();
    JsonArray on   = doc["on"].to<JsonArray>();
    for (uint8_t row = 0; row < PANEL_ROWS; row++)
    {
        rows.add(language->rows[row]);

        char lit[PANEL_COLS + 1];
        for (uint8_t col = 0; col < PANEL_COLS; col++)
        {
            lit[col] = (matrix[row] & (1 << (15 - col))) ? '#' : '.';
        }
        lit[PANEL_COLS] = '\0';
        on.add(lit);
    }

    // Reading order: top left, top right, bottom right, bottom left.
    const uint8_t CORNER_ROW[4] = { 1, 0, 3, 2 };
    JsonArray corners = doc["corners"].to<JsonArray>();
    for (uint8_t i = 0; i < 4; i++)
    {
        corners.add((matrix[CORNER_ROW[i]] & 0b11111) == 0b11111);
    }

    if (settings.getRenderColorCorner() && mode == STD_MODE_NORMAL)
    {
        JsonArray colors = doc["cornerColors"].to<JsonArray>();
        for (uint8_t i = 0; i < 4; i++)
        {
            byte hue;
            if (!ledDriver.cornerHue(i, hue))
            {
                colors.add("");
                continue;
            }

            // hsv2rgb_rainbow, not a plain HSV conversion: FastLED's wheel is
            // deliberately not the geometric one, and the preview should show
            // the colour the LED shows.
            CRGB rgb;
            hsv2rgb_rainbow(CHSV(hue, 255, 255), rgb);

            char hex[8];
            snprintf(hex, sizeof(hex), "#%02x%02x%02x", rgb.r, rgb.g, rgb.b);
            colors.add(hex);
        }
    }

    // The same sentence the once-a-minute log line carries, so the browser can
    // put it under the panel without walking the grid again.
    doc["text"] = displayedWords(settings.getLanguage());

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
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
    server.on("/panel", HTTP_GET, sendPanel);
    server.on("/languages", HTTP_GET, sendLanguages);
    server.on("/light", HTTP_POST, updateLight);
    server.on("/manifest.webmanifest", HTTP_GET, sendManifest);
    server.on("/hostname", HTTP_POST, updateHostname);
    server.on("/wifi", HTTP_GET, sendWifiStatus);
    server.on("/wifi", HTTP_POST, updateWifi);
    server.on("/wifi/scan", HTTP_GET, sendWifiScan);
    server.on("/log", HTTP_GET, sendLog);
    server.on("/expert", HTTP_GET, sendExpert);
    server.on("/expert", HTTP_POST, updateExpert);
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
