/**
 * FactoryLuminance
 * Where the colour-aware model comes from, and what is checked before it is
 * believed.
 *
 * The arithmetic is in FactoryProfile, which knows nothing about this clock.
 * This is the other half: one file in the filesystem image, read once at boot,
 * and a record in NVS saying which one is installed.
 *
 * **The numbers are never compiled in.** They are measured on one clock,
 * through one diffuser, behind one mask - `stackId` names which - and a
 * guessed profile would be worse than none, because it would look exactly like
 * a measured one. So the file is either there and passes every check, or the
 * clock regulates the way it did before this existed: the learned white line
 * in Luminance, colour-blind and honest about it.
 *
 * What is checked, in this order, and why the order matters:
 *
 *   1. **The layout.** The file is the canonical serialisation of
 *      `{"checksum": ..., "payload": ...}`, and "checksum" sorts before
 *      "payload", so the payload is a contiguous substring from a fixed
 *      offset to the closing brace. Fixed, because this cannot re-canonicalise
 *      a parsed document - ArduinoJson does not sort keys - so it hashes the
 *      substring instead, and that is only honest if the substring is where it
 *      is said to be.
 *   2. **The checksum**, over exactly those bytes. Before parsing, so a
 *      truncated or edited file never reaches the number reader at all.
 *   3. **The schema, the model and the stack.** Two models can share every
 *      field name and mean different things by them.
 *   4. **The shape**: every field read as the type and the range it has to be
 *      - `as<int>()` answers 0 for a missing key, a null, a string, a boolean
 *      and a float alike, which is five faults arriving as data - then the
 *      levels ascending, the residual rows matching the knots, and last the
 *      grid **rising with light**, measured on the grid rather than read out
 *      of the status field beside it. FactoryProfile::valid() does that half,
 *      and it is the same list scripts/factory_luminance.py refuses on.
 *
 * A failure at any step leaves `available()` false and `error()` naming which
 * step, in the vocabulary the web UI already translates. It is never partial:
 * a half-loaded profile would regulate a clock against a grid whose other half
 * is zeroes.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.2
 * @created  27.8.2026
 * @updated  27.8.2026
 */
#ifndef FACTORY_LUMINANCE_H
#define FACTORY_LUMINANCE_H

#include <Arduino.h>
#include "FactoryProfile.h"

// Shipped in the filesystem image from web/public, the same way zones.json is:
// generated, committed, and never fetched at runtime. The clock must work on a
// network with no internet at all, and a model fetched on demand is a model
// that is missing on the evening it is wanted.
#define FACTORY_PATH "/factory-luminance.json"

// The layout the checksum rests on. Asserted from the Python side too - see
// RUNTIME_HEAD in scripts/factory_luminance.py, which is the same three
// strings. A file that does not begin exactly like this is refused before a
// single number is read out of it.
#define FACTORY_HEAD "{\"checksum\":{\"algorithm\":\"sha256\",\"value\":\""
#define FACTORY_MARK "\"},\"payload\":"

// A file this big is not this file. The reviewed profile is 43 KB of
// provenance and the derived one is about 3 KB; the cap is what keeps a
// mistaken upload from being read into the heap an update wants.
#define FACTORY_MAX_BYTES 16384

namespace FactoryLuminance
{
    /**
     * Reads and checks the profile. Called once, from setup().
     *
     * Never throws and never half-loads: false means the clock carries on with
     * the white curve, which is what every clock did before this existed.
     */
    bool begin();

    /** Whether there is a profile the evaluator may act on. */
    bool available();

    /**
     * Which step refused it, as a code from the same vocabulary as any other
     * failed write - `factoryMissing`, `factoryLayout`, `factoryChecksum`,
     * `factorySchema`, `factoryModel`, `factoryShape`, `factoryNotMonotone`,
     * `factoryTooBig`, `factoryUnreadable`. Empty when it loaded.
     *
     * A code rather than a sentence, because the web UI translates it and the
     * remedies differ: a missing file is a filesystem image that predates this,
     * and a failing checksum is a file somebody edited.
     */
    const char *error();

    /** Its identity, for the read-out and for the NVS record. */
    const char *profileId();
    const char *stackId();
    const char *sourceChecksum();

    /**
     * Whether the **observations** the profile was fitted to rise with light.
     *
     * False on the reviewed profile, and that says nothing about the grid it
     * was shipped with: one hue's observations fall a quarter of a decade, and
     * the isotonic step pools the levels that disagreement sits between before
     * anything is written. Reported because it is provenance - a measurement
     * that contradicted itself somewhere is worth knowing about - and kept
     * apart from gridMonotone() because reading one for the other would have
     * the clock announcing a fault it does not have.
     */
    bool observationsMonotone();

    /** Whether the grid the clock is actually running rises with light. */
    bool gridMonotone();

    /** By how much it falls where it does, in decades. Zero when it does not. */
    double gridDip();

    /** What the held-out folds made of it. */
    bool acceptanceMet();
    /** The worst held-out error in percentage points, or -1 when unmeasured. */
    int maxError();
    /** The hue that error is at, or -1. */
    int worstHue();

    /** The model itself, for the evaluator and for the surface the UI draws. */
    const FactoryProfile::Profile &profile();

    /**
     * What this profile asks for in this room at this colour.
     *
     * False when there is no profile, in which case nothing is written and the
     * caller uses the white curve.
     */
    bool evaluate(float lux, uint16_t hue, uint8_t sat,
                  FactoryProfile::Answer &out);

    /**
     * Records this profile as the installed one, in NVS.
     *
     * The identity, not the numbers: the file is in the filesystem image and a
     * copy in NVS would be a second thing to keep in step. What NVS carries is
     * *which* profile the user residuals were learned on top of - a residual
     * measured against one baseline means nothing against another, and a
     * filesystem update can change the baseline underneath them.
     */
    bool record();

    /** The profile the stored residuals were learned against, or "". */
    const char *recordedChecksum();

    // Whether the *corrections in hand* were learned on this profile is
    // deliberately **not** here: it is Luminance::residualsStale(), because it
    // is a fact about the stored corrections and not about the file. The
    // difference bites on a clock that has never corrected anything, where the
    // honest answer is "nothing disagrees" and this side of the fence could
    // only have said "no record".
}

#endif
