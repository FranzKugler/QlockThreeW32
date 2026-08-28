/**
 * The user layer: what a nudge is kept as, and what it is allowed to replace.
 *
 * This is the half of the automatic that learns, and it is the half where a
 * mistake is invisible on a bench: every rule here is about which of two
 * statements survives a month of ordinary use, and the failure shows up as a
 * clock that is slightly wrong in one colour, in one room, in the evening.
 *
 * `src/ResidualStore.cpp` is pure for exactly that reason - no Arduino, no
 * NVS - so a desktop compiler can ask it the questions a clock would take
 * weeks to answer.
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
    ok(std::fabs(got - want) < 1e-9,
       what + " (got " + std::to_string(got) + ", wanted " + std::to_string(want) + ")");
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

/* ------------------------------------------------------------------
 * A nudge that ran out of slider
 * ---------------------------------------------------------------- */

static void theCeilingIsALowerBound()
{
    // Somebody who drags the slider to the top has said "at least this much",
    // not "exactly this much" - the slider had nothing above it to offer. Read
    // as an equality it drags the model down, and the room where that happens
    // is the bright one, where being too dim is worst.
    Store store{};
    add(store, -0.5, 0.10, 120, 100, 1, true);
    ok(store.at[0].bound == 1, "a nudge at the ceiling is kept as a bound");

    add(store, 0.5, 0.10, 120, 100, 2, false);
    ok(store.at[1].bound == 0, "and one below it is exact");
}

static void aBoundNeverPullsTheModelDown()
{
    double weight = 0.0;

    // An exact correction says +0.20. A censored one at the same light and
    // colour says "at least +0.05" - which is not a contradiction, it is a
    // weaker statement, and averaging the two would answer +0.125 and make the
    // clock dimmer than the one thing anybody actually measured.
    Store store{};
    add(store, -0.5, 0.20, 120, 100, 1, false);
    add(store, -0.45, 0.05, 240, 100, 2, true);
    double found = bias(store, -0.5, 120, 100, weight);
    ok(weight > 0.0, "something applies here");
    near(found, 0.20, "the bound did not drag the answer down");
}

static void aBoundStillRaisesTheModel()
{
    // The other direction is the whole point of keeping it. "At least +0.40"
    // beside an exact +0.10 means the model is too dim, and the bound is the
    // only evidence of that there will ever be.
    double weight = 0.0;
    Store store{};
    add(store, -0.5, 0.10, 120, 100, 1, false);
    add(store, -0.45, 0.40, 120, 100, 2, true);
    // Same colour and near enough in light, so the second replaced the first -
    // put them a decade apart instead so both apply.
    Store apart{};
    add(apart, -0.5, 0.10, 120, 100, 1, false);
    add(apart, -0.45, 0.40, 240, 100, 2, true);
    double found = bias(apart, -0.5, 180, 100, weight);
    ok(found > 0.10, "a bound above the exact answer raises it");
    ok(found <= 0.40 + 1e-9, "but never past what it claimed");
    ok(store.count == 1, "and the same colour at the same light still replaced");
}

static void aBoundAloneIsBetterThanNothing()
{
    // Nothing exact has ever been said here. "At least this much" is still
    // evidence, and acting on it can only make the clock brighter - which is
    // the direction the person asked for.
    double weight = 0.0;
    Store store{};
    add(store, -0.5, 0.25, 120, 100, 1, true);
    near(bias(store, -0.5, 120, 100, weight), 0.25, "a bound on its own is used");
    ok(weight > 0.0, "and says so");
}

/* ------------------------------------------------------------------
 * What the corrections say about a room they were not made in
 * ---------------------------------------------------------------- */

static void nothingNearEnoughSaysNothing()
{
    double weight = 0.0;
    Store store{};
    add(store, -1.7, 0.30, 120, 100, 1, false);
    // Three decades away, and in a colour on the other side of the wheel.
    double found = bias(store, 1.0, 300, 100, weight);
    ok(weight == 0.0, "a correction that is nowhere near says nothing");
    near(found, 0.0, "and contributes no bias");
}

static void whiteDoesNotLeakIntoAColourOrTheOtherWay()
{
    double weight = 0.0;
    Store store{};
    add(store, -0.5, 0.30, 0, 0, 1, false);          // said in white
    bias(store, -0.5, 240, 100, weight);
    ok(weight == 0.0, "a white correction says nothing about full blue");

    Store colour{};
    add(colour, -0.5, 0.30, 240, 100, 1, false);
    bias(colour, -0.5, 0, 0, weight);
    ok(weight == 0.0, "and a blue one says nothing about white");
}

static void whiteAppliesAtEveryHue()
{
    // Once the face is white the hue setting is not visible at all, so a
    // correction made in white has to apply whatever hue happens to be stored
    // beside the query.
    Store store{};
    add(store, -0.5, 0.30, 0, 0, 1, false);
    for (int hue = 0; hue < 360; hue += 37)
    {
        double weight = 0.0;
        double found = bias(store, -0.5, (uint16_t)hue, 0, weight);
        ok(weight > 0.0, "white applies at hue " + std::to_string(hue));
        near(found, 0.30, "and by the same amount");
    }
}

static void theBiasIsClampedToWhatAPreferenceCanBe()
{
    double weight = 0.0;
    Store store{};
    add(store, -0.5, 9.0, 120, 100, 1, false);
    double found = bias(store, -0.5, 120, 100, weight);
    ok(found <= RESIDUAL_MAX_DECADES + 1e-9, "an absurd correction is clamped");
    ok(found >= -RESIDUAL_MAX_DECADES - 1e-9, "in both directions");
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

int main()
{
    whiteIsWhiteWhateverHueIsBesideIt();
    distinctColoursDoNotReplaceEachOther();
    aDifferentLightIsADifferentStatement();
    theOldestLeavesWhenItIsFull();

    theCeilingIsALowerBound();
    aBoundNeverPullsTheModelDown();
    aBoundStillRaisesTheModel();
    aBoundAloneIsBetterThanNothing();

    nothingNearEnoughSaysNothing();
    whiteDoesNotLeakIntoAColourOrTheOtherWay();
    whiteAppliesAtEveryHue();
    theBiasIsClampedToWhatAPreferenceCanBe();
    forgettingOneKeepsTheOrder();

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
