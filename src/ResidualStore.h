/**
 * ResidualStore
 * Table 2: what the owner has taught the clock, and how it is combined with
 * Table 1 - the factory measurement the shipped nose and blue's line were
 * fitted from - into a model refit rather than a correction bolted on top.
 *
 * **This replaced a bias.** The first version of this file stored the same
 * shape - a colour, a light level, a residual - and answered a locally
 * weighted average of nearby ones, added to whatever the factory nose and
 * blue's line already said. That works, but it never moves the nose or the
 * line themselves: a dozen corrections in cyan taught the clock to argue with
 * its own model at every query rather than teaching the model. What is here
 * instead is closer to what was asked for from the start - the parameters
 * refit, online, from Table 1 and Table 2 together - and it is possible now
 * because the factory model is small enough to refit in a fraction of a
 * second: three numbers for the nose, two for blue's line, not a grid.
 *
 * Three rules carry the whole design, and each is a case a simpler store gets
 * wrong quietly:
 *
 * - **The colour is part of the identity, not decoration beside it.** Two
 *   corrections at the same light in different colours are two statements and
 *   neither replaces the other. That is the failure the old white-only ring
 *   could not even express: an evening in blue silently overwrote an
 *   afternoon in green.
 * - **White has no hue.** At saturation zero the driver emits 255,255,255
 *   whatever the hue byte says, so two whites with different hues beside them
 *   are the same statement and must not fill two of the eight slots - and,
 *   new here, white says nothing about a colour correction at all and takes
 *   no part in refit() on either side of the wheel.
 * - **A nudge that ran out of slider is a lower bound.** Somebody who drags to
 *   the top said "at least this much"; read as an equality by refit() it
 *   would make the model dimmer than the one thing they actually measured,
 *   and it does so in bright rooms, where being too dim is worst. It is still
 *   real evidence for the *other* rule below, though - see refit().
 *
 * **A taught point shadows a factory one that sits inside its sphere,
 * rather than merely outvoting it in an average.** A radius in hue and a
 * ratio in light, both already used elsewhere for "the same statement" (see
 * RESIDUAL_SHADOW_HUE/RESIDUAL_SHADOW_LUX): a factory point inside it is what
 * the owner just said is wrong, at the light and colour they said it at, and
 * keeping it in the fit would have the model average its own contradiction
 * with itself. Outside the sphere Table 1 stands, which is what lets ten
 * evening corrections in one colour still leave a factory profile with
 * something to say about the other five.
 *
 * **This file is no longer independent of FactoryProfile.** The first version
 * was pure in a stronger sense - it knew nothing about the model it corrected,
 * only decades and a colour. Combining Table 1 and Table 2 into one fit needs
 * the shape both are already in (FactoryProfile::Point) and the boundary
 * between the nose and blue's line (Profile::blueHue/blendHalfWidth), so that
 * coupling is now real rather than avoided. It is still pure in the sense that
 * matters for testing: no Arduino, no NVS, no logging - `tests/host/` compiles
 * it and FactoryProfile.cpp together with a desktop compiler.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.4
 * @created  28.8.2026
 * @updated  30.8.2026
 */
#ifndef RESIDUAL_STORE_H
#define RESIDUAL_STORE_H

#include <stdint.h>

#include "FactoryProfile.h"

// How many corrections are kept. Fewer than the white ring holds, and
// deliberately: the factory fit already carries the shape of the room-to-
// brightness relation, so what is left to learn is a level and a little
// colour preference, and eight statements are plenty for that.
#define RESIDUAL_MAX 8

// The sphere a taught point shadows a factory one within: near enough in hue
// and in light that the factory point is a statement about the same thing
// the owner just corrected, not a neighbour left standing to say something
// else. RESIDUAL_SHADOW_LUX is log10(1.3), the same ratio the white ring and
// the "same statement" rule below both already call "the same light".
#define RESIDUAL_SHADOW_HUE 30.0
#define RESIDUAL_SHADOW_LUX 0.11394335230683676

// Near enough in light to be the same statement said again. log10(1.3), the
// same ratio the white ring calls "the same light" - kept as its own name
// from RESIDUAL_SHADOW_LUX even though the value is identical, because the
// two answer different questions (replace this exact statement, against
// shadow that factory point) and a future change to one must not silently
// move the other.
#define RESIDUAL_SAME_LUX 0.11394335230683676

