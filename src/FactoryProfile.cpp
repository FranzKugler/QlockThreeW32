/**
 * FactoryProfile
 * See FactoryProfile.h for the model. This file is the arithmetic, and it is
 * pure on purpose: `tests/host/` compiles exactly this source with a desktop
 * compiler and runs it against the vectors scripts/factory_luminance.py
 * writes, which is the only place the two implementations can be compared.
 *
 * Everything here is `double` rather than `float`, and that is a decision
 * rather than an oversight. The reference is a Python model working in
 * doubles; the inversion below compares log outputs a few thousandths of a
 * decade apart, and single precision loses the comparison at exactly the dim
 * settings the clock spends its evenings at. It runs once a second.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.2
 * @created  27.8.2026
 * @updated  27.8.2026
 */
#include "FactoryProfile.h"

#include <math.h>

namespace
{
    // Below this, log10 has nothing useful to say and the face is off anyway.
    const double OUTPUT_FLOOR = 1e-12;

    // The stored levels are rounded to ten decimals so the checksum is a
    // statement about the data rather than about the libm that read it.
    // Comparing an unrounded log against them puts 0.02 lx a fraction of a
    // decade *below* the level it is, and the answer comes back clamped at
    // exactly the point it should not be.
    double roundTen(double value)
    {
        double scaled = value * 1e10;
        // Half away from zero, which is what Python's round() does on the
        // values this sees and what the generator wrote the file with.
        return (scaled >= 0.0 ? floor(scaled + 0.5) : ceil(scaled - 0.5)) / 1e10;
    }

    bool isReal(double value)
    {
        return !isnan(value) && !isinf(value);
    }

    // FastLED's own integer scaling. On the ESP32 SCALE8_C is 1 (it is not
    // AVR) and FASTLED_SCALE8_FIXED is 1, which is the `(i * (1 + s)) >> 8`
    // form. scale8_video is deliberately *not* the fixed form - it adds one
    // instead - and mixing the two up moves the floor of every desaturated
    // colour by a count.
    inline uint8_t scale8(uint8_t i, uint8_t scale)
    {
        return (uint8_t)(((uint16_t)i * (uint16_t)(1 + scale)) >> 8);
    }

    inline uint8_t scale8Video(uint8_t i, uint8_t scale)
    {
        return (uint8_t)((((uint16_t)i * (uint16_t)scale) >> 8)
                         + ((i && scale) ? 1 : 0));
    }
}

uint8_t FactoryProfile::hueByte(uint16_t degrees)
{
    if (degrees > 359) degrees = 359;
    return (uint8_t)(((uint32_t)degrees * 255u + 179u) / 359u);
}

uint8_t FactoryProfile::satByte(uint8_t percent)
{
    if (percent > 100) percent = 100;
    return (uint8_t)(((uint32_t)percent * 255u + 50u) / 100u);
}

/**
 * A transcription of FastLED 3.9.15 hsv2rgb_rainbow with Y1=1, Y2=0, G2=0 and
 * Gscale=0 - the library defaults, which is what the firmware links against.
 * Written out rather than calling the library so that the host tests can
 * compile this file alone; the two are checked against each other by
 * tests/golden/fastled_vectors.json on the Python side.
 */
void FactoryProfile::rainbow(uint8_t hue, uint8_t sat, uint8_t *rgb)
{
    const uint8_t K255 = 255, K171 = 171, K170 = 170, K85 = 85;

    uint8_t offset = hue & 0x1F;
    uint8_t offset8 = (uint8_t)(offset << 3);
    uint8_t third = scale8(offset8, 256 / 3);

    uint8_t red, green, blue;
    if (!(hue & 0x80))
    {
        if (!(hue & 0x40))
        {
            if (!(hue & 0x20)) { red = K255 - third; green = third; blue = 0; }
            else               { red = K171; green = (uint8_t)(K85 + third); blue = 0; }
        }
        else
        {
            if (!(hue & 0x20))
            {
                uint8_t twothirds = scale8(offset8, (256 * 2) / 3);
                red = (uint8_t)(K171 - twothirds); green = (uint8_t)(K170 + third); blue = 0;
            }
            else { red = 0; green = (uint8_t)(K255 - third); blue = third; }
        }
    }
    else
    {
        if (!(hue & 0x40))
        {
            if (!(hue & 0x20))
            {
                uint8_t twothirds = scale8(offset8, (256 * 2) / 3);
                red = 0; green = (uint8_t)(K171 - twothirds); blue = (uint8_t)(K85 + twothirds);
            }
            else { red = third; green = 0; blue = (uint8_t)(K255 - third); }
        }
        else
        {
            if (!(hue & 0x20)) { red = (uint8_t)(K85 + third); green = 0; blue = (uint8_t)(K171 - third); }
            else               { red = (uint8_t)(K170 + third); green = 0; blue = (uint8_t)(K85 - third); }
        }
    }

    if (sat != 255)
    {
        if (sat == 0) { red = 255; green = 255; blue = 255; }
        else
        {
            uint8_t desat = scale8Video((uint8_t)(255 - sat), (uint8_t)(255 - sat));
            uint8_t satscale = (uint8_t)(255 - desat);
            red = (uint8_t)(scale8(red, satscale) + desat);
            green = (uint8_t)(scale8(green, satscale) + desat);
            blue = (uint8_t)(scale8(blue, satscale) + desat);
        }
    }

    rgb[0] = red;
    rgb[1] = green;
    rgb[2] = blue;
}

