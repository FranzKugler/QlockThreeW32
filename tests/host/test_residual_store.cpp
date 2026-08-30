/**
 * The user layer: what a nudge is kept as, what it is allowed to replace, and
 * what refitting Table 1 and Table 2 together actually produces.
 *
 * This is the half of the automatic that learns, and it is the half where a
 * mistake is invisible on a bench: every rule here is about which of two
 * statements survives a month of ordinary use, and the failure shows up as a
 * clock that is slightly wrong in one colour, in one room, in the evening.
 *
 * `src/ResidualStore.cpp` is pure for exactly that reason - no Arduino, no
 * NVS - so a desktop compiler can ask it the questions a clock would take
 * weeks to answer. It is no longer independent of FactoryProfile (refit()
 * needs the shape Table 1 travels in and the boundary between the nose and
 * blue's line), so this harness links FactoryProfile.cpp too - see
 * tests/host/run.sh.
 *
 * Build and run:  tests/host/run.sh
 */
#include <cmath>
#include <cstdio>
#include <string>

#include "../../src/ResidualStore.h"

using namespace ResidualStore;

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

static void near(double got, double want, const std::string &what)
{
    ok(std::fabs(got - want) < 1e-6,
       what + " (got " + std::to_string(got) + ", wanted " + std::to_string(want) + ")");
}

/**
 * A small, valid profile with no Table 1 points of its own by default - each
 * refit test adds exactly the points its story needs. Cone and nose values
 * are arbitrary but real (never zero, never equal to each other) so a test
 * that forgets to seed one half of the fit fails loudly rather than by
 * accident matching a placeholder.
 */
static FactoryProfile::Profile makeProfile()
{
    FactoryProfile::Profile p{};
    p.huePeriod = 360;
    p.coneSlope = 0.47; p.coneOffset = -0.73; p.logLuxMin = -2.0; p.logLuxMax = 1.0;
    p.noseA0 = -0.25; p.noseA1 = -0.22; p.noseB1 = 0.18;
    p.blueHue = 240; p.blueSlope = -0.19; p.blueOffset = -0.61; p.blendHalfWidth = 45.0;
    p.percentMin = 20; p.percentMax = 100; p.satZero = 0; p.satFull = 100;
    p.driveCount = 2;
    p.driveLevel[0] = 255; p.driveLevel[1] = 4;
    p.driveResponse[0] = 1.0; p.driveResponse[1] = 0.003;
    p.weight[0] = 0.2126; p.weight[1] = 0.7152; p.weight[2] = 0.0722;
    p.pointCount = 0;
    return p;
}

/* ------------------------------------------------------------------
 * The colour a correction was made in
 * ---------------------------------------------------------------- */

static void whiteIsWhiteWhateverHueIsBesideIt()
{
    // At saturation zero the driver emits 255,255,255 whatever the hue byte
    // says, so two "white" corrections with different hues stored beside them
    // are the same statement - and one must replace the other rather than
    // filling two of the eight slots with the same fact.
    ok(canonicalHue(0, 0) == canonicalHue(240, 0),
       "hue is not part of white's identity");
    ok(sameColour(0, 0, 240, 0), "two whites are the same colour");
    ok(!sameColour(0, 100, 240, 100), "two saturated colours are not");

    Store store{};
    add(store, -0.5, 0.10, 0, 0, 1, false);
    add(store, -0.5, 0.20, 240, 0, 2, false);
    ok(store.count == 1, "the second white replaced the first");
    near(store.at[0].decades, 0.20, "and it is the newer one that is kept");
}

static void distinctColoursDoNotReplaceEachOther()
{
    // The headline case: somebody corrects the brightness one evening in
    // green and another in blue, in the same room at the same time of day.
    // Those are two statements about two different amounts of light, and
    // neither is an update of the other.
    Store store{};
    add(store, -0.5, 0.10, 120, 100, 1, false);
    add(store, -0.5, -0.08, 240, 100, 2, false);
    ok(store.count == 2, "same light, different colour: both kept");

    // And a colour a little way off is still a different colour. A threshold
    // wide enough to call 120 and 150 "the same" is wide enough to lose one of
    // them, and the model has no way of telling afterwards that it did.
    add(store, -0.5, 0.03, 150, 100, 3, false);
    ok(store.count == 3, "thirty degrees apart is not the same colour");

    // The same colour said again *is* an update, though, and that is the whole
    // reason a replacement rule exists.
    add(store, -0.5, 0.30, 120, 100, 4, false);
    ok(store.count == 3, "the same colour at the same light replaced");
    bool found = false;
    for (uint8_t i = 0; i < store.count; i++)
    {
        if (store.at[i].hue == 120 && std::fabs(store.at[i].decades - 0.30) < 1e-9) found = true;
    }
    ok(found, "and it is the newer value that is kept");
}

