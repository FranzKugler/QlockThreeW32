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
#include "FactoryLuminance.h"
#include "ResidualStore.h"
#include "LightSensor.h"   // LUX_FLOOR
#include "LogBuffer.h"

// Its own namespace, next to the settings' "qlock" and expert mode's
// "qlockexpert". One JSON string under one key, the same shape the settings
// use: NVS keys are capped at 15 characters and a dozen of them for one line
// and ten points would be a filing system rather than a record.
#define LUM_NAMESPACE "qlocklight"
#define LUM_KEY       "curve"
// The corrections on top of the factory model, under a key of their own beside
// the white curve rather than inside it. They are written on a different
// occasion, they survive a different set of resets, and an older firmware
// reading this namespace finds its own record untouched.
#define LUM_USER_KEY  "user"

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

    // Which points the last fit actually used. The read-out shows it, because
    // a point that is stored, visible, and silently ignored is worse than one
    // that is not stored at all.
    bool used_[LUM_POINTS];

    uint8_t rangeLow = LUM_MIN_PERCENT;
    uint8_t rangeHigh = LUM_MAX_PERCENT;

    // The nudge in progress, if any.
    bool waiting = false;
    uint8_t wanted = 0;
    uint16_t wantedHue = LUM_HUE_UNKNOWN;
    uint8_t wantedSat = 0;
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

        // A point at the top of the range is **censored**: the person wanted
        // "at least this much" and the slider had nothing more to offer. Least
        // squares reads it as "exactly this much", so in a bright room where
        // the honest answer would have been 150 % it records 100 and pulls the
        // bright end of the line down - which flattens the slope and makes the
        // clock too dim everywhere else. Left out of the fit rather than
        // stored differently: it is still what somebody said, it still stops a
        // second point being taught at the same light, and the read-out still
        // shows it.
        //
        // The floor is *not* treated the same way, and the asymmetry is real
        // rather than an oversight. The ceiling is what the hardware can do;
        // the floor is a number the owner chose on this very screen as the
        // dimmest they ever want, so a point sitting on it is a preference
        // being met, not a wish being cut off.
        uint8_t usable = 0;
        for (uint8_t i = 0; i < count; i++)
        {
            used_[i] = points_[i].percent < rangeHigh;
            if (used_[i]) usable++;
        }

        // Unless leaving them out leaves nothing to fit. Two points at the
        // ceiling and nothing else is a poor line, and no line at all is
        // worse.
        if (usable < 2)
        {
            for (uint8_t i = 0; i < count; i++) used_[i] = true;
            usable = count;
        }

        float sumX = 0.0f, sumY = 0.0f, lowest = 0.0f, highest = 0.0f;
        bool first = true;
        for (uint8_t i = 0; i < count; i++)
        {
            if (!used_[i]) continue;
            float x = logLux(points_[i].lux);
            if (first || x < lowest) lowest = x;
            if (first || x > highest) highest = x;
            first = false;
            sumX += x;
            sumY += (float)points_[i].percent;
        }
        float meanX = sumX / usable;
        float meanY = sumY / usable;

        bool canFitSlope = (highest - lowest) >= LUM_FIT_MIN_DECADES;
        if (canFitSlope)
        {
            float top = 0.0f, bottom = 0.0f;
            for (uint8_t i = 0; i < count; i++)
            {
                if (!used_[i]) continue;
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
            // Appended rather than given names: an older firmware reading this
            // record takes the first three and ignores the rest, and a newer
            // one reading an older record finds them missing and says so.
            one.add(points_[i].hue);
            one.add(points_[i].sat);
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
    void remember(float lux, uint8_t percent, uint32_t seconds,
                  uint16_t hue, uint8_t sat)
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
        points_[count].hue = hue;
        points_[count].sat = sat;
        count++;
    }

    // ------------------------------------------------------------------
    // The user layer: corrections on top of the factory model
    //
    // The white ring above is what a clock learns when it has no model of its
    // own optics. With a factory profile there *is* one, and it already
    // carries the shape - how much brighter a decade of ambient light is
    // worth, and how much more slider a blue face needs than a green one. What
    // is left to learn is a preference, and a preference is small.
    //
    // So a nudge is not stored as "at this lux, this percent". It is stored as
    // the difference, in decades of emitted light, between what the person
    // wanted and what the model asked for - keyed by the light *and the
    // colour* it was said in. Two nudges at the same lux in different colours
    // are two statements and neither replaces the other. That is the whole
    // point: the old ring could not tell them apart, so an evening in blue
    // silently overwrote an afternoon in green.
    // ------------------------------------------------------------------

    // The corrections themselves live in ResidualStore, which is pure and
    // therefore testable: every rule about which of two statements survives is
    // there, checked by tests/host/test_residual_store.cpp. What is left here
    // is the storage and the timing.
    ResidualStore::Store user_;

    /*
     * Whether there is a stored record of corrections that were learned on a
     * *different* profile from the one now installed.
     *
     * The fact belongs here rather than in FactoryLuminance, and the
     * difference matters: the NVS key FactoryLuminance keeps is "which
     * profile is this clock on", written once, while this is "were the
     * corrections in hand said about it" - and on a clock that has never
     * corrected anything the honest answer is "nothing disagrees", not "no".
     * Read the other way round, a brand new clock shows a warning about a
     * mismatch it does not have.
     */
    bool userStale = false;

    void storeUser()
    {
        JsonDocument doc;
        // Which profile these were learned against. A correction measured on
        // one baseline means nothing on another, and a filesystem update can
        // bring a new measurement underneath them.
        doc["profile"] = FactoryLuminance::sourceChecksum();
        JsonArray list = doc["residuals"].to<JsonArray>();
        for (uint8_t i = 0; i < user_.count; i++)
        {
            JsonArray one = list.add<JsonArray>();
            one.add(user_.at[i].logLux);
            one.add(user_.at[i].decades);
            one.add(user_.at[i].hue);
            one.add(user_.at[i].sat);
            one.add(user_.at[i].seconds);
            // Appended, so a record written before the bound was represented
            // reads as exact - which is what such a clock believed it was.
            one.add(user_.at[i].bound);
        }

        String out;
        serializeJson(doc, out);

        Preferences preferences;
        if (!preferences.begin(LUM_NAMESPACE, false))
        {
            debugE("Luminance: cannot open NVS for the corrections");
            return;
        }
        if (preferences.getString(LUM_USER_KEY, "") != out)
        {
            preferences.putString(LUM_USER_KEY, out);
        }
        preferences.end();
    }

    void loadUser()
    {
        user_.count = 0;
        userStale = false;

        Preferences preferences;
        if (!preferences.begin(LUM_NAMESPACE, true)) return;
        String stored = preferences.getString(LUM_USER_KEY, "");
        preferences.end();
        if (stored.length() == 0) return;

        JsonDocument doc;
        if (deserializeJson(doc, stored) != DeserializationError::Ok)
        {
            debugE("Luminance: the stored corrections are not readable");
            return;
        }

        // Learned against a different baseline. Kept in NVS rather than
        // deleted - they are still what somebody said - but not applied, since
        // adding them to a grid they were not measured on is arithmetic across
        // two models. The read-out says so and the factory restore clears them.
        String against = doc["profile"] | "";
        if (FactoryLuminance::available()
            && against != FactoryLuminance::sourceChecksum())
        {
            userStale = true;
            debugW("Luminance: %d corrections were learned on another profile "
                   "and are not being applied", (int)doc["residuals"].size());
            return;
        }

        for (JsonArray one : doc["residuals"].as<JsonArray>())
        {
            if (user_.count >= RESIDUAL_MAX) break;

            uint16_t hue = one[2] | (uint16_t)LUM_HUE_UNKNOWN;
            uint8_t sat = one[3] | (uint8_t)0;
            // A correction with no colour cannot be placed on a colour-aware
            // grid, and guessing one would put it at hue 0, which is red. It
            // belongs to the legacy white line and stays there.
            if (hue == LUM_HUE_UNKNOWN) continue;

            ResidualStore::Residual &into = user_.at[user_.count++];
            into.logLux = one[0] | 0.0;
            into.decades = one[1] | 0.0;
            // Canonicalised on the way in as well as on the way out, so a
            // record written before white lost its hue reads as one white
            // rather than as several.
            into.hue = ResidualStore::canonicalHue(hue, sat);
            into.sat = sat;
            into.seconds = one[4] | (uint32_t)0;
            into.bound = (one[5] | 0) ? 1 : 0;
        }
    }

    /** True when the factory model is the one answering. */
    bool factoryRuns(uint16_t hue)
    {
        return FactoryLuminance::available() && hue != LUM_HUE_UNKNOWN;
    }
}