uint8_t FactoryProfile::gammaScale(uint8_t percent)
{
    if (percent == 0) return 0;
    if (percent >= 100) return 255;

    // The driver computes this in float (`lroundf`/`powf`); this computes it
    // in double, because the reference model does. tests/host/ asserts the two
    // agree over the whole range - if a build ever makes them differ, that
    // test says so rather than the clock quietly regulating to a table the
    // profile was not measured against.
    double value = 255.0 * pow(percent / 100.0, 2.2);
    long rounded = (long)floor(value + 0.5);
    if (rounded < 1) rounded = 1;
    if (rounded > 255) rounded = 255;
    return (uint8_t)rounded;
}

uint8_t FactoryProfile::channelDrive(uint8_t component, uint8_t scaled)
{
    return (uint8_t)(((int)component * (int)scaled + 127) / 255);
}

double FactoryProfile::driveResponse(const Profile &profile, int value)
{
    if (value <= 0 || profile.driveCount == 0) return 0.0;
    if (value >= profile.driveLevel[0]) return profile.driveResponse[0];

    for (uint8_t i = 0; i + 1 < profile.driveCount; i++)
    {
        int high = profile.driveLevel[i], low = profile.driveLevel[i + 1];
        if (value <= high && value >= low)
        {
            double span = (double)(high - low);
            if (span <= 0.0) return profile.driveResponse[i];
            return profile.driveResponse[i + 1]
                 + (profile.driveResponse[i] - profile.driveResponse[i + 1])
                   * (value - low) / span;
        }
    }
    // Below the lowest measured level it goes straight down to zero rather
    // than holding flat: the clock spends its evenings down there and a floor
    // would be a lie about the dimmest light it emits.
    uint8_t last = (uint8_t)(profile.driveCount - 1);
    return profile.driveResponse[last] * value / (double)profile.driveLevel[last];
}

double FactoryProfile::relativeOutput(const Profile &profile, uint16_t hue,
                                      uint8_t sat, uint8_t percent)
{
    uint8_t rgb[3];
    rainbow(hueByte(hue), satByte(sat), rgb);
    uint8_t scaled = gammaScale(percent);

    // The firmware's path in the firmware's order: the wheel at full value,
    // the gamma on the setting, the per-channel scaling, and only then the
    // measured drive response. Any other order rounds differently, and at
    // 20 % - a drive of seven - a count is several per cent.
    double total = 0.0;
    for (int i = 0; i < 3; i++)
    {
        total += profile.weight[i] * driveResponse(profile, channelDrive(rgb[i], scaled));
    }
    return total;
}

double FactoryProfile::logOutput(double output)
{
    if (!(output > OUTPUT_FLOOR)) output = OUTPUT_FLOOR;
    return log10(output);
}

double FactoryProfile::worstDip(const Profile &profile)
{
    double worst = 0.0;
    for (uint8_t i = 0; i + 1 < profile.levelCount; i++)
    {
        const Level &low = profile.level[i];
        const Level &high = profile.level[i + 1];

        // The white line on its own first: a dip can sit there with every
        // residual flat, and that is a different fault from one in a hue.
        double fell = low.white - high.white;
        if (fell > worst) worst = fell;

        for (uint8_t k = 0; k < profile.hueCount; k++)
        {
            fell = (low.white + low.residual[k]) - (high.white + high.residual[k]);
            if (fell > worst) worst = fell;
        }
    }
    return worst;
}

