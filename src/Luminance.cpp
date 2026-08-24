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
    // Oldest first, newest last, always. That is what makes "the oldest one
    // leaves when an eleventh arrives" mean something, and it is the order the
    // read-out shows. It was a ring with a write cursor,
    // which cost nothing to shift and made two things untrue: a replaced point
    // kept the position of the one it replaced, so "the order it happened in"
    // was not the order stored - and there was no way to say which point is
    // the newest, which is now what the line is anchored on.
    Luminance::Point points_[LUM_POINTS];
    uint8_t count = 0;          // how many of the array are real

    float lineSlope = 0.0f;
    float lineOffset = 0.0f;
    bool fittedSlope = false;

    uint8_t rangeLow = LUM_MIN_PERCENT;
    uint8_t rangeHigh = LUM_MAX_PERCENT;

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
     * Fits the line through whatever points there are: least squares, both
     * halves, every point weighted the same.
     *
     * **Averaging out the person is the whole point.** Somebody setting the
     * brightness by eye is guessing, and guessing differently each time; ten
     * statements about a room are worth more than the last one, and the way to
     * use them is to let the errors cancel. Age is deliberately not a weight -
     * a point is not less true for being older, and when an eleventh arrives
     * the first one leaves the ring, which is the only ageing this needs.
     *
     * The slope is only fitted when the points are spread far enough apart to
     * carry one; otherwise the old slope stands and the line is moved to the
     * centroid, which is the same arithmetic with one unknown fixed.
     *
     * **The cost, stated once so it is not rediscovered as a bug.** A
     * correction no longer lands exactly on what was asked for: nudge to 55 %
     * at a light where the fit says 47, and ten seconds later the clock shows
     * something between the two, and repeating it changes nothing, because the
     * replaced point is the same point. That was once treated as the defect
     * and fixed by anchoring the line on the newest point - which converged
     * perfectly and threw away the averaging this feature exists for. It is a
     * trade, not a bug, and this is the end of it that was asked for.
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
            float x = logLux(points_[i].lux);
            if (i == 0 || x < lowest) lowest = x;
            if (i == 0 || x > highest) highest = x;
            sumX += x;
            sumY += (float)points_[i].percent;
        }
        float meanX = sumX / count;
        float meanY = sumY / count;

        bool canFitSlope = (highest - lowest) >= LUM_FIT_MIN_DECADES;
        if (canFitSlope)
        {
            float top = 0.0f, bottom = 0.0f;
            for (uint8_t i = 0; i < count; i++)
            {
                float dx = logLux(points_[i].lux) - meanX;
                top += dx * ((float)points_[i].percent - meanY);
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

        // Through the centroid, whatever the slope turned out to be. With a
        // fitted slope that is the least-squares line; with a kept one it is
        // the same line slid up or down to sit among the points.
        lineOffset = meanY - lineSlope * meanX;
        fittedSlope = canFitSlope;
    }

    void store()
    {
        JsonDocument doc;
        doc["slope"] = lineSlope;
        doc["offset"] = lineOffset;
        doc["fitted"] = fittedSlope;
        doc["min"] = rangeLow;
        doc["max"] = rangeHigh;

        JsonArray list = doc["points"].to<JsonArray>();
        for (uint8_t i = 0; i < count; i++)
        {
            // Already oldest first, so the order survives a reload and the
            // read-out shows what happened in the order it happened - which is
            // also what decides who leaves when the ring is full.
            JsonArray one = list.add<JsonArray>();
            one.add(points_[i].lux);
            one.add(points_[i].percent);
            one.add(points_[i].seconds);
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
            float ratio = (lux > points_[i].lux) ? (lux / points_[i].lux) : (points_[i].lux / lux);
            if (points_[i].lux > 0.0f && lux > 0.0f && ratio <= LUM_SAME_LIGHT_RATIO)
            {
                // Taken out rather than overwritten in place, so that what
                // goes in below is always the last element - otherwise a
                // replaced point would inherit the age of the one it replaced
                // and leave the ring in the wrong order.
                for (uint8_t j = i; j + 1 < count; j++) points_[j] = points_[j + 1];
                count--;
                break;   // at most one neighbour; they cannot overlap
            }
        }

        // Full means the oldest goes. Ten shifts of a twelve byte struct, at
        // the very most once every ten seconds.
        if (count == LUM_POINTS)
        {
            for (uint8_t j = 0; j + 1 < count; j++) points_[j] = points_[j + 1];
            count--;
        }

        points_[count].lux = lux;
        points_[count].percent = percent;
        points_[count].seconds = seconds;
        count++;
    }
}

void Luminance::begin()
{
    defaultLine();
    count = 0;

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

    // Absent in a record written before the range was settable, which reads as
    // the defaults - the honest answer, since that is what such a clock was
    // regulating to.
    rangeLow = doc["min"] | (uint8_t)LUM_MIN_PERCENT;
    rangeHigh = doc["max"] | (uint8_t)LUM_MAX_PERCENT;
    if (rangeHigh <= rangeLow + LUM_RANGE_GAP)
    {
        rangeLow = LUM_MIN_PERCENT;
        rangeHigh = LUM_MAX_PERCENT;
    }

    for (JsonArray one : doc["points"].as<JsonArray>())
    {
        if (count >= LUM_POINTS) break;
        points_[count].lux = one[0] | 0.0f;
        points_[count].percent = one[1] | rangeLow;
        points_[count].seconds = one[2] | (uint32_t)0;
        count++;
    }

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
    if (value < rangeLow) value = rangeLow;
    if (value > rangeHigh) value = rangeHigh;
    return (uint8_t)value;
}

void Luminance::nudged(uint8_t percent)
{
    if (percent < rangeLow) percent = rangeLow;
    if (percent > rangeHigh) percent = rangeHigh;

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
    waiting = false;
    defaultLine();
    store();
    debugI("Luminance: calibration cleared, back to the default line");
}

bool Luminance::forget(uint8_t index)
{
    if (index >= count) return false;

    // Shifted down rather than swapped with the last one: the order is the
    // order things happened, and that is what decides who leaves next.
    for (uint8_t j = index; j + 1 < count; j++) points_[j] = points_[j + 1];
    count--;

    fit();
    store();
    debugA("Luminance: point %d forgotten, %d left, %.1f%%/decade at %.1f%%",
           index, count, lineSlope, lineOffset);
    return true;
}

uint8_t Luminance::minPercent() { return rangeLow; }
uint8_t Luminance::maxPercent() { return rangeHigh; }

bool Luminance::setRange(uint8_t low, uint8_t high)
{
    if (low < LUM_RANGE_FLOOR || high > LUM_RANGE_CEILING) return false;
    if (high < low + LUM_RANGE_GAP) return false;

    if (low == rangeLow && high == rangeHigh) return true;

    rangeLow = low;
    rangeHigh = high;
    store();
    debugA("Luminance: range is now %d..%d %%", rangeLow, rangeHigh);
    return true;
}

float Luminance::slope()  { return lineSlope; }
float Luminance::offset() { return lineOffset; }
bool Luminance::slopeFitted() { return fittedSlope; }

uint8_t Luminance::points(Point *out, uint8_t max)
{
    uint8_t given = 0;
    for (uint8_t i = 0; i < count && given < max; i++)
    {
        out[given++] = points_[i];
    }
    return given;
}
