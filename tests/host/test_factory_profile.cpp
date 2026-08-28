/**
 * The firmware evaluator, against the model that produced the profile.
 *
 * `src/FactoryProfile.cpp` is compiled here by a desktop compiler - no
 * Arduino, no ArduinoJson, no clock - and asked the same questions
 * `scripts/factory_luminance.py` was asked when it wrote
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

static double statedDip = -1.0;

static Profile loadProfile(const std::string &path)
{
    Reader in(path);
    Profile profile{};
    profile.percentMin = (uint8_t)in.integer();
    profile.percentMax = (uint8_t)in.integer();
    profile.satZero = (uint8_t)in.integer();
    profile.satFull = (uint8_t)in.integer();
    profile.huePeriod = (uint16_t)in.integer();
    profile.hueCount = (uint8_t)in.integer();
    for (uint8_t i = 0; i < profile.hueCount; i++)
        profile.hueKnot[i] = (uint16_t)in.integer();
    profile.levelCount = (uint8_t)in.integer();
    for (uint8_t i = 0; i < profile.levelCount; i++)
    {
        profile.level[i].logLux = in.number();
        profile.level[i].white = in.number();
        for (uint8_t k = 0; k < profile.hueCount; k++)
            profile.level[i].residual[k] = in.number();
        for (uint8_t k = 0; k < profile.hueCount; k++)
            profile.level[i].bound[k] = (uint8_t)in.integer();
        profile.level[i].censored = (uint8_t)in.integer();
    }
    profile.driveCount = (uint8_t)in.integer();
    for (uint8_t i = 0; i < profile.driveCount; i++)
        profile.driveLevel[i] = (uint8_t)in.integer();
    for (uint8_t i = 0; i < profile.driveCount; i++)
        profile.driveResponse[i] = in.number();
    for (int i = 0; i < 3; i++) profile.weight[i] = in.number();
    statedDip = in.number();
    return profile;
}

/**
 * Whether the grid rises with light, measured on the grid rather than read out
 * of the status field beside it.
 *
 * The reviewed profile says `monotone: false` and its grid is monotone all the
 * same: the status is about the observations, where hue 240 falls a quarter of
 * a decade, and the isotonic step pools the levels that disagreement sits
 * between before anything is shipped. A firmware trusting the status field
 * would report a fault it does not have; one trusting it the other way would
 * accept a grid that really does fall - and a clock regulating on that gets
 * dimmer as the sun comes up.
 */
static void theGridRisesWithLight(const Profile &good)
{
    char what[120];
    double dip = worstDip(good);
    std::snprintf(what, sizeof(what),
                  "the shipped grid's worst dip is %.12g, Python says %.12g",
                  dip, statedDip);
    ok(std::fabs(dip - statedDip) < 1e-12, what);
    ok(dip == 0.0, "and the shipped grid does not dip at all");
    ok(valid(good), "so it is one the evaluator may act on");

    // A dip in the white line with every residual flat: a different fault from
    // one in a single hue, and one a check walking only the colour rows misses.
    Profile white = good;
    for (uint8_t i = 0; i < white.levelCount; i++)
        for (uint8_t k = 0; k < white.hueCount; k++)
            white.level[i].residual[k] = 0.0;
    white.level[2].white = white.level[1].white - 0.02;
    ok(std::fabs(worstDip(white) - 0.02) < 1e-9, "a dip in the white line is found");
    ok(!valid(white), "and refused");

    // One in a single hue, with the white line rising throughout.
    Profile hue = good;
    hue.level[3].residual[2] = hue.level[2].residual[2]
                             - (hue.level[3].white - hue.level[2].white) - 0.2;
    ok(worstDip(hue) > 0.19, "a dip in one hue is found");
    ok(!valid(hue), "and refused");
}

/* ------------------------------------------------------------------ */

