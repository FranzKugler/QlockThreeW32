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
#ifndef SETTINGS_H
#define SETTINGS_H
#include <FS.h>
#include "Arduino.h"
#include <ArduinoJson.h>
#include <Preferences.h>

class Settings
{
public:
	Settings();
    void loadSettings();
    void storeSettings();
    String getJSONSettings();

    // setters and getters
    void    setFS(fs::FS* fileSystem) {this->fileSystem = fileSystem;}
    void    setLanguage(byte Language) { this->Language = Language;}
    byte    getLanguage() {return this->Language;}
    void    setRenderCornersCw(boolean RenderCornersCw) {this->RenderCornersCw = RenderCornersCw;}
    boolean getRenderCornersCw() {return this->RenderCornersCw;}
    void    setRenderColorCorner(boolean RenderColorCorner) {this->RenderColorCorner = RenderColorCorner;}
    boolean getRenderColorCorner() {return this->RenderColorCorner;}
    void    setUseLdr(boolean UseLdr) {this->UseLdr = UseLdr;}
    boolean getUseLdr() {return this->UseLdr;}
    void    setBrightness(byte Brightness) {this->Brightness = Brightness;}
    byte    getBrightness() {return this->Brightness;}
    // Hue 0..359 and saturation 0..100 - the units the web UI works in, so a
    // value set there comes back unchanged. The conversion to the 8 bits
    // FastLED wants happens once, where the colour is handed to the driver.
    void     setColorHue(uint16_t ColorHue) {this->ColorHue = ColorHue;}
    uint16_t getColorHue() {return this->ColorHue;}
    void     setColorSat(byte ColorSat) {this->ColorSat = ColorSat;}
    byte     getColorSat() {return this->ColorSat;}
    void    setMode (byte Mode) {this->Mode = Mode;}
    byte    getMode() {return this->Mode;}

    // The name the clock answers to: `<hostname>.local` over mDNS, the DHCP
    // name, the name of the setup access point, and the heading of the web UI.
    // A second clock on the same network needs a second name, or the two fight
    // over one mDNS record. Store only a valid DNS label here - the firmware
    // reduces what the UI sends before it gets this far.
    void    setHostname (const char* Hostname) {strlcpy(this->Hostname, Hostname, sizeof(this->Hostname));}
    char*   getHostname() {return this->Hostname;}
    char*   getNTPServer() {return this->NTPServer;}
    void    setNTPServer (const char* NTPServer) {strcpy(this->NTPServer, NTPServer);}
    void    setUseDs(boolean UseDs) {this->UseDs = UseDs;}
    boolean getUseDs() {return this->UseDs;}
    void    setTzName (const char* TzName) {strcpy(this->TzName, TzName);}
    char*   getTzName() {return this->TzName;}
    void    setTzWeek(uint8_t TzWeek) {this->TzWeek = TzWeek;}
    uint8_t getTzWeek() {return this->TzWeek;}
    void    setTzDoW(uint8_t TzDoW) {this->TzDoW = TzDoW;}
    uint8_t getTzDoW() {return this->TzDoW;}
    void    setTzMonth(uint8_t TzMonth) {this->TzMonth = TzMonth;}
    uint8_t getTzMonth() {return this->TzMonth;}
    void    setTzHour(uint8_t TzHour) {this->TzHour = TzHour;}
    uint8_t getTzHour() {return this->TzHour;}
    void    setTzOffset(int TzOffset) {this->TzOffset = TzOffset;}
    int     getTzOffset() {return this->TzOffset;}
    
    void    setTzDsName (const char* TzDsName) {strcpy(this->TzDsName, TzDsName);}
    char*   getTzDsName() {return this->TzDsName;}
    void    setTzDsWeek(uint8_t TzDsWeek) {this->TzDsWeek = TzDsWeek;}
    uint8_t getTzDsWeek() {return this->TzDsWeek;}
    void    setTzDsDoW(uint8_t TzDsDoW) {this->TzDsDoW = TzDsDoW;}
    uint8_t getTzDsDoW() {return this->TzDsDoW;}
    void    setTzDsMonth(uint8_t TzDsMonth) {this->TzDsMonth = TzDsMonth;}
    uint8_t getTzDsMonth() {return this->TzDsMonth;}
    void    setTzDsHour(uint8_t TzDsHour) {this->TzDsHour = TzDsHour;}
    uint8_t getTzDsHour() {return this->TzDsHour;}
    void    setTzDsOffset(int TzDsOffset) {this->TzDsOffset = TzDsOffset;}
    int     getTzDsOffset() {return this->TzDsOffset;}