bool FactoryProfile::valid(const Profile &profile)
{
    if (profile.levelCount < 2 || profile.levelCount > FACTORY_MAX_LEVELS) return false;
    if (profile.hueCount < 2 || profile.hueCount > FACTORY_MAX_HUES) return false;
    if (profile.driveCount == 0 || profile.driveCount > FACTORY_MAX_DRIVE) return false;
    if (profile.huePeriod == 0) return false;
    if (profile.huePeriod % profile.hueCount) return false;
    if (profile.percentMin >= profile.percentMax) return false;
    if (profile.percentMax > 100) return false;
    // Strictly less, not merely different. A fade that runs backwards is not a
    // fade with a sign: it would make the colour residual whole at saturation
    // zero, where the face is white and there is no colour to correct, and
    // nothing at all at full saturation, where the whole correction lives.
    // Refused rather than flipped - a file silently reinterpreted is a file
    // nobody can reason about afterwards.
    if (profile.satZero >= profile.satFull) return false;
    // And both ends are saturations. A `satFull` above a hundred makes the
    // fade divide by a span the face can never reach, so every colour the
    // clock can actually show gets a fraction of its measured correction -
    // wrong everywhere, and wrong quietly. Only the top is compared: satZero
    // is strictly below it, so this bounds both.
    if (profile.satFull > 100) return false;

    // Evenly spaced knots, because the segment arithmetic below assumes it.
    uint16_t span = (uint16_t)(profile.huePeriod / profile.hueCount);
    for (uint8_t i = 0; i < profile.hueCount; i++)
    {
        if (profile.hueKnot[i] != (uint16_t)(i * span)) return false;
    }

    for (uint8_t i = 0; i < profile.levelCount; i++)
    {
        const Level &level = profile.level[i];
        if (!isReal(level.logLux) || !isReal(level.white)) return false;
        // Strictly ascending: the bracket search walks the pairs and divides
        // by the span, so a repeated or descending level answers something
        // plausible out of the wrong bracket.
        if (i && !(level.logLux > profile.level[i - 1].logLux)) return false;
        // A flag, and there is exactly one kind of it: "at least this much",
        // from an observation that ran out of slider. Anything other than 0 or
        // 1 is truthy, so every answer touching that corner would report a
        // bound - which the read-out shows and the user layer acts on - with
        // nothing behind it. Only the knots in this grid are looked at: the
        // array is sized for the largest profile the firmware can hold, and
        // what sits beyond hueCount is not part of the measurement.
        if (level.censored > 1) return false;
        for (uint8_t k = 0; k < profile.hueCount; k++)
        {
            if (!isReal(level.residual[k])) return false;
            if (level.bound[k] > 1) return false;
        }
    }

    // The drive table, as Coupling checks it: strictly descending from a top
    // level of 1..255, nothing negative, and a bottom level that can divide.
    if (profile.driveLevel[0] == 0) return false;
    for (uint8_t i = 0; i + 1 < profile.driveCount; i++)
    {
        if (profile.driveLevel[i + 1] >= profile.driveLevel[i]) return false;
    }
    if (profile.driveLevel[profile.driveCount - 1] == 0) return false;
    if (!(profile.driveResponse[0] > 0.0)) return false;
    for (uint8_t i = 0; i < profile.driveCount; i++)
    {
        if (!isReal(profile.driveResponse[i]) || profile.driveResponse[i] < 0.0) return false;
    }

    double sum = 0.0;
    for (int i = 0; i < 3; i++)
    {
        if (!isReal(profile.weight[i]) || profile.weight[i] < 0.0) return false;
        sum += profile.weight[i];
    }
    if (!(sum > 0.0)) return false;

    // Last, because it is the only check that needs every number already known
    // to be a number. Measured on the grid rather than read out of a status
    // field: see worstDip().
    return worstDip(profile) <= FACTORY_MAX_DIP;
}

bool FactoryProfile::evaluate(const Profile &profile, double lux, uint16_t hue,
                              uint8_t sat, Answer &out)
{
    return evaluateWith(profile, lux, hue, sat, 0.0, out);
}

