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
 *     log L = cone(log lux) + residual(log lux, hue) * fade(sat)
 *
 * **The second version of this file.** The first shipped `residual` as a
 * six-knot grid, one row per measured ambient level, read back by
 * interpolation - see git history if that shape is ever wanted again, and
 * `tests/golden/factory_vectors.txt` for what it produced. It cross-validated
 * at 6.9 % RMS against its own 6 % goal on ~40 measurements with roughly as
 * many free numbers as points - an overfitting gap between that figure and
 * its 4.1 % in-sample one that a model with this few parameters does not get
 * to have. What replaced it is smaller on purpose:
 *
 *   - `cone(x)` is one straight line, shared by white and every saturated
 *     colour - the slope this diffuser turns a decade of ambient light into.
 *   - `residual(hue)` for every hue but one is a single first-harmonic wave -
 *     three numbers, `a0 + a1*cos(hue) + b1*sin(hue)` - fitted once against
 *     five colours and refittable in a fraction of a second against however
 *     many more a clock has taught it. It carries no light-level term: on the
 *     measurement this was built from, red through magenta hold flat to
 *     within noise across six ambient levels while blue's own residual falls
 *     0.19 decades per decade of light - which is the argument for the next
 *     point, not against this one.
 *   - **Blue is not on the wave.** Its own residual measurably has a slope in
 *     light the other five do not, so it gets its own line, `bSlope*x +
 *     bOffset`, blended into the wave over `blendHalfWidth` degrees either
 *     side of `blueHue` by a raised cosine - full weight at the centre, zero
 *     at the edge, and nothing discontinuous at 235° or 245° because nothing
 *     here is a grid cell with a neighbour. See `blueWeight()`.
 *
 * The percentage is what comes back out of the target, by inverting the
 * gamma, the per-channel scaling and the measured drive table over the
 * integers in the regulated range. There is no closed form for that inverse
 * and deliberately none attempted: every step of the forward direction
 * rounds, and an algebraic inverse would disagree with it at exactly the
 * settings the clock spends its evenings at.
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
 * @version  2.3
 * @created  27.8.2026
 * @updated  29.8.2026
 */
#ifndef FACTORY_PROFILE_H
#define FACTORY_PROFILE_H

#include <stdint.h>

// The model this code evaluates, and the file schema it reads. Both are
// compared against the profile rather than assumed: two models can share every
// field name and mean different things by them, and that is the failure a
// version number exists to prevent.
#define FACTORY_MODEL_ID "white-cone-plus-first-harmonic-hue-nose-with-blue-line"
#define FACTORY_SCHEMA 2
#define FACTORY_RUNTIME_SCHEMA 2

// COUPLING_MAX_LEVELS in Coupling.h. The same lamp, so the same limit: a
// longer table would describe a driver the rest of the firmware is not using.
#define FACTORY_MAX_DRIVE 20

// Room for Table 1 - the measurements the shipped nose and blue's line were
// fitted from - carried in the profile so the learning layer can refit
// against them rather than against numbers already baked into a curve. The
// reviewed record holds 36; this is not tight against that on purpose.
#define FACTORY_MAX_POINTS 48

namespace FactoryProfile
{
    /** Why an answer is not simply the answer. Reported, never folded in. */
    enum Limited { LIMITED_NONE = 0, LIMITED_CEILING = 1, LIMITED_FLOOR = 2 };
    enum Clamped { CLAMP_NONE = 0, CLAMP_BELOW = 1, CLAMP_ABOVE = 2 };

    /**
     * One measurement Table 1 or Table 2 is made of: a colour, a light level,
     * and how far the face's output sat from the cone there. Table 1 (shipped
     * with the profile) and Table 2 (ResidualStore, taught by the owner) are
     * the same shape for exactly this reason - refit() combines them without
     * translating between two formats.
     */
    struct Point
    {
        uint16_t hue;
        uint8_t sat;
        double logLux;
        double residual;    // decades, relative to the cone, at this sat
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
        uint16_t huePeriod;

        // The cone: one straight line in log light, shared by white and every
        // saturated colour. logLuxMin/logLuxMax are not a functional clamp -
        // the line is evaluated past them rather than held flat, which is the
        // one thing a line does better than the grid it replaced - only a
        // record of what was actually measured, so `evaluate()` can say
        // "outside anything anybody measured" without pretending to refuse it.
        double coneSlope;
        double coneOffset;
        double logLuxMin;
        double logLuxMax;

