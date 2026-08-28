/**
 * FactoryProfile
 * The colour-aware brightness model, as arithmetic and nothing else.
 *
 * A percentage on the brightness slider is not an amount of light. The eye
 * responds to light, and how much light a percentage produces depends on the
 * colour the face is showing: measured on this clock, the same setting emits
 * about a tenth as much in full blue as in the green it runs. Expressed in
 * percent that correction is neither an offset nor a factor - both wander by
 * three to one across the range - and expressed in the log of the emitted
 * light it is very nearly a constant. So the model lives in log light:
 *
 *     log L = white(log lux) + residual(log lux, hue) * fade(sat)
 *
 * and the percentage is what comes back out of it, by inverting the gamma, the
 * per-channel scaling and the measured drive table over the integers in the
 * regulated range. There is no closed form for that inverse and deliberately
 * none attempted: every step of the forward direction rounds, and an algebraic
 * inverse would disagree with it at exactly the settings the clock spends its
 * evenings at.
 *
 * **This file is pure.** No Arduino, no ArduinoJson, no LittleFS, no logging -
 * only <math.h> and integers. That is what lets `tests/host/` compile it with
 * a desktop compiler and run it against the golden vectors the Python model
 * writes, which is the only way the two implementations can be shown to agree.
 * Everything that touches the clock - reading the file, checking its checksum,
 * remembering which profile is installed - is in FactoryLuminance.
 *
 * The numbers themselves are never compiled in. They are measured on one
 * clock, through one diffuser, behind one mask, and a guessed profile would be
 * worse than none: it would look exactly like a measured one. See
 * FactoryLuminance.h for where they come from and what is checked before they
 * are believed.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.2
 * @created  27.8.2026
 * @updated  27.8.2026
 */
#ifndef FACTORY_PROFILE_H
#define FACTORY_PROFILE_H

#include <stdint.h>

// The model this code evaluates, and the file schema it reads. Both are
// compared against the profile rather than assumed: two models can share every
// field name and mean different things by them, and that is the failure a
// version number exists to prevent.
#define FACTORY_MODEL_ID "white-baseline-plus-cyclic-hue-loglux-residual-grid"
#define FACTORY_SCHEMA 1
#define FACTORY_RUNTIME_SCHEMA 1

// Room for the shape this model can take, not for the shape it happens to
// have. The reviewed profile is six ambient levels by six hue knots; the
// limits below are what a later measurement may grow to without a firmware
// change, and a profile beyond them is refused rather than truncated.
#define FACTORY_MAX_LEVELS 12
#define FACTORY_MAX_HUES 8
// COUPLING_MAX_LEVELS in Coupling.h. The same lamp, so the same limit: a
// longer table would describe a driver the rest of the firmware is not using.
#define FACTORY_MAX_DRIVE 20

// How far the grid may fall between two ambient levels before it is refused.
// This is the last bit of a rounded double and **not a budget**: the isotonic
// step in the generator cannot leave a real dip, so a file carrying one was
// not built by that generator. GRID_MAX_DIP in scripts/factory_luminance.py is
// the same number on the other side.
#define FACTORY_MAX_DIP 1e-9

namespace FactoryProfile
{
    /** Why an answer is not simply the answer. Reported, never folded in. */
    enum Limited { LIMITED_NONE = 0, LIMITED_CEILING = 1, LIMITED_FLOOR = 2 };
    enum Clamped { CLAMP_NONE = 0, CLAMP_BELOW = 1, CLAMP_ABOVE = 2 };

    /** One ambient level of the grid: the white line, and the colour offsets. */
    struct Level
    {
        double logLux;
        double white;                       // log output white asks for here
        double residual[FACTORY_MAX_HUES];  // decades, per hue knot
        uint8_t bound[FACTORY_MAX_HUES];    // 1 where the corner is "at least"
        uint8_t censored;                   // the whole level ran out of slider
    };

    /**
     * The model, as the clock holds it.
     *
     * The hardware half - the drive table and the luminance weights - travels
     * with the measurement rather than being looked up beside it. A profile
     * evaluated against a different drive table is being evaluated against a
     * different lamp, and the caller who has to remember to pass the right one
     * will one day not.
     */
    struct Profile
    {
        uint8_t levelCount;
        uint8_t hueCount;
        uint8_t driveCount;
        uint16_t huePeriod;
        uint8_t percentMin;
        uint8_t percentMax;
        // The fade is zero at or below satZero and whole at or above satFull,
        // and satZero is strictly the lower of the two: reversed, the colour
        // residual would be whole on a white face and absent on a coloured
        // one, which is the model meaning the opposite of itself.
        uint8_t satZero;
        uint8_t satFull;
        uint16_t hueKnot[FACTORY_MAX_HUES];
        Level level[FACTORY_MAX_LEVELS];
        uint8_t driveLevel[FACTORY_MAX_DRIVE];
        double driveResponse[FACTORY_MAX_DRIVE];
        double weight[3];                   // photopic, red green blue
    };

