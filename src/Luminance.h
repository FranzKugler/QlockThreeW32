/**
 * Luminance
 * The automatic brightness curve, and how it learns.
 *
 * The curve is a straight line in log light:
 *
 *     brightness = slope * log10(lux) + offset    clamped to the range below
 *
 * Log, because perception is roughly logarithmic and the range to cover spans
 * decades - a dark bedroom and a sunlit room differ by a factor of thousands,
 * which no straight line in plain lux survives.
 *
 * **There is no "remember this" button, and that is the design.** Nudging the
 * brightness is the only signal a user ever gives about whether the automatic
 * got it right, so that nudge *is* the calibration. While the automatic is on
 * and the slider moves, the automatic steps aside; SETTLE_MS after the last
 * move, the pair (light now, brightness asked for) is kept and the line is
 * fitted again through everything kept so far. The switch keeps saying
 * "automatic" throughout, because it still is - it is being taught, not
 * turned off.
 *
 * The line is a plain least-squares fit through every point, both halves,
 * every point weighted the same. Averaging out the person is what it is for:
 * somebody setting the brightness by eye guesses, and guesses differently each
 * time, so ten statements about a room are worth more than the last one. Age
 * is not a weight - a point is not less true for being older - and the only
 * ageing is the ring: an eleventh point pushes the first one out.
 *
 * Five things keep the fit from going somewhere silly:
 *
 * - **Too little spread, and only the offset moves.** Ten corrections all made
 *   in the same evening light say nothing about steepness; a least-squares fit
 *   through them is noise multiplied by a very large number. Below
 *   FIT_MIN_DECADES of spread the slope is left alone and only the level is
 *   re-fitted - which is exactly what the old "shift the whole curve" did. That
 *   was never wrong, it was just done always instead of only when it is all one
 *   can honestly do.
 * - **A slope of zero or less is refused** the same way. Darker room, brighter
 *   clock is not a thing anybody wants, and one careless nudge in daylight can
 *   produce it.
 * - **A new point replaces a near neighbour** instead of joining the queue, so
 *   ten evening corrections cannot push the one daylight point out of the ring
 *   and collapse the line onto a single lighting condition.
 * - **A point at the top of the range is left out of the fit.** It is
 *   censored: the slider had nothing more to offer, so "100 %" means "at
 *   least 100 %", and read as an equality it drags the bright end of the line
 *   down and flattens the slope. The floor is not treated the same way - it is
 *   a number the owner chose as the dimmest they ever want, so a point on it
 *   is a preference met rather than a wish cut off.
 * - **The output is clamped to the regulated range.** Below a fifth the face is not really
 *   readable, and zero is the display switching itself off - a mode chosen in
 *   the display tab, never something the light sensor gets to decide.
 *
 * Stored in NVS in a namespace of its own, next to the settings rather than in
 * them: a settings record is rewritten whole on every change, and this is
 * written from a timer on a completely different schedule.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  22.8.2026
 * @updated  22.8.2026
 */
#ifndef LUMINANCE_H
#define LUMINANCE_H

#include <Arduino.h>

#include "ResidualStore.h"

// How many calibration points are kept. Ten is enough to describe a home - a
// few daylight levels, a lamp, a dark room - and small enough that a bad one
// is forgotten within a week of ordinary use.
#define LUM_POINTS 10

// Where the regulated range starts out. Not where it stays: both ends are
// stored with the curve and settable from the brightness screen, because how
// dim a face is still readable depends on the panel in front of it, how far
// away it is read from, and whose eyes are reading it. Twenty was chosen on
// one clock and there is no reason it should bind every other one.
//
// Zero is deliberately not reachable. The display switching itself off is a
// mode, chosen in the display tab, and never something the light sensor gets
// to decide.
#define LUM_MIN_PERCENT 20
#define LUM_MAX_PERCENT 100

// What the range may be set to. The floor is 1 rather than 0 for the reason
// above; the gap keeps the two ends from meeting, which would make the curve a
// constant and the whole screen a lie.
#define LUM_RANGE_FLOOR 1
#define LUM_RANGE_CEILING 100
#define LUM_RANGE_GAP 5

// Silence after a slider move before the pair is kept. Long enough that
// dragging counts as one adjustment rather than fifty.
#define LUM_SETTLE_MS 10000

// A new point this close to an old one in lux replaces it. 1.3 is well inside
// what the eye calls "the same light" and well outside sensor noise.
#define LUM_SAME_LIGHT_RATIO 1.3f

// Decades of spread the points must cover before a slope is fitted at all.
// 0.6 is a factor of four, two doublings - the least that says anything.
#define LUM_FIT_MIN_DECADES 0.6f

// A point from a record written before the colour was kept. Not zero: hue 0 is
// red, and a point silently claiming to have been taught in red would be worse
// than one that admits it does not know.
#define LUM_HUE_UNKNOWN 0xFFFF