bool FactoryProfile::evaluateWith(const Profile &profile, double lux,
                                  uint16_t hue, uint8_t sat, double bias,
                                  Answer &out)
{
    if (!valid(profile)) return false;
    if (isnan(bias) || isinf(bias)) return false;
    if (!(lux > 0.0) || !isReal(lux)) return false;
    if (hue >= profile.huePeriod) hue = (uint16_t)(hue % profile.huePeriod);
    if (sat > 100) sat = 100;

    double logLux = roundTen(log10(lux));

    // Which two levels this ambient sits between. Clamped rather than
    // extrapolated: a straight line run past the last measurement is a claim
    // nobody made, and this grid is short of exactly the conditions its ends
    // are at.
    uint8_t low = 0, high = 0;
    double along = 0.0;
    Clamped clamped = CLAMP_NONE;
    if (logLux <= profile.level[0].logLux)
    {
        low = high = 0;
        if (logLux < profile.level[0].logLux) clamped = CLAMP_BELOW;
    }
    else if (logLux >= profile.level[profile.levelCount - 1].logLux)
    {
        low = high = (uint8_t)(profile.levelCount - 1);
        if (logLux > profile.level[low].logLux) clamped = CLAMP_ABOVE;
    }
    else
    {
        for (uint8_t i = 0; i + 1 < profile.levelCount; i++)
        {
            if (logLux >= profile.level[i].logLux && logLux <= profile.level[i + 1].logLux)
            {
                low = i;
                high = (uint8_t)(i + 1);
                along = (logLux - profile.level[i].logLux)
                      / (profile.level[i + 1].logLux - profile.level[i].logLux);
                break;
            }
        }
    }

    const Level &lowLevel = profile.level[low];
    const Level &highLevel = profile.level[high];
    double white = lowLevel.white + (highLevel.white - lowLevel.white) * along;

    // Cyclic in hue: 330 sits half way from the last knot back to the first,
    // over the wrap. That segment has no knot at its far end in the stored
    // order, which is exactly why it is the one that gets written wrong.
    uint16_t hueSpan = (uint16_t)(profile.huePeriod / profile.hueCount);
    double position = (double)(hue % profile.huePeriod) / (double)hueSpan;
    uint8_t lowHue = (uint8_t)(((int)floor(position)) % profile.hueCount);
    uint8_t highHue = (uint8_t)((lowHue + 1) % profile.hueCount);
    double fraction = position - floor(position);

    // A corner only taints the answer if it is actually being read: at
    // fraction 0 the far knot contributes nothing, and saying "at least" on
    // its account would report a dependency that is not there.
    double residualLow = lowLevel.residual[lowHue]
        + (lowLevel.residual[highHue] - lowLevel.residual[lowHue]) * fraction;
    bool boundLow = (fraction < 1.0 && lowLevel.bound[lowHue])
                 || (fraction > 0.0 && lowLevel.bound[highHue]);
    double residualHigh = highLevel.residual[lowHue]
        + (highLevel.residual[highHue] - highLevel.residual[lowHue]) * fraction;
    bool boundHigh = (fraction < 1.0 && highLevel.bound[lowHue])
                  || (fraction > 0.0 && highLevel.bound[highHue]);

    double residual = residualLow + (residualHigh - residualLow) * along;
    bool bound = (along < 1.0 && boundLow) || (along > 0.0 && boundHigh);

    // The fade, declared in the profile and applied here: exactly zero at
    // sat=0 - which is what makes white the same answer whatever hue is stored
    // beside it - and exactly the whole residual at sat=100.
    double fade;
    if (sat <= profile.satZero) fade = 0.0;
    else if (sat >= profile.satFull) fade = 1.0;
    else fade = (double)(sat - profile.satZero)
              / (double)(profile.satFull - profile.satZero);
    residual *= fade;
    if (fade == 0.0)
    {
        // Nothing of the colour grid reached the answer, so nothing about the
        // colour grid limits it.
        residual = 0.0;
        bound = false;
    }

    // What the stack does, plus what this person prefers. Added in decades,
    // before the inversion, so the correction survives a change of colour.
    double target = white + residual + bias;

    // The inverse: a bounded search over integer percentages, ties to the
    // lowest. Several percentages produce the same drive at the bottom of the
    // curve - 1, 2 and 3 all come out as a drive of one - and when they are
    // indistinguishable in light the dimmest is the honest answer.
    uint8_t bestPercent = profile.percentMin;
    double bestZ = logOutput(relativeOutput(profile, hue, sat, profile.percentMin));
    double bestGap = fabs(bestZ - target);
    double lowest = bestZ, highest = bestZ;

    for (int percent = profile.percentMin + 1; percent <= profile.percentMax; percent++)
    {
        double z = logOutput(relativeOutput(profile, hue, sat, (uint8_t)percent));
        if (z > highest) highest = z;
        if (z < lowest) lowest = z;
        double gap = fabs(z - target);
        if (gap < bestGap)
        {
            bestPercent = (uint8_t)percent;
            bestZ = z;
            bestGap = gap;
        }
    }

    out.percent = bestPercent;
    out.target = target;
    out.white = white;
    out.residual = residual;
    out.achieved = bestZ;
    out.logLux = logLux;
    out.limited = (target > highest) ? LIMITED_CEILING
                : (target < lowest) ? LIMITED_FLOOR : LIMITED_NONE;
    out.clamped = clamped;
    out.bound = bound;
    return true;
}
