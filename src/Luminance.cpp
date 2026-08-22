/**
 * Luminance
 * See Luminance.h for the model and why it has no "remember" button.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  22.8.2026
 * @updated  22.8.2026
 */
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <math.h>

#include "Luminance.h"
#include "LightSensor.h"   // LUX_FLOOR
#include "LogBuffer.h"

// Its own namespace, next to the settings' "qlock" and expert mode's
// "qlockexpert". One JSON string under one key, the same shape the settings
// use: NVS keys are capped at 15 characters and a dozen of them for one line
// and ten points would be a filing system rather than a record.
#define LUM_NAMESPACE "qlocklight"
#define LUM_KEY       "curve"

namespace
{
    Luminance::Point ring[LUM_POINTS];
    uint8_t count = 0;          // how many of the ring are real
    uint8_t next = 0;           // where the next appended point goes

    float lineSlope = 0.0f;
    float lineOffset = 0.0f;
    bool fittedSlope = false;

    // The nudge in progress, if any.
    bool waiting = false;
    uint8_t wanted = 0;
    uint32_t settleAt = 0;

    /** log10 of a reading, with the floor that keeps log(0) out of it. */
    float logLux(float lux)
    {
        if (!(lux > LUX_FLOOR)) lux = LUX_FLOOR;
        return log10f(lux);
    }

    void defaultLine()
    {
        float lowX = logLux(LUM_DEFAULT_LOW_LUX);
        float highX = logLux(LUM_DEFAULT_HIGH_LUX);

        lineSlope = ((float)LUM_DEFAULT_HIGH_PERCENT - (float)LUM_DEFAULT_LOW_PERCENT)
                  / (highX - lowX);
        lineOffset = (float)LUM_DEFAULT_LOW_PERCENT - lineSlope * lowX;
        fittedSlope = false;
    }

    /**
     * Fits the line through whatever points there are.
     *
     * Least squares on (log10 lux, percent), but only for the slope and only
     * when the points are spread far enough apart to carry one - see the
     * header. Otherwise the slope stands and the offset is the mean residual,
     * which moves the whole line up or down without changing how hard the
     * clock reacts to a change in light.
     */
    void fit()
    {
        if (count == 0)
        {
            defaultLine();
            return;
        }

        float sumX = 0.0f, sumY = 0.0f, lowest = 0.0f, highest = 0.0f;
        for (uint8_t i = 0; i < count; i++)
        {
            float x = logLux(ring[i].lux);
            if (i == 0 || x < lowest) lowest = x;
            if (i == 0 || x > highest) highest = x;
            sumX += x;
            sumY += (float)ring[i].percent;
        }
        float meanX = sumX / count;
        float meanY = sumY / count;

        bool canFitSlope = (highest - lowest) >= LUM_FIT_MIN_DECADES;
        if (canFitSlope)
        {
            float top = 0.0f, bottom = 0.0f;
            for (uint8_t i = 0; i < count; i++)
            {
                float dx = logLux(ring[i].lux) - meanX;
                top += dx * ((float)ring[i].percent - meanY);
                bottom += dx * dx;
            }

            // bottom cannot be 0 here - that would mean no spread at all, which
            // the test above has already excluded - but a slope of zero or less
            // is refused all the same: it would mean the clock gets no brighter,
            // or dimmer, as the room lightens.
            float candidate = (bottom > 0.0f) ? (top / bottom) : 0.0f;
            if (candidate > 0.0f) lineSlope = candidate;
            else canFitSlope = false;
        }

        // Either way the level comes from the points: with a fitted slope this
        // is the least-squares intercept, without one it slides the old line up
        // or down to sit among them.
        lineOffset = meanY - lineSlope * meanX;
        fittedSlope = canFitSlope;
    }

    void store()
    {
        JsonDocument doc;
        doc["slope"] = lineSlope;
        doc["offset"] = lineOffset;
        doc["fitted"] = fittedSlope;

        JsonArray list = doc["points"].to<JsonArray>();
        for (uint8_t i = 0; i < count; i++)
        {
            // Oldest first, so the order survives a reload and the read-out
            // shows what happened in the order it happened.
            uint8_t at = (count < LUM_POINTS) ? i : (uint8_t)((next + i) % LUM_POINTS);
            JsonArray one = list.add<JsonArray>();
            one.add(ring[at].lux);
            one.add(ring[at].percent);
            one.add(ring[at].seconds);
        }

        String out;
        serializeJson(doc, out);

        Preferences preferences;
        if (!preferences.begin(LUM_NAMESPACE, false))
        {
            debugE("Luminance: cannot open NVS");
            return;
        }
        // Compared first, so a re-fit that changes nothing costs no flash write.
        if (preferences.getString(LUM_KEY, "") != out) preferences.putString(LUM_KEY, out);
        preferences.end();
    }