// The line a clock starts with, and returns to on reset. Deliberately cautious
// rather than good: it assumes a sensor in the open, and behind a front panel
// both readings shrink by the same factor - which in log space only shifts the
// line sideways, so an uncalibrated clock still dims in the right direction,
// just not by the right amount. Better numbers when there is more experience.
#define LUM_DEFAULT_LOW_LUX     0.3f
#define LUM_DEFAULT_LOW_PERCENT 20
#define LUM_DEFAULT_HIGH_LUX    9.0f
#define LUM_DEFAULT_HIGH_PERCENT 100

// The corrections on top of the factory model - how many are kept, how far
// each one reaches, and how much any of them may move the model - are
// ResidualStore's, because the rules that use them are. The alias exists so
// callers that size an array by it do not have to know which header it moved
// to.
#define LUM_USER_POINTS RESIDUAL_MAX

// A nudge made this close to white is not taught as a colour correction.
// Teaching one means dividing the observed decades by the saturation fade to
// get back to the sat-100 size ResidualStore::refit() fits in, and below this
// fade that division amplifies whatever noise is in the reading by more than
// a factor of ten - a nudge nobody would trust manufactured into one that
// looks precise.
#define LUM_MIN_FADE_TO_LEARN 0.1

// How finely the hue axis of the surface at GET /luminance/surface is walked.
// The knots are 60 degrees apart and the interesting thing about the surface
// is what happens *between* them - above all across the seam at 300 back to 0,
// which is the one segment whose far end is not the next knot in store order.
// Fifteen degrees gives 24 columns: enough to draw a curve, small enough that
// the whole answer stays under a kilobyte.
#define LUM_SURFACE_HUE_STEP 15

// How many ambient rows the surface at GET /luminance/surface samples. The
// cone has no knots of its own any more - it is a straight line - so the rows
// are chosen rather than read out of the model: evenly spaced in log light
// between what the fit was actually built from (Profile::logLuxMin/Max),
// which is enough to draw the curve and to show blue's own line bending away
// from it without pretending the sampling is a measurement.
#define LUM_SURFACE_LUX_ROWS 9

namespace Luminance
{
    /** One thing the user said: at this much light, I wanted this much face. */
    struct Point
    {
        float lux;
        uint8_t percent;
        uint32_t seconds;   // uptime when it was made, for the read-out only

        /**
         * The colour on the face when it was said, and it is not decoration.
         *
         * "60 %" means a different amount of light in blue than in green - the
         * eye is far less sensitive at 470 nm than at 555 - so a point is only
         * comparable with another taught in the same colour. Measured on this
         * clock: the same setting emits 0.99 decades less light in full blue
         * than in the green it runs, a factor of ten, and constant to 2 % over
         * the whole range.
         *
         * Nothing reads this yet. It is kept from now on so that the model
         * which wants it - one curve learned once, plus one offset per colour -
         * arrives to a history rather than a cold start. Three bytes a point,
         * and not recoverable afterwards if they are not spent now.
         *
         * Hue and saturation rather than the RGB the driver writes: those are
         * what the owner set, and the RGB is a rendering of them the same
         * driver can produce again.
         */
        uint16_t hue;       // 0..359, or LUM_HUE_UNKNOWN
        uint8_t sat;        // 0..100
    };

    /**
     * One thing the owner taught the clock: Table 2, refit together with
     * Table 1 rather than averaged into a correction on top of either.
     *
     * Defined in ResidualStore rather than here, because the rules that decide
     * what may replace what, and what refit() does with the result, are there
     * - pure, and therefore checked by tests/host/test_residual_store.cpp
     * rather than by a month of evenings. Two things that a simpler store
     * gets wrong quietly and this does not: white has no hue, and two
     * corrections at the same light in different colours are two statements.
     */
    typedef ResidualStore::Residual Residual;

    /** Which layers produced the number on the wall. */
    enum Source
    {
        // No factory profile, or one this firmware refuses: the learned white
        // line, exactly as every clock behaved before the model existed.
        SOURCE_LEGACY = 0,
        // The factory fit alone - nothing taught yet, or nothing taught about
        // a colour at all (a stray white correction, say).
        SOURCE_FACTORY = 1,
        // The nose and blue's line refit from Table 1 and this clock owner's
        // own Table 2 together.
        SOURCE_FACTORY_USER = 2,
    };

    /** What the automatic wants, and what that answer rests on. */
    struct Target
    {
        uint8_t percent;        // clamped into the regulated range
        uint8_t source;         // Source
        uint8_t factory;        // what the factory fit alone asked for
        float bias;             // decades the user's corrections added
        bool limited;           // the colour cannot emit this at any setting
        // Always false: the parametric fit carries no per-observation "at
        // least this much" the grid it replaced did. Kept in the struct so
        // the wire format does not change underneath the web UI.
        bool bound;
        bool clamped;           // the room is outside anything measured
    };

    /** Loads the points, the line and the residuals from NVS. */
    void begin();

    /** What the line makes of a reading, clamped to the regulated range. */
    uint8_t forLux(float lux);

    /**
     * What the automatic wants at this light **in this colour**.
     *
     * The one call the render loop and the API should be making. Falls back to
     * forLux() when there is no factory profile, so a clock without one
     * behaves exactly as it did - and says so through `source` rather than
     * quietly looking the same.
     */
    void targetFor(float lux, uint16_t hue, uint8_t sat, Target &out);

