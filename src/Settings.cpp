/**
 * Settings
 * The user settings of the clock, persisted to NVS as JSON and served to the
 * web UI. NVS rather than the filesystem, because the filesystem partition is
 * overwritten wholesale by an update of the web UI.
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
#include "LittleFS.h"
#include <Preferences.h>
#include "Settings.h"
#include "Renderer.h"

#include <RemoteDebug.h>
extern RemoteDebug Debug;

// The settings live in NVS, not in the filesystem: NVS has its own partition,
// which neither a firmware nor a filesystem update touches. The whole record
// goes in as one JSON string rather than as individual keys - partly to share
// the serialisation with getJSONSettings(), partly because NVS keys are capped
// at 15 characters and "RenderColorCorner" does not fit.
#define NVS_NAMESPACE "qlock"
#define NVS_KEY_CONF  "conf"

// Where the settings used to live. Read once on the first start after the
// update, then removed.
#define LEGACY_CONF_FILE "/qlockconf.json"

// Bumped whenever a stored field changes meaning, so loadSettings() can convert
// an older record instead of silently misreading it. 1 = pre-2.0.1, hue and
// saturation scaled to 0..255.
#define SETTINGS_SCHEMA 2


/**
 *  Konstruktor.
 */
Settings::Settings()
{
    // set default values
    Language = LANGUAGE_DE_BA;
    RenderCornersCw = true;
    RenderColorCorner = false;
    UseLdr = false;
    Brightness = 50;
    ColorHue = 0;
    ColorSat = 0;
    Mode = 1;
    strcpy (Hostname, "QlockThreeW32");
    strcpy (NTPServer, "pool.ntp.org");
    UseDs = true;
    strcpy (TzName, "CET");
    TzWeek = 0;
    TzDoW = 1;
    TzMonth = 10;
    TzHour = 3;
    TzOffset = 60;
    strcpy (TzDsName, "CEST");
    TzDsWeek = 0;
    TzDsDoW = 1;
    TzDsMonth = 3;
    TzDsHour = 2;
    TzDsOffset = 120;
    strcpy (TzZone, "Europe/Berlin");

    OtaChannel = 0;        // stable
    OtaAutoUpdate = false; // opt-in, see Settings.h
    OtaCheckInterval = 24;
}

/**
 * Takes over settings written by a firmware that still kept them in the
 * filesystem. Returns the JSON it found, or an empty string. The file is
 * removed once it is safely in NVS, so this happens exactly once.
 */
String Settings::migrateLegacyFile(Preferences &preferences)
{
    if (!LittleFS.exists(LEGACY_CONF_FILE)) return String("");

    File file = LittleFS.open(LEGACY_CONF_FILE, "r");
    if (!file) return String("");

    String json = file.readString();
    file.close();

    if (json.length() == 0) return String("");

    if (preferences.putString(NVS_KEY_CONF, json) > 0)
    {
        LittleFS.remove(LEGACY_CONF_FILE);
        debugA("Settings migrated from the filesystem to NVS.\n");
    }
    return json;
}

void Settings::loadSettings()
{
    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false))
    {
        debugE("Cannot open NVS, keeping the default settings.\n");
        return;
    }

    String json = preferences.getString(NVS_KEY_CONF, "");
    if (json.length() == 0) json = migrateLegacyFile(preferences);
    preferences.end();

    // Nothing stored yet: first start, or after the NVS partition was erased.
    // The constructor has already set the defaults, so there is nothing to do.
    if (json.length() == 0)
    {
        debugI("No stored settings found, using the defaults.\n");
        return;
    }

    // generate JSON doc
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);

    // check if there was a problem
    if (error)
    {
        debugE("Failed to parse the stored settings.\n");
    }

    // get values - even if we failed before because then we get the default values
    Language =          doc["Language"] | LANGUAGE_DE_BA;
    RenderCornersCw =   doc["RenderCornersCw"] | true;
    RenderColorCorner = doc["RenderColorCorner"] | false;
    UseLdr =            doc["UseLdr"] | false;
    Brightness =        doc["Brightness"] | 50;
    ColorHue =          doc["ColorHue"] | 0;
    ColorSat =          doc["ColorSat"] | 0;
    // The display mode used to be reported to the web UI and settable through
    // /display, but was in neither this function nor fillDocument - so it was
    // the one thing on screen that no restart survived. "Off (dark)" and
    // "time with WiFi status" are settings someone means to keep.
    Mode =              doc["Mode"] | 1;

    // Schema 1 kept hue and saturation scaled to 0..255, which cost precision
    // twice over: 195/90 set in the UI came back as 194/89. They are stored in
    // the UI's own units now, so an older record has to be converted once.
    // Absent field means schema 1 - that is what makes this detectable at all.
    if ((int)(doc["Schema"] | 1) < SETTINGS_SCHEMA)
    {
        ColorHue = (uint16_t)((ColorHue * 359L + 127) / 255);
        ColorSat = (byte)((ColorSat * 100L + 127) / 255);
        debugA("Settings migrated to schema %d (hue %u, sat %u).\n",
               SETTINGS_SCHEMA, ColorHue, ColorSat);
    }

    // A record from the future, or a corrupted one, must not drive the renderer
    // out of range.
    if (ColorHue > 359) ColorHue = 0;
    if (ColorSat > 100) ColorSat = 100;

    // Absent in a record from a firmware that had the name hardcoded, which is
    // exactly the name to fall back to.
    strlcpy(Hostname, doc["Hostname"] | "QlockThreeW32", sizeof(Hostname));

    strcpy(NTPServer, doc["NTPServer"] | "pool.ntp.org");
    UseDs =             doc["UseDs"] | true;
    strcpy(TzName, doc["TzName"] | "CET");
    TzWeek =            doc["TzWeek"] | 0;
    TzDoW =             doc["TzDoW"] | 1;
    TzMonth =           doc["TzMonth"] | 10;
    TzHour =            doc["TzHour"] | 3;
    TzOffset =          doc["TzOffset"] | 60;
    strcpy(TzDsName, doc["TzDsName"] | "CEST");
    TzDsWeek =            doc["TzDsWeek"] | 0;
    TzDsDoW =             doc["TzDsDoW"] | 1;
    TzDsMonth =           doc["TzDsMonth"] | 3;
    TzDsHour =            doc["TzDsHour"] | 2;
    TzDsOffset =          doc["TzDsOffset"] | 120;

    // Absent in a record written before the zone picker existed. Left empty
    // rather than guessed: the rules in such a record may well have been
    // edited by hand, and naming a city that does not match them would be a
    // worse answer than naming none.
    strlcpy(TzZone, doc["TzZone"] | "", sizeof(TzZone));

    OtaChannel =          doc["OtaChannel"] | 0;
    OtaAutoUpdate =       doc["OtaAutoUpdate"] | false;
    OtaCheckInterval =    doc["OtaCheckInterval"] | 24;
}

