/**
 * Settings
 * The user settings of the clock, persisted to LittleFS as JSON and served to
 * the web UI.
 *
 * @mc       ESP32S3
 * @autor    Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 *
 * Version history:
 * V 2.0:  - Consolidated for ESP32-S3 / WS2812B, comments translated to English.
 */
#include "LittleFS.h"
#include "Settings.h"
#include "Renderer.h"

#include <RemoteDebug.h>
extern RemoteDebug Debug;


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

void Settings::storeSettings()
{
    // generate JSON doc
    StaticJsonDocument<512> doc;

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