static void aDifferentLightIsADifferentStatement()
{
    Store store{};
    add(store, -1.7, 0.10, 120, 100, 1, false);
    add(store, 0.3, 0.20, 120, 100, 2, false);
    ok(store.count == 2, "same colour, different light: both kept");
}

static void theOldestLeavesWhenItIsFull()
{
    Store store{};
    for (int i = 0; i < RESIDUAL_MAX + 2; i++)
    {
        // Far enough apart in light that none of them replaces another.
        add(store, -2.0 + i * 0.5, 0.01 * i, 120, 100, (uint32_t)i, false);
    }
    ok(store.count == RESIDUAL_MAX, "it holds no more than it says it does");
    ok(store.at[0].seconds == 2, "and the two oldest left");
}

static void forgettingOneKeepsTheOrder()
{
    Store store{};
    add(store, -2.0, 0.1, 120, 100, 1, false);
    add(store, -1.0, 0.2, 120, 100, 2, false);
    add(store, 0.0, 0.3, 120, 100, 3, false);
    ok(forget(store, 1), "the middle one goes");
    ok(store.count == 2, "and only it");
    ok(store.at[0].seconds == 1 && store.at[1].seconds == 3,
       "the order is still the order things happened");
    ok(!forget(store, 9), "a place that is not there is refused");
}

/* ------------------------------------------------------------------
 * refit(): Table 1 and Table 2, combined
 * ---------------------------------------------------------------- */

static void refitRefusesAnInvalidProfile()
{
    FactoryProfile::Profile p = makeProfile();
    p.coneSlope = 0.0;   // a cone that does not rise: FactoryProfile::valid() says no
    Store empty{};
    Fit fit{};
    ok(!refit(p, empty, fit), "an invalid profile is refused");
}

static void withNothingTaughtTheFactoryNumbersStand()
{
    FactoryProfile::Profile p = makeProfile();
    p.point[0] = {240, 100, -1.0, 0.0};
    p.pointCount = 1;   // one point is not enough to refit either half

    Store empty{};
    Fit fit{};
    ok(refit(p, empty, fit), "refits");
    near(fit.noseA0, p.noseA0, "too little to refit the nose: kept as shipped");
    near(fit.noseA1, p.noseA1, "same for its cosine term");
    near(fit.noseB1, p.noseB1, "and its sine term");
    near(fit.blueSlope, p.blueSlope, "and blue's line: kept as shipped");
    near(fit.blueOffset, p.blueOffset, "same for its offset");
}

static void enoughTaughtPointsMoveBluesLine()
{
    FactoryProfile::Profile p = makeProfile();   // no Table 1 points at all
    Store taught{};
    // Two statements at hue 240 that define a line of their own: slope 0.2,
    // offset 0.2.
    add(taught, -1.0, 0.0, 240, 100, 1, false);
    add(taught, 1.0, 0.4, 240, 100, 2, false);

    Fit fit{};
    ok(refit(p, taught, fit), "refits");
    near(fit.blueSlope, 0.2, "blue's line comes entirely from what was taught");
    near(fit.blueOffset, 0.2, "same for its offset");
}

static void theNoseRefitsFromEnoughTaughtPoints()
{
    FactoryProfile::Profile p = makeProfile();
    Store taught{};
    // Three hues, well clear of the blend window, all saying the same
    // residual - a flat nose, a0 = -0.5, nothing on cos or sin.
    add(taught, -0.5, -0.5, 0, 100, 1, false);
    add(taught, -0.5, -0.5, 60, 100, 2, false);
    add(taught, -0.5, -0.5, 120, 100, 3, false);

    Fit fit{};
    ok(refit(p, taught, fit), "refits");
    near(fit.noseA0, -0.5, "three points at one flat value refit a flat nose");
    near(fit.noseA1, 0.0, "no cosine term is needed to explain a flat value");
    near(fit.noseB1, 0.0, "nor a sine term");
}