void Settings::fillDocument(JsonDocument &doc)
{
    doc["Schema"]           = SETTINGS_SCHEMA;
    doc["Language"]         = Language;
    doc["RenderCornersCw"]  = RenderCornersCw;
    doc["RenderColorCorner"]= RenderColorCorner;
    doc["UseLdr"]           = UseLdr;        
    doc["Brightness"]       = Brightness;
    doc["ColorHue"]         = ColorHue;
    doc["ColorSat"]         = ColorSat;
    doc["Mode"]             = Mode;
    doc["Hostname"]         = String(Hostname);

    doc["NTPServer"]        = String(NTPServer);
    doc["UseDs"]            = UseDs;
    doc["TzName"]           = String(TzName);
    doc["TzWeek"]           = TzWeek;
    doc["TzDoW"]            = TzDoW;
    doc["TzMonth"]          = TzMonth;   
    doc["TzHour"]           = TzHour;
    doc["TzOffset"]         = TzOffset;
    doc["TzDsName"]         = String(TzDsName);
    doc["TzDsWeek"]         = TzDsWeek;
    doc["TzDsDoW"]          = TzDsDoW;
    doc["TzDsMonth"]        = TzDsMonth;   
    doc["TzDsHour"]         = TzDsHour;
    doc["TzDsOffset"]       = TzDsOffset;
    doc["TzZone"]           = String(TzZone);

    doc["OtaChannel"]       = OtaChannel;
    doc["OtaAutoUpdate"]    = OtaAutoUpdate;
    doc["OtaCheckInterval"] = OtaCheckInterval;
}

void Settings::storeSettings()
{
    // generate JSON doc
    JsonDocument doc;
    fillDocument(doc);

    String json;
    serializeJson(doc, json);

    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false))
    {
        debugE("Cannot open NVS to store the settings.\n");
        return;
    }

    // Nothing to do when the stored record is already identical. NVS appends a
    // new entry on every write and only erases a sector once a page is full,
    // so skipping the no-ops here directly saves erase cycles.
    if (preferences.getString(NVS_KEY_CONF, "") == json)
    {
        preferences.end();
        return;
    }

    size_t written = preferences.putString(NVS_KEY_CONF, json);
    preferences.end();

    if (written == 0)
    {
        debugE("Failed to write the settings to NVS.\n");
    }
    else
    {
        debugI("Successful write of %u bytes to NVS.\n", written);
    }
}


String Settings::getJSONSettings()
{
	StaticJsonDocument<200> doc;
	doc["display"] = Mode;
	doc["hue"] = ColorHue;
	doc["sat"] = ColorSat;
	doc["lum"] = Brightness;
	doc["automaticLum"] = (bool)UseLdr;
	doc["language"] = Language;
	doc["cornerColor"] = RenderColorCorner?1:0;
	doc["cornerDirection"] = RenderCornersCw?1:0;
    // Also in /wifi, which is where it is edited; here so the shell can put it
    // in the heading and the page title without waiting for the WLAN tab.
    doc["hostname"]        = String(Hostname);

    doc["ntpServer"]        = String(NTPServer);
    doc["useDs"]            = UseDs?1:0;
    doc["tzName"]           = String(TzName);
    doc["tzWeek"]           = TzWeek;
    doc["tzDoW"]            = TzDoW;
    doc["tzMonth"]          = TzMonth;   
    doc["tzHour"]           = TzHour;
    doc["tzOffset"]         = TzOffset;
    doc["tzDsName"]         = String(TzDsName);
    doc["tzDsWeek"]         = TzDsWeek;
    doc["tzDsDoW"]          = TzDsDoW;
    doc["tzDsMonth"]        = TzDsMonth;   
    doc["tzDsHour"]         = TzDsHour;
    doc["tzDsOffset"]       = TzDsOffset;
    doc["tzZone"]           = String(TzZone);

	String res;
	serializeJsonPretty(doc, res);
    debugI("Initial JSON %s", res.c_str());
	return res;
}