    /**
     * Adds a point, replacing a near neighbour rather than appending.
     *
     * Without this, ten corrections made in one evening push the daylight point
     * out of the ring and the line collapses onto a single lighting condition -
     * which is the failure this whole scheme has to survive, because a person
     * adjusts their clock when they are sitting in front of it, and that is
     * usually the same room at the same time of day.
     */
    void remember(float lux, uint8_t percent, uint32_t seconds)
    {
        for (uint8_t i = 0; i < count; i++)
        {
            float ratio = (lux > ring[i].lux) ? (lux / ring[i].lux) : (ring[i].lux / lux);
            if (ring[i].lux > 0.0f && lux > 0.0f && ratio <= LUM_SAME_LIGHT_RATIO)
            {
                ring[i].lux = lux;
                ring[i].percent = percent;
                ring[i].seconds = seconds;
                return;
            }
        }

        ring[next].lux = lux;
        ring[next].percent = percent;
        ring[next].seconds = seconds;
        next = (uint8_t)((next + 1) % LUM_POINTS);
        if (count < LUM_POINTS) count++;
    }
}

void Luminance::begin()
{
    defaultLine();
    count = 0;
    next = 0;

    Preferences preferences;
    if (!preferences.begin(LUM_NAMESPACE, true))
    {
        debugI("Luminance: no stored curve, using the default");
        return;
    }
    String stored = preferences.getString(LUM_KEY, "");
    preferences.end();

    if (stored.length() == 0)
    {
        debugI("Luminance: no stored curve, using the default");
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, stored) != DeserializationError::Ok)
    {
        debugE("Luminance: stored curve is not readable, using the default");
        return;
    }

    for (JsonArray one : doc["points"].as<JsonArray>())
    {
        if (count >= LUM_POINTS) break;
        ring[count].lux = one[0] | 0.0f;
        ring[count].percent = one[1] | (uint8_t)LUM_MIN_PERCENT;
        ring[count].seconds = one[2] | (uint32_t)0;
        count++;
    }
    next = (uint8_t)(count % LUM_POINTS);

    // Re-fitted rather than trusted: the stored coefficients are there so the
    // read-out and a future tool can see them, but the points are the record
    // and the line is derived. If the two ever disagree the points win.
    fit();

    debugI("Luminance: %d points, %.1f%%/decade at %.1f%%, slope %s",
           count, lineSlope, lineOffset, fittedSlope ? "fitted" : "kept");
}

uint8_t Luminance::forLux(float lux)
{
    float percent = lineSlope * logLux(lux) + lineOffset;

    long value = lroundf(percent);
    if (value < LUM_MIN_PERCENT) value = LUM_MIN_PERCENT;
    if (value > LUM_MAX_PERCENT) value = LUM_MAX_PERCENT;
    return (uint8_t)value;
}

void Luminance::nudged(uint8_t percent)
{
    if (percent < LUM_MIN_PERCENT) percent = LUM_MIN_PERCENT;
    if (percent > LUM_MAX_PERCENT) percent = LUM_MAX_PERCENT;

    wanted = percent;
    waiting = true;
    settleAt = millis() + LUM_SETTLE_MS;
}

bool Luminance::adjusting(uint8_t &percent)
{
    if (!waiting) return false;
    percent = wanted;
    return true;
}

bool Luminance::poll(float lux)
{
    if (!waiting) return false;

    // Unsigned arithmetic, so this survives millis() wrapping after 49 days.
    if ((int32_t)(millis() - settleAt) < 0) return false;

    waiting = false;
    remember(lux, wanted, (uint32_t)(millis() / 1000));
    fit();
    store();

    debugI("Luminance: learned %.2f lx -> %d%%, now %d points, "
           "%.1f%%/decade at %.1f%% (slope %s)",
           lux, wanted, count, lineSlope, lineOffset,
           fittedSlope ? "fitted" : "kept");
    return true;
}

void Luminance::reset()
{
    count = 0;
    next = 0;
    waiting = false;
    defaultLine();
    store();
    debugI("Luminance: calibration cleared, back to the default line");
}

float Luminance::slope()  { return lineSlope; }
float Luminance::offset() { return lineOffset; }
bool Luminance::slopeFitted() { return fittedSlope; }

uint8_t Luminance::points(Point *out, uint8_t max)
{
    uint8_t given = 0;
    for (uint8_t i = 0; i < count && given < max; i++)
    {
        uint8_t at = (count < LUM_POINTS) ? i : (uint8_t)((next + i) % LUM_POINTS);
        out[given++] = ring[at];
    }
    return given;
}