/**
 * Reads the legacy white curve, if there is one.
 *
 * Its own function, and that is the fix for a real bug rather than tidiness.
 * This used to be the body of begin() with three early returns in it - no NVS
 * namespace, no stored key, unreadable JSON - and loadUser() sat *after* them.
 * On a clock that has only ever known the factory model there is no `curve`
 * key at all, so the second return fired, the corrections were never read
 * back, and everything the owner had taught the clock vanished at every
 * reboot while the read-out went on showing an empty list as though nothing
 * had been said.
 *
 * The two records are independent and are now read independently.
 */
static void loadCurve()
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
        points_[count].hue = one[3] | (uint16_t)LUM_HUE_UNKNOWN;
        points_[count].sat = one[4] | (uint8_t)0;
        count++;
    }

    // Re-fitted rather than trusted: the stored coefficients are there so the
    // read-out and a future tool can see them, but the points are the record
    // and the line is derived. If the two ever disagree the points win.
    fit();
}

/**
 * Both records, unconditionally.
 *
 * There is deliberately no `return` in this function. The two stores are
 * independent - a clock can have corrections and no white curve, or a white
 * curve and no corrections - and every early exit added here is a way for one
 * of them to be silently skipped because the *other* one is missing. That is
 * exactly the bug loadCurve() was split out of.
 */