    /**
     * Whether a stored set of corrections was learned on a different profile
     * from the one now installed, and is therefore being ignored.
     *
     * A filesystem update can bring a new measurement in underneath them. They
     * are still what somebody said, so they are kept rather than deleted - but
     * adding a correction measured against one baseline to another is
     * arithmetic across two models, and the factory restore is the deliberate
     * act that resolves it.
     *
     * False on a clock that has never corrected anything, which is the case
     * this must not report as a mismatch: nothing disagrees with the profile
     * when nothing has been said about it.
     */
    bool residualsStale();

    /** The corrections, oldest first. Returns how many, 0..LUM_USER_POINTS. */
    uint8_t residuals(Residual *out, uint8_t max);

    /**
     * The nose and blue's line this clock is actually running right now -
     * whatever ResidualStore::refit() last made of Table 1 and Table 2
     * together - and whether that is worth showing at all.
     *
     * False under exactly the condition targetFor() uses for "nothing was
     * taught about a colour": no factory profile, or a refit that never
     * produced anything usable, or a store that holds nothing but a stray
     * white correction. `out` is untouched in that case, matching every other
     * accessor here - a caller that checks the return value first never reads
     * a fit that is not actually in effect.
     */
    bool learnedFit(ResidualStore::Fit &out);

    /** Forgets one, by its position in residuals(). */
    bool forgetResidual(uint8_t index);

    /**
     * Back to the factory baseline: the corrections go, the shipped fit stays.
     *
     * The device calibration is **not** touched. The coupling map is a
     * measurement of where this clock's sensor sits behind its own letters -
     * twenty minutes of the clock measuring itself - and it has nothing to do
     * with anybody's preferences. Throwing it away with them would be the one
     * irreversible part of a reset that is otherwise cheap.
     *
     * The legacy white points go too: they are the same preferences said in
     * the old coordinates, and leaving them would mean a clock that reads as
     * restored until its profile is next refused.
     *
     * False when there is no valid factory profile to restore to - which is
     * checked *before* anything in NVS is touched, so a failure leaves the
     * clock exactly as it was rather than half way between two models.
     */
    bool factoryRestore();

    /**
     * The user moved the brightness slider while the automatic was on.
     *
     * Suspends the automatic and arms the settle timer. Called again for every
     * move, which only pushes the timer out - a drag is one adjustment.
     *
     * The colour comes in with it rather than being read at settle time: what
     * is being described is the face the person was looking at when they
     * decided, and ten seconds is long enough to have changed it.
     */
    void nudged(uint8_t percent, uint16_t hue, uint8_t sat);

    /** True while a nudge is being waited out; `percent` is what to display. */
    bool adjusting(uint8_t &percent);

    /**
     * Called once a second. Keeps the pair and re-fits when the settle time
     * has passed. Returns true if anything was stored, so the caller can log it.
     */
    bool poll(float lux);

    /** Throws the points away and restores the default line. */
    void reset();

    /**
     * Forgets one point, by its position in points().
     *
     * A point can be wrong rather than merely old, and until this existed the
     * only way to remove one was to throw the whole calibration away. The case
     * that produced it: a correction made ten seconds after the room went dark
     * was stored at 0.1184 lx when the room was at 0.0008 - one bad point
     * among three good ones, and no way to drop it alone.
     *
     * Re-fits and stores. Returns false if there is no such point.
     */
    bool forget(uint8_t index);

    /** The line, for the read-out and the API. */
    float slope();
    float offset();

    /**
     * The regulated range, both ends stored with the curve.
     *
     * forLux() clamps to them and nudged() refuses anything outside, so a
     * clock cannot be taught a point it would never be allowed to show - which
     * would sit in the fit pulling the line towards a brightness that can
     * never appear on the wall.
     */
    uint8_t minPercent();
    uint8_t maxPercent();

    /**
     * Moves the range. False when the two ends are the wrong way round, too
     * close together, or outside 1..100; the caller then knows to refuse
     * rather than to store something the screen would draw as a flat line.
     *
     * Stored points are left alone even when they now fall outside: they are
     * what somebody said, and a range moved back would want them again. They
     * are clamped where they are used, not where they are kept.
     */
    bool setRange(uint8_t low, uint8_t high);

    /** The points, oldest first. Returns how many there are, 0..LUM_POINTS. */
    uint8_t points(Point *out, uint8_t max);

    /**
     * Whether a point was in the last fit.
     *
     * A point at the top of the range is censored - the person wanted "at
     * least this much" and the slider had nothing more to give - and least
     * squares would read it as an equality and pull the bright end down. It is
     * kept and shown but left out, unless leaving it out leaves fewer than two
     * points to fit, in which case a poor line beats no line.
     */
    bool usedInFit(uint8_t index);

    /**
     * Whether the last fit could estimate a slope, or had to keep the old one
     * for want of spread. The read-out says so, because it is the difference
     * between "the clock has learned how your room behaves" and "the clock has
     * learned how bright you like it in one particular room".
     */
    bool slopeFitted();
}

#endif