        // The nose: one first-harmonic wave, hue in, decades out, no term in
        // light. Used everywhere blueWeight() is not 1.
        double noseA0;
        double noseA1;
        double noseB1;

        // Blue's own line, blended in near blueHue. Also decades, also a
        // function of log light this time - see the file header for why.
        uint16_t blueHue;
        double blueSlope;
        double blueOffset;
        double blendHalfWidth;   // degrees; 0 weight at this distance from blueHue

        uint8_t percentMin;
        uint8_t percentMax;
        // The fade is zero at or below satZero and whole at or above satFull,
        // and satZero is strictly the lower of the two: reversed, the colour
        // residual would be whole on a white face and absent on a coloured
        // one, which is the model meaning the opposite of itself.
        uint8_t satZero;
        uint8_t satFull;

        uint8_t driveCount;
        uint8_t driveLevel[FACTORY_MAX_DRIVE];
        double driveResponse[FACTORY_MAX_DRIVE];
        double weight[3];                   // photopic, red green blue

        // Table 1: provenance, not arithmetic. evaluate() and valid() never
        // look at this - it exists so ResidualStore::refit() can combine it
        // with Table 2, the owner's own taught points, and solve the nose and
        // blue's line again. `residual` is already in the same coordinate
        // `evaluate()` computes internally: log output minus the cone, at the
        // saturation the observation was made at (always 100 on the reviewed
        // record - every colour point is fully saturated, every white anchor
        // is excluded before this array is built).
        uint8_t pointCount;
        Point point[FACTORY_MAX_POINTS];
    };

    /**
     * What the profile asks for here, and what that answer rests on.
     *
     * Two different things can be wrong with an answer and each is reported
     * separately, because the remedies differ: `clamped` says the room is
     * outside anything anybody measured (informational only - see
     * `Profile::logLuxMin`), and `limited` says the colour cannot emit what
     * was asked for at any percentage in the range, which is the gamut, not a
     * fault.
     *
     * `bound` is gone from this half of the model. The grid carried a
     * per-corner "at least this much" from an observation that ran out of
     * slider; the parametric fit simply excludes a censored observation
     * rather than encoding it into the surface, so nothing evaluated here is
     * ever a lower bound. A user's own censored nudge is still one - see
     * ResidualStore::Residual::bound - which is a different layer answering a
     * different question.
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
    };

    /**
     * The weight blue's own line carries at this hue, 0..1.
     *
     * A raised cosine: 1 at `blueHue`, 0 at `blendHalfWidth` degrees either
     * side of it and beyond, smooth at both ends (zero derivative), one
     * parameter. Exposed because the host tests check it directly - a fault
     * here would otherwise only show up as a slightly wrong percentage 40
     * degrees from blue, which is a hard place to notice one.
     */
    double blueWeight(const Profile &profile, uint16_t hue);

    /**
     * The saturation fade at this saturation, 0..1 - exactly what
     * evaluateWith() scales the residual by. Exposed so ResidualStore::refit()
     * can un-fade a taught point to the sat-100-equivalent size Table 1's own
     * points are already in, rather than a second reading of satZero/satFull.
     */
    double fadeFor(const Profile &profile, uint8_t sat);

    /**
     * Whether this profile is one the evaluator may act on.
     *
     * Every check has a counterpart in `factory_luminance.load_runtime()` and
     * they are in the same order.
     *
     * The one check worth naming: `coneSlope > 0` and `coneSlope + blueSlope
     * > 0`. Those are the two extremes blueWeight() interpolates between, and
     * together they are the whole monotonicity argument this model needs - a
     * hue whose combined slope is not positive would ask for less light in a
     * brighter room, which the grid this replaced caught by walking every
     * cell and this catches by construction from two numbers.
     */
    bool valid(const Profile &profile);

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
     * The layering is deliberate. The factory fit is what this optical stack
     * does, measured once; the bias is what this person prefers, learned from
     * their nudges. Keeping them apart is what lets a filesystem update
     * replace the first without discarding the second - and what lets a
     * factory restore discard the second without re-measuring the first.
     */
    bool evaluateWith(const Profile &profile, double lux, uint16_t hue,
                      uint8_t sat, double bias, Answer &out);
}

#endif