void Luminance::begin()
{
    loadCurve();
    loadUser();

    debugI("Luminance: %d points, %.1f%%/decade at %.1f%%, slope %s, "
           "%d colour corrections on %s",
           count, lineSlope, lineOffset, fittedSlope ? "fitted" : "kept",
           user_.count,
           FactoryLuminance::available() ? FactoryLuminance::profileId()
                                         : "no factory profile");
}

uint8_t Luminance::forLux(float lux)
{
    float percent = lineSlope * logLux(lux) + lineOffset;

    long value = lroundf(percent);
    if (value < rangeLow) value = rangeLow;
    if (value > rangeHigh) value = rangeHigh;
    return (uint8_t)value;
}

void Luminance::nudged(uint8_t percent, uint16_t hue, uint8_t sat)
{
    if (percent < rangeLow) percent = rangeLow;
    if (percent > rangeHigh) percent = rangeHigh;

    wanted = percent;
    wantedHue = hue;
    wantedSat = sat;
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
    uint32_t seconds = (uint32_t)(millis() / 1000);

    // With a factory profile, what is learned is a *correction to it*, not a
    // point on a line of its own. The grid already says what this stack does
    // at this light in this colour; the person is saying it should be a little
    // more or a little less, and that difference is what generalises.
    if (factoryRuns(wantedHue))
    {
        FactoryProfile::Answer asked;
        if (FactoryLuminance::evaluate(lux, wantedHue, wantedSat, asked))
        {
            // In decades of emitted light, which is the coordinate the
            // difference is constant in. Expressed in percent it would mean
            // something else the next time the colour changed.
            double got = FactoryProfile::logOutput(FactoryProfile::relativeOutput(
                FactoryLuminance::profile(), wantedHue, wantedSat, wanted));
            // A nudge sitting at the top of the regulated range is a lower
            // bound: the slider had nothing above it to offer, so what was
            // said is "at least this much". Kept as such rather than as an
            // equality - read as one it would make the model dimmer than the
            // only thing anybody measured, and it would do so in bright rooms,
            // where being too dim is worst. The floor is deliberately not
            // treated the same way; see ResidualStore::Residual::bound.
            bool censored = (wanted >= rangeHigh);

            ResidualStore::add(user_, logLux(lux), got - asked.target,
                               wantedHue, wantedSat, seconds, censored);
            storeUser();

            debugI("Luminance: %.2f lx hue %d sat %d - wanted %d%%, the model "
                   "asked %d%%, keeping %+.3f decades%s (%d corrections)",
                   lux, wantedHue, wantedSat, wanted, asked.percent,
                   got - asked.target, censored ? " as a lower bound" : "",
                   user_.count);
            return true;
        }
    }

    // No profile, or a nudge made in a colour nobody recorded. The white line,
    // exactly as before.
    remember(lux, wanted, seconds, wantedHue, wantedSat);
    fit();
    store();

    debugI("Luminance: learned %.2f lx -> %d%% (hue %d sat %d), now %d points, "
           "%.1f%%/decade at %.1f%% (slope %s)",
           lux, wanted, wantedHue, wantedSat, count, lineSlope, lineOffset,
           fittedSlope ? "fitted" : "kept");
    return true;
}

