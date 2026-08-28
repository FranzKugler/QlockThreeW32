/**
 * ResidualStore
 * What the owner has taught the clock on top of the factory model.
 *
 * The factory grid says what this optical stack does: how much brighter a
 * decade of ambient light is worth, and how much more slider a blue face needs
 * than a green one. What it cannot know is a preference, and a preference is
 * what a nudge on the brightness slider expresses. So a nudge is not stored as
 * "at this lux, this percent" - the grid already answers that - but as the
 * **difference** between what the model asked for and what the person wanted,
 * in decades of emitted light.
 *
 * Decades rather than percentage points, because that is the coordinate the
 * difference is constant in: "a bit brighter" is a different number of slider
 * steps in blue than in green, and stored as steps it would mean something
 * else the next time the colour changed.
 *
 * Three rules carry the whole design, and each of them is a case that a
 * simpler store gets wrong quietly:
 *
 * - **The colour is part of the identity, not decoration beside it.** Two
 *   corrections at the same light in different colours are two statements and
 *   neither replaces the other. That is the failure the old white-only ring
 *   could not even express: an evening in blue silently overwrote an afternoon
 *   in green.
 * - **White has no hue.** At saturation zero the driver emits 255,255,255
 *   whatever the hue byte says, so two whites with different hues beside them
 *   are the same statement and must not fill two of the eight slots.
 * - **A nudge that ran out of slider is a lower bound.** Somebody who drags to
 *   the top said "at least this much"; read as an equality it makes the model
 *   dimmer than the one thing they actually measured, and it does so in bright
 *   rooms, where being too dim is worst.
 *
 * **This file is pure.** No Arduino, no NVS, no logging - so `tests/host/` can
 * compile it with a desktop compiler and ask it in a second the questions a
 * clock would take a month of evenings to answer. Luminance owns the storage
 * and the timing; this owns the arithmetic and the rules.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.2
 * @created  28.8.2026
 * @updated  28.8.2026
 */
#ifndef RESIDUAL_STORE_H
#define RESIDUAL_STORE_H

#include <stdint.h>

// How many corrections are kept. Fewer than the white ring holds, and
// deliberately: the grid already carries the shape of the room-to-brightness
// relation, so what is left to learn is a level and a little colour
// preference, and eight statements are plenty for that.
#define RESIDUAL_MAX 8

// How far a correction reaches, in each of the three directions it can be away
// from where it was made. A triangular weight, zero beyond these, so a
// correction made one evening in deep blue says nothing about a bright
// afternoon in green - which is the failure the old single line had.
#define RESIDUAL_LUX_SPAN 0.9    // decades
#define RESIDUAL_HUE_SPAN 90.0   // degrees, measured the short way round
#define RESIDUAL_SAT_SPAN 60.0   // percentage points

// Near enough in light to be the same statement said again. log10(1.3), the
// same ratio the white ring calls "the same light".
#define RESIDUAL_SAME_LUX 0.11394335230683676

// The most a correction may move the model. Half a decade is a factor of three
// in light and far more than anybody means by "a bit brighter"; beyond it,
// something other than a preference is being expressed - a sensor in the wrong
// place, or a profile measured on another clock - and a model that followed it
// would be hiding the fault it should be showing.
#define RESIDUAL_MAX_DECADES 0.5

namespace ResidualStore
{
    /** One thing the owner said, and how firmly they said it. */
    struct Residual
    {
        double logLux;
        double decades;     // wanted minus what the factory model asked for
        uint16_t hue;       // 0..359, canonicalised: see canonicalHue()
        uint8_t sat;        // 0..100; zero is white and has no hue
        uint32_t seconds;   // uptime when it was said, for the read-out
        /**
         * Whether the slider had anything left to give.
         *
         * 1 when the nudge sat at the top of the regulated range, which makes
         * this "at least `decades`" rather than "exactly". Kept rather than
         * dropped: it is still evidence, it still occupies its colour and its
         * light so a second statement cannot be made there unnoticed, and it
         * is the only evidence there will ever be that the model is too dim in
         * a room the slider cannot reach out of.
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
     * "The same thing" is the same colour *and* the same light. Either one
     * different and it is a new statement. When the store is full the oldest
     * leaves, which is the only ageing this needs - a correction is not less
     * true for being older.
     */
    void add(Store &store, double logLux, double decades, uint16_t hue,
             uint8_t sat, uint32_t seconds, bool bound);

    /**
     * What the stored corrections say about this light in this colour.
     *
     * A weighted mean over everything near enough, rather than the nearest
     * one: corrections are guesses made by eye, and averaging the ones that
     * are close is the same argument the white fit rests on.
     *
     * **A bound can raise the answer and can never lower it.** It says "at
     * least this much", so an exact statement beside it wins whenever the
     * bound is the smaller of the two - and when there is nothing exact at
     * all, acting on it can only make the clock brighter, which is the
     * direction the person asked for.
     *
     * `weight` comes back zero when nothing is near enough to say anything,
     * which is a different answer from "they say nothing changes" and the
     * caller reports it as such.
     */
    double bias(const Store &store, double logLux, uint16_t hue, uint8_t sat,
                double &weight);

    /** Forgets one by its place, keeping the order of the rest. */
    bool forget(Store &store, uint8_t index);
}

#endif
