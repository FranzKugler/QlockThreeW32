/**
 * The firmware evaluator, against the model that produced the profile.
 *
 * `src/FactoryProfile.cpp` is compiled here by a desktop compiler - no
 * Arduino, no ArduinoJson, no clock - and asked the same questions
 * `scripts/build_cone_golden_vectors.py` was asked when it wrote
 * `tests/golden/factory_vectors.txt`. The two have to give the same integer
 * percentage at every one of them.
 *
 * That is worth more than it sounds. The path from a room to a percentage runs
 * through FastLED's rainbow wheel, an integer hue conversion, a gamma with a
 * floor, a per-channel scale that rounds, a measured drive table that
 * interpolates, three luminance weights and a search over integers - and a
 * single count out of place anywhere in it moves the answer at exactly the dim
 * settings the clock spends its evenings at. There is no environment where
 * both implementations run in one process, so the agreement is a file, and
 * this is what keeps the file honest.
 *
 * Build and run:  tests/host/run.sh
 */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "../../src/FactoryProfile.h"

using namespace FactoryProfile;

static int failures = 0;
static int checks = 0;

static void ok(bool condition, const std::string &what)
{
    checks++;
    if (!condition)
    {
        failures++;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

/** The flat fixtures are whitespace-separated numbers with `#` comments. */
class Reader
{
public:
    explicit Reader(const std::string &path)
    {
        std::ifstream in(path.c_str());
        if (!in) { std::printf("FAIL: cannot open %s\n", path.c_str()); std::exit(2); }
        std::string word;
        while (in >> word)
        {
            if (word[0] == '#') { std::string rest; std::getline(in, rest); continue; }
            words.push_back(word);
        }
    }
    double number() { return std::atof(next().c_str()); }
    int integer() { return std::atoi(next().c_str()); }
private:
    const std::string &next()
    {
        if (at >= words.size()) { std::printf("FAIL: fixture ran out\n"); std::exit(2); }
        return words[at++];
    }
    std::vector<std::string> words;
    size_t at = 0;
};

static Profile loadProfile(const std::string &path)
{
    Reader in(path);
    Profile profile{};
    profile.percentMin = (uint8_t)in.integer();
    profile.percentMax = (uint8_t)in.integer();
    profile.satZero = (uint8_t)in.integer();
    profile.satFull = (uint8_t)in.integer();
    profile.huePeriod = (uint16_t)in.integer();
    profile.coneSlope = in.number();
    profile.coneOffset = in.number();
    profile.logLuxMin = in.number();
    profile.logLuxMax = in.number();
    profile.noseA0 = in.number();
    profile.noseA1 = in.number();
    profile.noseB1 = in.number();
    profile.blueHue = (uint16_t)in.integer();
    profile.blueSlope = in.number();
    profile.blueOffset = in.number();
    profile.blendHalfWidth = in.number();
    profile.driveCount = (uint8_t)in.integer();
    for (uint8_t i = 0; i < profile.driveCount; i++)
        profile.driveLevel[i] = (uint8_t)in.integer();
    for (uint8_t i = 0; i < profile.driveCount; i++)
        profile.driveResponse[i] = in.number();
    for (int i = 0; i < 3; i++) profile.weight[i] = in.number();
    return profile;
}

/* ------------------------------------------------------------------ */

static void theProfileLoads(const Profile &profile)
{
    ok(valid(profile), "the shipped profile is one the evaluator may act on");
    ok(profile.huePeriod == 360, "the wheel is 360 degrees");
    ok(profile.blendHalfWidth > 0.0, "blue's blend has a width");
}

/**
 * The whole monotonicity argument this model needs: the cone on its own, and
 * the cone with blue's line fully blended in - the two extremes blueWeight()
 * ever interpolates between. No grid to walk any more, so this is two numbers
 * instead of the dip search the six-knot model needed.
 */
static void theConeRisesWithLightEverywhereOnTheWheel(const Profile &good)
{
    ok(good.coneSlope > 0.0, "the shipped cone rises with light");
    ok(good.coneSlope + good.blueSlope > 0.0,
       "and still rises once blue's own line is fully blended in");

    Profile flat = good;
    flat.coneSlope = 0.0;
    ok(!valid(flat), "a cone that does not rise is refused");

    Profile falling = good;
    falling.coneSlope = -0.1;
    ok(!valid(falling), "a cone that falls is refused");

    // Blue's own slope is negative on the shipped profile (it falls behind
    // white as light rises) - refused only once it overtakes the cone's,
    // which is the actual monotonicity boundary, not merely being negative.
    Profile steepBlue = good;
    steepBlue.blueSlope = -(good.coneSlope) - 0.01;
    ok(!valid(steepBlue),
       "blue's line refused once it overtakes the cone's own slope");

    Profile shallowerBlue = good;
    shallowerBlue.blueSlope = -(good.coneSlope) + 0.01;
    ok(valid(shallowerBlue),
       "and accepted while the combined slope is still just positive");
}

/**
 * The raised cosine blueWeight() is built on: 1 at the centre, 0 at and
 * beyond the half width, symmetric, and nowhere outside 0..1. A fault here
 * would otherwise only show up as a slightly wrong percentage forty degrees
 * from blue, which is a hard place to notice one.
 */
static void blueWeightIsAWellBehavedBump(const Profile &good)
{
    char what[96];
    ok(std::fabs(blueWeight(good, good.blueHue) - 1.0) < 1e-12,
       "full weight exactly at blue's own hue");

    uint16_t edge = (uint16_t)(((int)good.blueHue
                              + (int)good.blendHalfWidth + good.huePeriod)
                             % good.huePeriod);
    ok(std::fabs(blueWeight(good, edge)) < 1e-9,
       "zero weight exactly at the edge of the blend");

    uint16_t beyond = (uint16_t)(((int)edge + 5 + good.huePeriod) % good.huePeriod);
    ok(blueWeight(good, beyond) == 0.0, "and zero beyond it, not merely small");

    double last = 1.0;
    for (int step = 0; step <= (int)good.blendHalfWidth; step += 5)
    {
        uint16_t hue = (uint16_t)(((int)good.blueHue + step + good.huePeriod)
                                 % good.huePeriod);
        double w = blueWeight(good, hue);
        std::snprintf(what, sizeof(what),
                      "weight %d degrees from blue does not exceed the last",
                      step);
        ok(w <= last + 1e-12, what);
        ok(w >= -1e-12 && w <= 1.0 + 1e-12, "and stays inside 0..1");
        last = w;
    }

    uint16_t plusSide = (uint16_t)((good.blueHue + 10) % good.huePeriod);
    uint16_t minusSide = (uint16_t)((good.blueHue + good.huePeriod - 10) % good.huePeriod);
    ok(std::fabs(blueWeight(good, plusSide) - blueWeight(good, minusSide)) < 1e-12,
       "symmetric either side of blue's own hue");
}

/**
 * The driver computes the gamma in float and this model computes it in double.
 * They have to agree, or the clock regulates against a table the profile was
 * never measured on - and the difference would be one count at one percentage,
 * which is exactly the kind of thing nobody notices.
 */
static void theGammaMatchesTheDriver()
{
    for (int percent = 0; percent <= 100; percent++)
    {
        uint8_t mine = gammaScale((uint8_t)percent);
        uint8_t driver;
        if (percent == 0) driver = 0;
        else if (percent >= 100) driver = 255;
        else
        {
            long value = lroundf(255.0f * powf(percent / 100.0f, 2.2f));
            if (value < 1) value = 1;
            driver = (uint8_t)value;
        }
        char what[80];
        std::snprintf(what, sizeof(what),
                      "gamma at %d %%: %d against the driver's %d",
                      percent, mine, driver);
        ok(mine == driver, what);
    }
    ok(gammaScale(1) >= 1, "the floor of 1 holds: 1 % must not go dark");
    ok(gammaScale(0) == 0, "and zero is still zero");
}

/** White is white: at sat 0 the wheel gives 255,255,255 whatever the hue. */
static void whiteIsWhiteAtEveryHue()
{
    for (int hue = 0; hue < 360; hue += 7)
    {
        uint8_t rgb[3];
        rainbow(hueByte((uint16_t)hue), satByte(0), rgb);
        ok(rgb[0] == 255 && rgb[1] == 255 && rgb[2] == 255,
           "sat 0 is white at hue " + std::to_string(hue));
    }
}

/** A profile the evaluator must refuse rather than answer plausibly. */
static void refusals(const Profile &good)
{
    Answer answer{};

    Profile broken = good;
    broken.coneOffset = std::nan("");
    ok(!valid(broken), "a value that is not a number is refused");

    Profile flatSpan = good;
    flatSpan.logLuxMax = flatSpan.logLuxMin;
    ok(!valid(flatSpan), "a measured range with no span is refused");

    Profile backwardsSpan = good;
    backwardsSpan.logLuxMax = good.logLuxMin - 1.0;
    ok(!valid(backwardsSpan), "and one that runs backwards");

    Profile empty = good;
    empty.percentMin = empty.percentMax;
    ok(!valid(empty), "an empty regulated range is refused");

    Profile lamp = good;
    lamp.driveLevel[1] = lamp.driveLevel[0];
    ok(!valid(lamp), "a drive table that does not descend is refused");

    Profile noBlend = good;
    noBlend.blendHalfWidth = 0.0;
    ok(!valid(noBlend), "a zero-width blend is refused - it would divide by zero");

    Profile hugeBlend = good;
    hugeBlend.blendHalfWidth = (double)good.huePeriod;
    ok(!valid(hugeBlend), "a blend wider than half the wheel is refused");

    Profile offWheel = good;
    offWheel.blueHue = good.huePeriod;
    ok(!valid(offWheel), "blue's hue must be on the wheel it is blue's hue of");

    // A fade that runs the wrong way round is not a fade with a sign, it is a
    // profile that means the opposite of what this code will do with it: the
    // colour residual would be whole at saturation zero, where the face is
    // white and there is no colour, and nothing at all at full saturation,
    // where the whole correction is. Refused rather than flipped - a file
    // silently reinterpreted is a file nobody can reason about.
    Profile reversed = good;
    reversed.satZero = good.satFull;
    reversed.satFull = good.satZero;
    ok(reversed.satZero != reversed.satFull, "the reversed fade still has a span");
    ok(!valid(reversed), "a fade that runs backwards is refused");

    Profile ordered = good;
    ok(ordered.satZero < ordered.satFull, "and the shipped one runs forwards");

    Profile high = good;
    high.satFull = 101;
    ok(!valid(high), "a fade whose top is not a saturation is refused");

    Profile edge = good;
    edge.satZero = 0;
    edge.satFull = 100;
    ok(valid(edge), "0..100 is the whole range and is allowed");

    ok(!evaluate(broken, 1.0, 0, 100, answer), "and evaluate says no to all of it");
    ok(!evaluate(good, 0.0, 0, 100, answer), "zero lux is not a room, it is a floor");
    ok(!evaluate(good, -1.0, 0, 100, answer), "nor is negative light");
}

/** Sat 0 is one answer, whatever hue is stored beside it. */
static void whiteIsOneAnswer(const Profile &profile)
{
    Answer first{};
    ok(evaluate(profile, 0.5, 0, 0, first), "white evaluates");
    for (int hue = 0; hue < 360; hue += 13)
    {
        Answer answer{};
        evaluate(profile, 0.5, (uint16_t)hue, 0, answer);
        ok(answer.percent == first.percent,
           "white at hue " + std::to_string(hue) + " is the same percentage");
        ok(answer.residual == 0.0, "and carries no colour residual");
    }
}

/** The vectors: the whole model, case by case. */
static void theVectors(const Profile &profile, const std::string &path)
{
    Reader in(path);
    int count = in.integer();
    ok(count > 0, "there are vectors to run");

    int worst = 0;
    for (int i = 0; i < count; i++)
    {
        double lux = in.number();
        int hue = in.integer();
        int sat = in.integer();
        int percent = in.integer();
        double target = in.number();
        int limited = in.integer();
        int clamped = in.integer();

        Answer answer{};
        bool answered = evaluate(profile, lux, (uint16_t)hue, (uint8_t)sat, answer);

        char what[160];
        std::snprintf(what, sizeof(what), "case %d: %g lx, hue %d, sat %d",
                      i, lux, hue, sat);
        std::string where(what);

        ok(answered, where + " is answered");
        if (!answered) continue;

        if (answer.percent != percent)
        {
            std::snprintf(what, sizeof(what), " - %d %% against Python's %d %%",
                          answer.percent, percent);
            where += what;
        }
        ok(answer.percent == percent, where);
        ok(std::fabs(answer.target - target) < 1e-9,
           where + " target " + std::to_string(answer.target)
                 + " against " + std::to_string(target));
        ok((int)answer.limited == limited, where + " limited");
        ok((int)answer.clamped == clamped, where + " clamped");

        int off = std::abs((int)answer.percent - percent);
        if (off > worst) worst = off;
    }
    std::printf("%d vectors, worst percentage disagreement %d\n", count, worst);
}

int main(int argc, char **argv)
{
    std::string base = (argc > 1) ? argv[1] : "tests/golden";
    Profile profile = loadProfile(base + "/factory_profile.txt");

    theProfileLoads(profile);
    theConeRisesWithLightEverywhereOnTheWheel(profile);
    blueWeightIsAWellBehavedBump(profile);
    theGammaMatchesTheDriver();
    whiteIsWhiteAtEveryHue();
    refusals(profile);
    whiteIsOneAnswer(profile);
    theVectors(profile, base + "/factory_vectors.txt");

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