namespace ResidualStore
{
    /** One thing the owner said, and how firmly they said it. */
    struct Residual
    {
        double logLux;
        // Decades, relative to the cone, at saturation 100 - Table 1's own
        // coordinate. Un-faded on the way in (see FactoryProfile::fadeFor())
        // so a point taught at sat 60 sits in the same units as one taught at
        // sat 100 and both can be fitted together.
        double decades;
        uint16_t hue;       // 0..359, canonicalised: see canonicalHue()
        uint8_t sat;        // 0..100; zero is white and has no hue
        uint32_t seconds;   // uptime when it was said, for the read-out
        /**
         * Whether the slider had anything left to give.
         *
         * 1 when the nudge sat at the top of the regulated range, which makes
         * this "at least `decades`" rather than "exactly". Left out of
         * refit()'s regression for the same reason a censored Table 1 point
         * is - reading "at least" as "exactly" pulls the fit towards a value
         * nobody measured - but it still occupies its colour and its light,
         * so a second statement cannot be made there unnoticed, and it still
         * shadows a factory point inside its sphere: "at least this much" is
         * still evidence that the factory number there is wrong.
         *
         * The floor is deliberately **not** treated the same way. The ceiling
         * is what the hardware can do; the floor is a number the owner chose
         * as the dimmest they ever want, so a nudge sitting on it is a
         * preference being met rather than a wish being cut off.
         */
        uint8_t bound;
    };

    /** Oldest first, newest last - which is what decides who leaves. */
    struct Store
    {
        Residual at[RESIDUAL_MAX];
        uint8_t count;
    };

    /** What refit() makes of Table 1 and Table 2 together. */
    struct Fit
    {
        double noseA0, noseA1, noseB1;
        double blueSlope, blueOffset;
    };

    /**
     * The hue a colour is remembered by.
     *
     * White has none: at saturation zero every hue produces the same light, so
     * carrying the hue that happened to be set would make two identical
     * statements look like two different ones.
     */
    uint16_t canonicalHue(uint16_t hue, uint8_t sat);

    /**
     * Whether two colours are the same statement.
     *
     * **Exact**, after canonicalising white, and that is a deliberate choice
     * over a tolerance. A threshold wide enough to call 120 and 150 "the same"
     * is wide enough to lose one of them, and nothing afterwards can tell that
     * it did. The colour comes from a stored setting rather than from a
     * measurement, so it does not drift: the same colour really is the same
     * number, and a person correcting the same face twice is the case the
     * replacement rule exists for.
     */
    bool sameColour(uint16_t hueA, uint8_t satA, uint16_t hueB, uint8_t satB);

    /**
     * Keeps a correction, replacing one that says the same thing.
     *
     * "The same thing" is the same colour *and* the same light (within
     * RESIDUAL_SAME_LUX). Either one different and it is a new statement.
     * When the store is full the oldest leaves, which is the only ageing this
     * needs - a correction is not less true for being older.
     */
    void add(Store &store, double logLux, double decades, uint16_t hue,
             uint8_t sat, uint32_t seconds, bool bound);

    /** Forgets one by its place, keeping the order of the rest. */
    bool forget(Store &store, uint8_t index);

    /**
     * Table 1 and Table 2, refit.
     *
     * Every factory point not shadowed by a taught one, plus every taught
     * point that is not a bound and not white, split by distance from
     * `factory.blueHue` (inside `factory.blendHalfWidth` feeds blue's line,
     * outside feeds the nose) and each half solved by ordinary least squares -
     * the same fit `scripts/build_cone_profile.py` ran once at measurement
     * time, run again here on whatever is left after shadowing.
     *
     * `out` is always written, even on failure: seeded with the factory's own
     * numbers first, so a half that cannot be refit (too few points once the
     * censored and the white are out) is left exactly as the factory shipped
     * it rather than zeroed. Returns false only when the profile itself is
     * not one FactoryProfile::valid() accepts, in which case `out` is
     * untouched and the caller keeps whatever it already had.
     */
    bool refit(const FactoryProfile::Profile &factory, const Store &store, Fit &out);
}

#endif