static void aTaughtPointShadowsAndReplacesABadFactoryPoint()
{
    // Two factory points that agree on a clean line (slope 0.2, offset 0.2),
    // and a third that does not - the case a stale or noisy factory
    // measurement is supposed to look like on the wall.
    FactoryProfile::Profile p = makeProfile();
    p.point[0] = {240, 100, -1.0, 0.0};
    p.point[1] = {240, 100, 1.0, 0.4};
    p.point[2] = {245, 100, 0.5, 5.0};
    p.pointCount = 3;

    Store empty{};
    Fit before{};
    ok(refit(p, empty, before), "refits with nothing taught yet");
    ok(before.blueSlope > 0.5,
       "the outlier alone drags the untaught fit far from the true slope");

    // The owner corrects exactly that point - same light, same colour, the
    // value the true line actually predicts there.
    Store taught{};
    add(taught, 0.5, 0.3, 245, 100, 1, false);
    Fit after{};
    ok(refit(p, taught, after), "refits again with the correction taught");
    near(after.blueSlope, 0.2, "shadowing the bad factory point recovers the true slope");
    near(after.blueOffset, 0.2, "and the true offset");
}

static void aFarTaughtPointDoesNotShadowAnything()
{
    FactoryProfile::Profile p = makeProfile();
    p.point[0] = {240, 100, -1.0, 0.0};
    p.point[1] = {240, 100, 1.0, 0.4};
    p.pointCount = 2;

    Store taught{};
    // A different colour entirely, nowhere near either factory point in hue
    // or in light.
    add(taught, -1.0, 999.0, 60, 100, 1, false);

    Fit fit{};
    ok(refit(p, taught, fit), "refits");
    near(fit.blueSlope, 0.2, "blue's line is exactly the two factory points");
    near(fit.blueOffset, 0.2, "the far-away correction does not touch it");
}

static void aBoundTaughtPointShadowsButDoesNotEnterTheFit()
{
    // The two rules at once: "at least this much" is still evidence that the
    // factory point there is wrong (it shadows), but it is not itself read
    // as an equality (it does not enter the regression).
    FactoryProfile::Profile p = makeProfile();
    p.point[0] = {240, 100, -1.0, 0.0};
    p.point[1] = {240, 100, 1.0, 0.4};
    p.point[2] = {245, 100, 0.5, 5.0};
    p.pointCount = 3;

    Store taught{};
    add(taught, 0.5, 999.0, 245, 100, 1, true);   // bound, and an absurd value

    Fit fit{};
    ok(refit(p, taught, fit), "refits");
    near(fit.blueSlope, 0.2,
        "the bound shadows the bad factory point without polluting the fit");
    near(fit.blueOffset, 0.2, "leaving exactly the two good points");
}

static void aWhiteTaughtPointTakesNoPartInEitherFit()
{
    FactoryProfile::Profile p = makeProfile();
    p.point[0] = {240, 100, -1.0, 0.0};
    p.point[1] = {240, 100, 1.0, 0.4};
    p.pointCount = 2;

    Store taught{};
    add(taught, -1.0, 999.0, 0, 0, 1, false);   // white, canonicalised hue 0

    Fit fit{};
    ok(refit(p, taught, fit), "refits");
    near(fit.blueSlope, 0.2, "white shadows nothing and feeds no fit");
    near(fit.blueOffset, 0.2, "blue's line is untouched");
    near(fit.noseA0, p.noseA0, "neither is the nose");
}

int main()
{
    whiteIsWhiteWhateverHueIsBesideIt();
    distinctColoursDoNotReplaceEachOther();
    aDifferentLightIsADifferentStatement();
    theOldestLeavesWhenItIsFull();
    forgettingOneKeepsTheOrder();

    refitRefusesAnInvalidProfile();
    withNothingTaughtTheFactoryNumbersStand();
    enoughTaughtPointsMoveBluesLine();
    theNoseRefitsFromEnoughTaughtPoints();
    aTaughtPointShadowsAndReplacesABadFactoryPoint();
    aFarTaughtPointDoesNotShadowAnything();
    aBoundTaughtPointShadowsButDoesNotEnterTheFit();
    aWhiteTaughtPointTakesNoPartInEitherFit();

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
