/**
 * Luminance
 * The automatic brightness curve, and how it learns.
 *
 * The curve is a straight line in log light:
 *
 *     brightness = slope * log10(lux) + offset       clamped to 20..100
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
 * Four things keep the fit from going somewhere silly:
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
 * - **The output is clamped to 20..100.** Below a fifth the face is not really
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

// How many calibration points are kept. Ten is enough to describe a home - a
// few daylight levels, a lamp, a dark room - and small enough that a bad one
// is forgotten within a week of ordinary use.
#define LUM_POINTS 10

// The regulated range. Rule one and rule two of this feature.
#define LUM_MIN_PERCENT 20
#define LUM_MAX_PERCENT 100

// Silence after a slider move before the pair is kept. Long enough that
// dragging counts as one adjustment rather than fifty.
#define LUM_SETTLE_MS 10000

// A new point this close to an old one in lux replaces it. 1.3 is well inside
// what the eye calls "the same light" and well outside sensor noise.
#define LUM_SAME_LIGHT_RATIO 1.3f

// Decades of spread the points must cover before a slope is fitted at all.
// 0.6 is a factor of four, two doublings - the least that says anything.
#define LUM_FIT_MIN_DECADES 0.6f

// The line a clock starts with, and returns to on reset. Deliberately cautious
// rather than good: it assumes a sensor in the open, and behind a front panel
// both readings shrink by the same factor - which in log space only shifts the
// line sideways, so an uncalibrated clock still dims in the right direction,
// just not by the right amount. Better numbers when there is more experience.
#define LUM_DEFAULT_LOW_LUX     0.3f
#define LUM_DEFAULT_LOW_PERCENT 20
#define LUM_DEFAULT_HIGH_LUX    9.0f
#define LUM_DEFAULT_HIGH_PERCENT 100

namespace Luminance
{
    /** One thing the user said: at this much light, I wanted this much face. */
    struct Point
    {
        float lux;
        uint8_t percent;
        uint32_t seconds;   // uptime when it was made, for the read-out only
    };

    /** Loads the points and the line from NVS, or starts from the default. */
    void begin();

    /** What the line makes of a reading, clamped to 20..100. */
    uint8_t forLux(float lux);

    /**
     * The user moved the brightness slider while the automatic was on.
     *
     * Suspends the automatic and arms the settle timer. Called again for every
     * move, which only pushes the timer out - a drag is one adjustment.
     */
    void nudged(uint8_t percent);

    /** True while a nudge is being waited out; `percent` is what to display. */
    bool adjusting(uint8_t &percent);

    /**
     * Called once a second. Keeps the pair and re-fits when the settle time
     * has passed. Returns true if anything was stored, so the caller can log it.
     */
    bool poll(float lux);

    /** Throws the points away and restores the default line. */
    void reset();

    /** The line, for the read-out and the API. */
    float slope();
    float offset();

    /** The points, oldest first. Returns how many there are, 0..LUM_POINTS. */
    uint8_t points(Point *out, uint8_t max);

    /**
     * Whether the last fit could estimate a slope, or had to keep the old one
     * for want of spread. The read-out says so, because it is the difference
     * between "the clock has learned how your room behaves" and "the clock has
     * learned how bright you like it in one particular room".
     */
    bool slopeFitted();
}

#endif