    // The entry of the zone list the two rules above were filled from, e.g.
    // "Europe/Berlin". A label only: the clock runs on the rules, never on
    // this. It exists because the rules cannot be mapped back - fewer than a
    // hundred distinct rule pairs cover every zone, so Berlin, Paris and Rome
    // are indistinguishable once stored, and the picker could not show what
    // was chosen. Empty when the rules were set by hand.
    void    setTzZone (const char* TzZone) {strlcpy(this->TzZone, TzZone, sizeof(this->TzZone));}
    char*   getTzZone() {return this->TzZone;}

    // Update channel: 0 = stable (tagged releases), 1 = edge (every commit).
    void    setOtaChannel(byte OtaChannel) {this->OtaChannel = OtaChannel;}
    byte    getOtaChannel() {return this->OtaChannel;}
    // Off by default: a bad image that flashes cleanly and then crashes can
    // only be recovered over USB, so installing unattended is opt-in.
    void    setOtaAutoUpdate(boolean OtaAutoUpdate) {this->OtaAutoUpdate = OtaAutoUpdate;}
    boolean getOtaAutoUpdate() {return this->OtaAutoUpdate;}
    // Hours between checks. 0 switches checking off entirely.
    void    setOtaCheckInterval(byte OtaCheckInterval) {this->OtaCheckInterval = OtaCheckInterval;}
    byte    getOtaCheckInterval() {return this->OtaCheckInterval;}

private:
    // Fills a document with the stored representation. Kept separate so the
    // field list exists once, next to the one that reads it back.
    void fillDocument(JsonDocument &doc);
    // Takes over settings from a firmware that kept them in the filesystem.
    String migrateLegacyFile(Preferences &preferences);

    fs::FS  *fileSystem;
    byte    Language;
    boolean RenderCornersCw;
    boolean RenderColorCorner;
    boolean UseLdr;
    byte     Brightness;   // 0..100
    uint16_t ColorHue;     // 0..359
    byte     ColorSat;     // 0..100
    byte    Mode;
    // A DNS label: up to 63 characters in theory, kept to 32 here because it
    // is also typed into a phone's address bar.
    char    Hostname[33];

    // Entries for automatic Timezone
    char    NTPServer[80];
    bool    UseDs;      // Flag if Daylight Saving Time is used
    char    TzName[10]; // Name of timezone
    uint8_t TzWeek;     // First, Second, Third, Fourth, or Last week of the month
    uint8_t TzDoW;      // day of week, 1=Sun, 2=Mon, ... 7=Sat
    uint8_t TzMonth;    // 1=Jan, 2=Feb, ... 12=Dec
    uint8_t TzHour;     // 0-23
    int     TzOffset;   // offset from UTC in minutes
    
    // all fields also for Daylight Saving Time
    char    TzDsName[10]; // Name of timezone
    uint8_t TzDsWeek;     // First, Second, Third, Fourth, or Last week of the month
    uint8_t TzDsDoW;      // day of week, 1=Sun, 2=Mon, ... 7=Sat
    uint8_t TzDsMonth;    // 1=Jan, 2=Feb, ... 12=Dec
    uint8_t TzDsHour;     // 0-23
    int     TzDsOffset;   // offset from UTC in minutes
    // IANA name the rules came from, "" when they were set by hand. The
    // longest name in the database is 30 characters.
    char    TzZone[40];

    // Over the air updates from the release channel
    byte    OtaChannel;       // 0 = stable, 1 = edge
    boolean OtaAutoUpdate;    // install without asking
    byte    OtaCheckInterval; // hours between checks, 0 = never
};

#endif