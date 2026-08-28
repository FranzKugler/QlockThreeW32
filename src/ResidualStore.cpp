/**
 * ResidualStore
 * See ResidualStore.h for the three rules and why each of them exists.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.2
 * @created  28.8.2026
 * @updated  28.8.2026
 */
#include "ResidualStore.h"

#include <math.h>

namespace
{
    /** Degrees between two hues, measured the short way round the wheel. */
    double hueDistance(uint16_t a, uint16_t b)
    {
        double gap = fabs((double)a - (double)b);
        if (gap > 180.0) gap = 360.0 - gap;
        return gap;
    }

    /** A triangular weight: one at the point, zero at the span and beyond. */
    double taper(double distance, double span)
    {
        if (span <= 0.0) return 0.0;
        double weight = 1.0 - fabs(distance) / span;
        return weight > 0.0 ? weight : 0.0;
    }

    /**
     * How much one stored correction has to say about this light and colour.
     *
     * The hue term is skipped when either side is white, and that is not a
     * shortcut: white has no hue, so a hue distance measured against it would
     * be measuring the setting that happens to be stored beside it rather than
     * anything the face was showing. What separates white from a colour is the
     * saturation term, which does it properly - zero against a hundred is
     * beyond RESIDUAL_SAT_SPAN, so neither leaks into the other.
     */
    double weightOf(const ResidualStore::Residual &one, double logLux,
                    uint16_t hue, uint8_t sat)
    {
        double weight = taper(logLux - one.logLux, RESIDUAL_LUX_SPAN)
                      * taper((double)sat - (double)one.sat, RESIDUAL_SAT_SPAN);
        if (weight <= 0.0) return 0.0;
        if (one.sat != 0 && sat != 0)
        {
            weight *= taper(hueDistance(hue, one.hue), RESIDUAL_HUE_SPAN);
        }
        return weight;
    }

    double clampToPreference(double decades)
    {
        if (decades > RESIDUAL_MAX_DECADES) return RESIDUAL_MAX_DECADES;
        if (decades < -RESIDUAL_MAX_DECADES) return -RESIDUAL_MAX_DECADES;
        return decades;
    }
}

uint16_t ResidualStore::canonicalHue(uint16_t hue, uint8_t sat)
{
    // Zero rather than the hue that happened to be set. At saturation zero the
    // driver emits 255,255,255 whatever the hue byte says, so keeping it would
    // make two identical statements look like two different ones - and fill
    // two of eight slots with the same fact.
    if (sat == 0) return 0;
    return (uint16_t)(hue % 360);
}

bool ResidualStore::sameColour(uint16_t hueA, uint8_t satA,
                               uint16_t hueB, uint8_t satB)
{
    if (satA != satB) return false;
    return canonicalHue(hueA, satA) == canonicalHue(hueB, satB);
}

void ResidualStore::add(Store &store, double logLux, double decades,
                        uint16_t hue, uint8_t sat, uint32_t seconds, bool bound)
{
    uint16_t canonical = canonicalHue(hue, sat);

    for (uint8_t i = 0; i < store.count; i++)
    {
        // The same colour *and* the same light. Either one different and this
        // is a new statement rather than an update of an old one.
        if (!sameColour(store.at[i].hue, store.at[i].sat, canonical, sat)) continue;
        if (fabs(logLux - store.at[i].logLux) > RESIDUAL_SAME_LUX) continue;

        // Taken out rather than overwritten in place, so what goes in below is
        // always the newest and the stored order stays the order things
        // happened - which is what decides who leaves when the store is full.
        for (uint8_t j = i; j + 1 < store.count; j++) store.at[j] = store.at[j + 1];
        store.count--;
        break;   // at most one; two would have replaced each other already
    }

    if (store.count == RESIDUAL_MAX)
    {
        for (uint8_t j = 0; j + 1 < store.count; j++) store.at[j] = store.at[j + 1];
        store.count--;
    }

    store.at[store.count].logLux = logLux;
    store.at[store.count].decades = clampToPreference(decades);
    store.at[store.count].hue = canonical;
    store.at[store.count].sat = sat;
    store.at[store.count].seconds = seconds;
    store.at[store.count].bound = bound ? 1 : 0;
    store.count++;
}

double ResidualStore::bias(const Store &store, double logLux, uint16_t hue,
                           uint8_t sat, double &weight)
{
    // The two kinds are summed apart, because they are different statements
    // and only one of them may lower the answer.
    double exactTop = 0.0, exactBottom = 0.0;
    double boundTop = 0.0, boundBottom = 0.0;

    for (uint8_t i = 0; i < store.count; i++)
    {
        double w = weightOf(store.at[i], logLux, hue, sat);
        if (w <= 0.0) continue;
        if (store.at[i].bound)
        {
            boundTop += w * store.at[i].decades;
            boundBottom += w;
        }
        else
        {
            exactTop += w * store.at[i].decades;
            exactBottom += w;
        }
    }

    weight = exactBottom + boundBottom;
    if (weight <= 0.0) return 0.0;

    if (exactBottom <= 0.0)
    {
        // Nothing exact has ever been said here. "At least this much" is still
        // evidence, and acting on it can only make the clock brighter - which
        // is the direction the person asked for.
        return clampToPreference(boundTop / boundBottom);
    }

    double found = exactTop / exactBottom;
    if (boundBottom > 0.0)
    {
        // A lower bound raises the answer or does nothing at all. Averaging it
        // in would let "at least a little" pull down "exactly a lot", which is
        // reading a weaker statement as a contradiction.
        double bounded = boundTop / boundBottom;
        if (bounded > found) found = bounded;
    }
    return clampToPreference(found);
}

bool ResidualStore::forget(Store &store, uint8_t index)
{
    if (index >= store.count) return false;
    for (uint8_t j = index; j + 1 < store.count; j++) store.at[j] = store.at[j + 1];
    store.count--;
    return true;
}