static void theProfileLoads(const Profile &profile)
{
    ok(valid(profile), "the shipped profile is one the evaluator may act on");
    ok(profile.levelCount >= 2, "it has levels to interpolate between");
    ok(profile.hueCount >= 2, "it has hue knots");
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

    Profile one = good;
    one.levelCount = 1;
    ok(!valid(one), "one level is refused - there is nothing to interpolate");

    Profile backwards = good;
    double first = backwards.level[0].logLux;
    backwards.level[0].logLux = backwards.level[backwards.levelCount - 1].logLux;
    backwards.level[backwards.levelCount - 1].logLux = first;
    ok(!valid(backwards), "levels out of order are refused");

    Profile flat = good;
    flat.level[1].logLux = flat.level[0].logLux;
    ok(!valid(flat), "a repeated level is refused - the bracket would divide by zero");

    Profile broken = good;
    broken.level[0].white = std::nan("");
    ok(!valid(broken), "a value that is not a number is refused");

    Profile empty = good;
    empty.percentMin = empty.percentMax;
    ok(!valid(empty), "an empty regulated range is refused");

    Profile skew = good;
    skew.hueKnot[1] = 61;
    ok(!valid(skew), "unevenly spaced hue knots are refused");

    Profile lamp = good;
    lamp.driveLevel[1] = lamp.driveLevel[0];
    ok(!valid(lamp), "a drive table that does not descend is refused");

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

    ok(!evaluate(one, 1.0, 0, 100, answer), "and evaluate says no to all of it");
    ok(!evaluate(good, 0.0, 0, 100, answer), "zero lux is not a room, it is a floor");
    ok(!evaluate(good, -1.0, 0, 100, answer), "nor is negative light");
}

/**
 * The three fields a struct can hold nonsense in, and the loader cannot.
 *
 * `FactoryLuminance` reads the document and refuses what ArduinoJson says is
 * not a boolean or not a percentage. That is the right place for it and it is
 * not the only place it has to be, because this struct is also reached by a
 * future reader - an NVS cache, a second file format, a test - and `valid()`
 * is the only thing standing between a `Profile` and the arithmetic. A field
 * whose invariant lives exclusively in the parser is a field with no invariant
 * at all the first time somebody fills the struct another way.
 *
 * Each of these is also a *silent* wrong answer rather than a crash, which is
 * what makes it worth a check:
 *
 * - a saturation above 100 makes the fade divide by a span it never reaches,
 *   so the colour correction is applied at a fraction of its measured size for
 *   every colour the clock can actually show;
 * - a `bound` of 2 is truthy, so every answer touching that corner says "at
 *   least this much" - which the read-out shows and the user layer acts on -
 *   with nothing behind it;
 * - a `censored` of 2 is the same fault one level up.
 *
 * `scripts/factory_luminance.py:load_runtime` refuses the same three, and the
 * order there is the order here.
 */
static void flagsAreFlagsAndSaturationsAreSaturations(const Profile &good)
{
    Profile high = good;
    high.satFull = 101;
    ok(high.satZero < high.satFull, "the out-of-range fade still runs forwards");
    ok(!valid(high), "a fade whose top is not a saturation is refused");

    Profile lowEnd = good;
    lowEnd.satZero = 101;
    lowEnd.satFull = 102;
    ok(!valid(lowEnd), "and neither end may sit above a hundred");

    Profile edge = good;
    edge.satZero = 0;
    edge.satFull = 100;
    ok(valid(edge), "0..100 is the whole range and is allowed");

    Profile bound = good;
    bound.level[0].bound[0] = 2;
    ok(!valid(bound), "a bound that is not 0 or 1 is refused");

    Profile lastBound = good;
    lastBound.level[good.levelCount - 1].bound[good.hueCount - 1] = 7;
    ok(!valid(lastBound), "every corner is looked at, not just the first");

    // Beyond hueCount is not read by anything and must not be a reason to
    // refuse: the array is sized for the largest grid the firmware can hold
    // and a shorter profile leaves the tail as it was.
    Profile spare = good;
    if (good.hueCount < FACTORY_MAX_HUES)
    {
        spare.level[0].bound[FACTORY_MAX_HUES - 1] = 3;
        ok(valid(spare), "a corner outside the grid is not part of the grid");
    }

    Profile censored = good;
    censored.level[0].censored = 2;
    ok(!valid(censored), "a censored flag that is not 0 or 1 is refused");

    Profile marked = good;
    marked.level[good.levelCount - 1].censored = 1;
    ok(valid(marked), "and a level that really is censored is fine");
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
        ok(!answer.bound, "and no bound from a corner it never read");
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
        int bound = in.integer();

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
        ok((int)(answer.bound ? 1 : 0) == bound, where + " bound");

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
    theGridRisesWithLight(profile);
    theGammaMatchesTheDriver();
    whiteIsWhiteAtEveryHue();
    refusals(profile);
    flagsAreFlagsAndSaturationsAreSaturations(profile);
    whiteIsOneAnswer(profile);
    theVectors(profile, base + "/factory_vectors.txt");

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
