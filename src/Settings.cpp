/**
 * Settings
 * The user settings of the clock, persisted to LittleFS as JSON and served to
 * the web UI.
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

// Where the settings are parked while a filesystem image is written. NVS has
// its own partition, so an update to the filesystem leaves it alone.
#define NVS_NAMESPACE "qlock"
#define NVS_KEY_CONF  "conf"


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
}

void Settings::loadSettings()
{
    if (LittleFS.exists("/qlockconf.json"))
    {
        // If the file exists, open for read
        File file = LittleFS.open("/qlockconf.json", "r");

        // generate JSON doc
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, file);
        
        // check if there was a problem
        if (error) 
        {
            debugE("Failed to read file qlockconf.json.\n");
        }

        // get values - even if we failed before because then we get the default values
        Language =          doc["Language"] | LANGUAGE_DE_BA;
        RenderCornersCw =   doc["RenderCornersCw"] | true;
        RenderColorCorner = doc["RenderColorCorner"] | false;
        UseLdr =            doc["UseLdr"] | false;
        Brightness =        doc["Brightness"] | 50;
        ColorHue =          doc["ColorHue"] | 0;
        ColorSat =          doc["ColorSat"] | 0;

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
 
        file.close(); 
    }
}

void Settings::fillDocument(JsonDocument &doc)
{
    doc["Language"]         = Language;
    doc["RenderCornersCw"]  = RenderCornersCw;
    doc["RenderColorCorner"]= RenderColorCorner;
    doc["UseLdr"]           = UseLdr;        
    doc["Brightness"]       = Brightness;
    doc["ColorHue"]         = ColorHue;
    doc["ColorSat"]         = ColorSat;
    
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
}

void Settings::storeSettings()
{
    // generate JSON doc
    JsonDocument doc;
    fillDocument(doc);

    // open for write
    File file = LittleFS.open("/qlockconf.json", "w+");

    // Serialize JSON to file
    if (serializeJson(doc, file) == 0) 
    {
        debugE("Failed to write file qlockconf.json.\n");
    }
    else
    {
        debugI("Successful write to file qlockconf.json.\n");
    }
    
    file.flush();
    file.close();
}

/**
 * Parks the current settings in NVS, to be picked up by restoreFromNvs() after
 * the next boot. Called before a filesystem image is written, because that
 * image covers the whole partition and takes qlockconf.json with it.
 */
bool Settings::backupToNvs()
{
    JsonDocument doc;
    fillDocument(doc);

    String json;
    serializeJson(doc, json);

    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false))
    {
        debugE("Cannot open NVS to back up the settings.\n");
        return false;
    }

    size_t written = preferences.putString(NVS_KEY_CONF, json);
    preferences.end();

    if (written == 0)
    {
        debugE("Failed to write the settings backup to NVS.\n");
        return false;
    }

    debugI("Settings backed up to NVS (%u bytes).\n", written);
    return true;
}

/**
 * Writes a backup left by backupToNvs() back to the filesystem, so the settings
 * survive a filesystem update. Does nothing when qlockconf.json is present -
 * the file always wins, the backup is only for the case where it is gone.
 */
bool Settings::restoreFromNvs()
{
    if (LittleFS.exists("/qlockconf.json")) return false;

    Preferences preferences;
    if (!preferences.begin(NVS_NAMESPACE, false)) return false;

    String json = preferences.getString(NVS_KEY_CONF, "");
    if (json.length() == 0)
    {
        preferences.end();
        return false;
    }

    File file = LittleFS.open("/qlockconf.json", "w+");
    if (!file)
    {
        preferences.end();
        debugE("Cannot write qlockconf.json while restoring from NVS.\n");
        return false;
    }

    size_t written = file.print(json);
    file.flush();
    file.close();

    // One shot: a later, deliberate wipe of the filesystem should start from
    // the defaults rather than resurrect settings from an old update.
    if (written == json.length()) preferences.remove(NVS_KEY_CONF);
    preferences.end();

    if (written != json.length())
    {
        debugE("Settings restore from NVS incomplete.\n");
        return false;
    }

    return true;
}


String Settings::getJSONSettings()
{
	StaticJsonDocument<200> doc;
	doc["display"] = Mode;
	doc["hue"] = (int)((ColorHue * 359) / 255);
	doc["sat"] = (int)((ColorSat * 100) / 255);
	doc["lum"] = Brightness;
	doc["automaticLum"] = (bool)UseLdr;
	doc["language"] = Language;
	doc["cornerColor"] = RenderColorCorner?1:0;
	doc["cornerDirection"] = RenderCornersCw?1:0;

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

	String res;
	serializeJsonPretty(doc, res);
    debugI("Initial JSON %s", res.c_str());
	return res;
}