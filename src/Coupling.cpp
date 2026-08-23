/**
 * Coupling - see Coupling.h for what this is and why the numbers are measured.
 */
#include "Coupling.h"
#include "LedDriverWS2812FastLED.h"
#include "LogBuffer.h"

#include <ArduinoJson.h>
#include <Preferences.h>

// Beside the brightness curve rather than in it: both belong to the automatic,
// but the curve is rewritten every time somebody nudges the slider and this is
// written once, when a clock is calibrated.
#define COUPLING_NAMESPACE "qlocklight"
#define COUPLING_KEY       "coupling"

extern LedDriverWS2812FastLED ledDriver;

namespace
{
    struct Cell
    {
        uint8_t row, column;
        float red, green, blue;   // lux at full drive on that channel alone
    };

    Cell cells_[COUPLING_MAX_CELLS];
    uint8_t count = 0;

    // The drive response, oldest measurement first: levels descending from 255
    // and what each one gives as a fraction of full.
    uint8_t levels_[COUPLING_MAX_LEVELS];
    float response_[COUPLING_MAX_LEVELS];
    uint8_t levelCount = 0;

    String record;

    /**
     * The drive response table, interpolated.
     *
     * Linear between measured points and straight down to zero below the
     * lowest one. Not a curve fit: an offset model held from 255 down to 24
     * and then broke completely, and the clock spends its evenings below that.
     */
    float responseFor(uint8_t value)
    {
        if (value == 0 || levelCount == 0) return 0.0f;
        if (value >= levels_[0]) return response_[0];

        for (uint8_t i = 0; i + 1 < levelCount; i++)
        {
            uint8_t high = levels_[i], low = levels_[i + 1];
            if (value <= high && value >= low)
            {
                float span = (float)(high - low);
                if (span <= 0.0f) return response_[i];
                return response_[i + 1] +
                       (response_[i] - response_[i + 1]) * (value - low) / span;
            }
        }
        return response_[levelCount - 1] * value / (float)levels_[levelCount - 1];
    }

    bool parse(const String &json)
    {
        JsonDocument doc;
        if (deserializeJson(doc, json) != DeserializationError::Ok) return false;

        JsonObjectConst map = doc["cells"];
        JsonArrayConst levels = doc["drive"]["levels"];
        JsonArrayConst response = doc["drive"]["response"];
        if (map.isNull() || levels.isNull() || response.isNull()) return false;
        if (levels.size() != response.size() || levels.size() == 0) return false;

        uint8_t newLevels = 0;
        for (size_t i = 0; i < levels.size() && newLevels < COUPLING_MAX_LEVELS; i++)
        {
            levels_[newLevels] = levels[i].as<uint8_t>();
            response_[newLevels] = response[i].as<float>();
            newLevels++;
        }

        uint8_t newCount = 0;
        for (JsonPairConst entry : map)
        {
            if (newCount >= COUPLING_MAX_CELLS) break;

            // The key is "row,column" - the shape the script writes and the
            // shape a person reading the record in the NVS explorer can follow
            // back to a letter on the panel.
            int row = -1, column = -1;
            if (sscanf(entry.key().c_str(), "%d,%d", &row, &column) != 2) continue;
            if (row < 0 || column < 0) continue;
            if (LedDriverWS2812FastLED::physicalFor((byte)row, (byte)column) == 255) continue;

            cells_[newCount].row = (uint8_t)row;
            cells_[newCount].column = (uint8_t)column;
            cells_[newCount].red   = entry.value()["r"] | 0.0f;
            cells_[newCount].green = entry.value()["g"] | 0.0f;
            cells_[newCount].blue  = entry.value()["b"] | 0.0f;
            newCount++;
        }
        if (newCount == 0) return false;

        levelCount = newLevels;
        count = newCount;
        return true;
    }
}

void Coupling::begin()
{
    count = 0;
    levelCount = 0;
    record = "";

    Preferences preferences;
    if (!preferences.begin(COUPLING_NAMESPACE, true))
    {
        debugI("Coupling: nothing stored, the display is not compensated");
        return;
    }
    String stored = preferences.getString(COUPLING_KEY, "");
    preferences.end();

    if (stored.length() == 0)
    {
        debugI("Coupling: nothing stored, the display is not compensated");
        return;
    }

    if (!parse(stored))
    {
        debugE("Coupling: stored map is not readable, the display is not compensated");
        return;
    }

    record = stored;
    debugA("Coupling: %d cells, %d drive levels", count, levelCount);
}

bool Coupling::available() { return count > 0; }
uint8_t Coupling::cells()  { return count; }
String Coupling::stored()  { return record; }

float Coupling::contribution()
{
    if (count == 0) return 0.0f;

    float total = 0.0f;
    for (uint8_t i = 0; i < count; i++)
    {
        byte pixel = LedDriverWS2812FastLED::physicalFor(cells_[i].row, cells_[i].column);
        if (pixel == 255) continue;

        CRGB colour = ledDriver.getPixelRaw(pixel);
        total += responseFor(colour.r) * cells_[i].red;
        total += responseFor(colour.g) * cells_[i].green;
        total += responseFor(colour.b) * cells_[i].blue;
    }
    return total;
}

bool Coupling::store(const String &json)
{
    if (!parse(json)) return false;
    record = json;

    Preferences preferences;
    if (!preferences.begin(COUPLING_NAMESPACE, false))
    {
        debugE("Coupling: cannot open NVS");
        return false;
    }
    preferences.putString(COUPLING_KEY, json);
    preferences.end();

    debugA("Coupling: stored, %d cells, %d drive levels", count, levelCount);
    return true;
}

void Coupling::reset()
{
    count = 0;
    levelCount = 0;
    record = "";

    Preferences preferences;
    if (preferences.begin(COUPLING_NAMESPACE, false))
    {
        preferences.remove(COUPLING_KEY);
        preferences.end();
    }
    debugA("Coupling: map cleared, the display is no longer compensated");
}