    /**
     * What the profile asks for here, and what that answer rests on.
     *
     * Three different things can be wrong with an answer and each is reported
     * separately, because the remedies differ: `clamped` says the room is
     * outside anything anybody measured, `bound` says a grid corner behind it
     * only ever said "at least this much", and `limited` says the colour
     * cannot emit what was asked for at any percentage in the range - which is
     * the gamut, not a fault.
     */
    struct Answer
    {
        uint8_t percent;
        double target;                      // log output the model asks for
        double white;
        double residual;
        double achieved;                    // what that percentage really emits
        double logLux;
        Limited limited;
        Clamped clamped;
        bool bound;
    };

    /**
     * Whether this profile is one the evaluator may act on.
     *
     * Every check has a counterpart in `factory_luminance.load_runtime()` and
     * they are in the same order. A grid the interpolator cannot walk - fewer
     * than two levels, levels not strictly ascending, a residual row that does
     * not match the knots, a value that is not a number - is refused here
     * rather than producing a plausible percentage out of the wrong bracket.
     *
     * Note what is *not* refused: a grid that dips, asking for slightly less
     * light in a slightly brighter room. The reviewed profile has three such
     * segments and they are reported as status, not smoothed away. A model
     * edited until it looked right would be a model of the editing.
     */
    bool valid(const Profile &profile);

    /**
     * The largest fall in target log output between two adjacent levels.
     *
     * Measured on the grid, never read out of a status field beside it. The
     * reviewed profile says `monotone: false` and its grid is monotone all the
     * same - the status is about the *observations*, where one hue falls a
     * quarter of a decade, and the isotonic step pools the levels that
     * disagreement sits between before anything is shipped. A firmware
     * trusting the status field would report a fault it does not have; one
     * trusting it the other way would accept a grid that really does fall.
     *
     * Walks the white line on its own as well as every hue: a dip can sit in
     * the baseline with every residual flat, which a check walking only the
     * colour rows would miss.
     *
     * Zero when the grid rises everywhere, which is what this generator can
     * produce and therefore what `valid()` requires - see FACTORY_MAX_DIP.
     */
    double worstDip(const Profile &profile);

    // The hardware path, exposed because the host tests check each step
    // against the Python transcription rather than only the answer. A parity
    // failure in `gammaScale` and one in the inversion look identical from the
    // outside and are found in completely different places.

    /** 0..359 to FastLED's 0..255, as `Settings` hands it to the driver. */
    uint8_t hueByte(uint16_t degrees);
    /** 0..100 to 0..255, likewise. */
    uint8_t satByte(uint8_t percent);
    /** `CRGB(CHSV(hue, sat, 255))`, byte for byte - FastLED's rainbow wheel. */
    void rainbow(uint8_t hue, uint8_t sat, uint8_t *rgb);
    /** `_gammaScale()`: the setting as a drive value, gamma 2.2, floor of 1. */
    uint8_t gammaScale(uint8_t percent);
    /** `_brightnessScaleColor()`: one channel once the brightness is on it. */
    uint8_t channelDrive(uint8_t component, uint8_t scaled);
    /** The measured drive response, interpolated as `Coupling::responseFor()`. */
    double driveResponse(const Profile &profile, int value);
    /** What the face emits at this colour and setting, relative to white 100 %. */
    double relativeOutput(const Profile &profile, uint16_t hue, uint8_t sat,
                          uint8_t percent);
    /** Log of that, floored so a dark face is still a number. */
    double logOutput(double output);

    /**
     * The whole answer: light, colour, and what the slider should say.
     *
     * `lux` must be positive - log10 has nothing to say about zero and a
     * sensor that reports it is reporting a floor, not a room. False when the
     * profile is not one this may act on, in which case `out` is untouched and
     * the caller falls back to the white curve it had before.
     */
    bool evaluate(const Profile &profile, double lux, uint16_t hue, uint8_t sat,
                  Answer &out);

    /**
     * The same, with a correction of the user's own added to the target.
     *
     * `bias` is in decades of emitted light, which is the coordinate the model
     * is in and the only one a correction can be carried in: the same "two per
     * cent brighter" is a different number of slider steps in blue than in
     * green, and stored as steps it would mean something else the next time
     * the colour changed. Zero is exactly `evaluate()`.
     *
     * The layering is deliberate. The factory grid is what this optical stack
     * does, measured once; the bias is what this person prefers, learned from
     * their nudges. Keeping them apart is what lets a filesystem update
     * replace the first without discarding the second - and what lets a
     * factory restore discard the second without re-measuring the first.
     */
    bool evaluateWith(const Profile &profile, double lux, uint16_t hue,
                      uint8_t sat, double bias, Answer &out);
}

#endif