void Luminance::targetFor(float lux, uint16_t hue, uint8_t sat, Target &out)
{
    out.percent = forLux(lux);
    out.source = SOURCE_LEGACY;
    out.factory = out.percent;
    out.bias = 0.0f;
    out.limited = out.bound = out.clamped = false;

    if (!factoryRuns(hue)) return;

    // What the grid alone asks for, kept separately - the read-out shows both,
    // because "the model says 42 and you have taught it 47" is the sentence
    // somebody needs when the automatic feels wrong.
    FactoryProfile::Answer plain;
    if (!FactoryLuminance::evaluate(lux, hue, sat, plain)) return;

    double weight = 0.0;
    double bias = ResidualStore::bias(user_, logLux(lux), hue, sat, weight);

    FactoryProfile::Answer answer = plain;
    if (weight > 0.0
        && !FactoryProfile::evaluateWith(FactoryLuminance::profile(), (double)lux,
                                         hue, sat, bias, answer))
    {
        answer = plain;
        bias = 0.0;
        weight = 0.0;
    }

    // Clamped to the regulated range the owner set on this very screen, which
    // is not the same as the range the profile was measured over.
    long value = answer.percent;
    if (value < rangeLow) value = rangeLow;
    if (value > rangeHigh) value = rangeHigh;

    out.percent = (uint8_t)value;
    out.factory = plain.percent;
    out.bias = (weight > 0.0) ? (float)bias : 0.0f;
    out.source = (weight > 0.0) ? SOURCE_FACTORY_USER : SOURCE_FACTORY;
    out.limited = (answer.limited != FactoryProfile::LIMITED_NONE);
    out.bound = answer.bound;
    out.clamped = (answer.clamped != FactoryProfile::CLAMP_NONE);
}

bool Luminance::residualsStale() { return userStale; }

uint8_t Luminance::residuals(Residual *out, uint8_t max)
{
    uint8_t given = 0;
    for (uint8_t i = 0; i < user_.count && given < max; i++) out[given++] = user_.at[i];
    return given;
}

bool Luminance::forgetResidual(uint8_t index)
{
    if (!ResidualStore::forget(user_, index)) return false;
    storeUser();
    debugA("Luminance: correction %d forgotten, %d left", index, user_.count);
    return true;
}

bool Luminance::factoryRestore()
{
    // Checked before anything is written. A restore that failed half way
    // would leave the clock between two models, which is the one state
    // nothing here can describe.
    if (!FactoryLuminance::available()) return false;

    user_.count = 0;
    userStale = false;
    waiting = false;
    storeUser();

    // The white points are the same preferences said in the old coordinates.
    // Left behind, they would make a restored clock read as restored only
    // until the day its profile is refused.
    count = 0;
    defaultLine();
    store();

    // And a note of which profile this clock is now on. Coupling is untouched:
    // it is a measurement of this clock's own optics, not of anybody's taste.
    FactoryLuminance::record();

    debugA("Luminance: back to the factory profile %s, corrections cleared, "
           "the coupling measurement kept", FactoryLuminance::profileId());
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

bool Luminance::usedInFit(uint8_t index)
{
    return index < count ? used_[index] : false;
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

    // Which points count as censored depends on where the ceiling is, so the
    // line has to be worked out again rather than only re-stored. Lowering the
    // ceiling takes points out of the fit; raising it brings them back.
    fit();
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
