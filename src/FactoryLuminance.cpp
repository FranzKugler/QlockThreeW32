/**
 * FactoryLuminance
 * See FactoryLuminance.h for what is checked and in which order.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.2
 * @created  27.8.2026
 * @updated  27.8.2026
 */
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <mbedtls/sha256.h>

#include "FactoryLuminance.h"
#include "LogBuffer.h"

// Beside the curve and the residuals, in the namespace the automatic already
// owns. What is stored is an identity, not a model: see record().
#define FACTORY_NAMESPACE "qlocklight"
#define FACTORY_KEY       "factory"

namespace
{
    FactoryProfile::Profile model_;
    bool loaded_ = false;
    String error_ = "factoryMissing";

    String profileId_, stackId_, checksum_, recorded_;
    bool monotone_ = true;
    double gridDip_ = 0.0;
    bool acceptanceMet_ = false;
    int maxError_ = -1;
    int worstHue_ = -1;

    /**
     * Names why a candidate was not taken, and changes nothing else.
     *
     * Deliberately does **not** clear `loaded_` or touch the model. begin() may
     * be called again on a clock that is already regulating - after a
     * filesystem update, say - and a refusal there must leave the profile in
     * use exactly where it was rather than dropping the clock onto the white
     * curve because a *replacement* was bad. On the first call there is
     * nothing to leave, and `loaded_` is already false.
     */
    bool refuse(const char *code, const char *why)
    {
        error_ = code;
        debugW("Factory profile refused (%s): %s", code, why);
        return false;
    }

    /** Lower-case hex of the SHA-256 of a byte range. */
    String digestOf(const uint8_t *bytes, size_t length)
    {
        uint8_t out[32];
        mbedtls_sha256_context ctx;
        mbedtls_sha256_init(&ctx);
        mbedtls_sha256_starts(&ctx, 0);
        mbedtls_sha256_update(&ctx, bytes, length);
        mbedtls_sha256_finish(&ctx, out);
        mbedtls_sha256_free(&ctx);

        String hex;
        hex.reserve(64);
        for (int i = 0; i < 32; i++)
        {
            char pair[3];
            snprintf(pair, sizeof(pair), "%02x", out[i]);
            hex += pair;
        }
        return hex;
    }

    // ------------------------------------------------------------------
    // Reading a field, rather than asking for one
    //
    // `doc["percentRange"]["min"].as<int>()` answers 0 for a missing key, for
    // a null, for a string, for a boolean and for a float - five different
    // faults, all arriving as a number that looks like data and regulates a
    // clock. Every field below is therefore *tested* for its type and its
    // range before it is taken, and a field that fails is a refusal rather
    // than a default.
    //
    // The range half matters as much as the type. A percentage of 4000 is a
    // valid JSON integer and is not a percentage, and it would be truncated
    // into a uint8_t on the way into the struct - which is how a nonsense
    // value becomes a plausible one.
    // ------------------------------------------------------------------

    /** A number the arithmetic may touch. JSON has no NaN, so one arrives as
     *  a string or a null - both of which parse to zero and look like data. */
    bool number(JsonVariantConst value, double &out)
    {
        if (value.isNull() || value.is<bool>() || value.is<const char *>()) return false;
        if (!value.is<double>() && !value.is<int>()) return false;
        double found = value.as<double>();
        if (isnan(found) || isinf(found)) return false;
        out = found;
        return true;
    }

    /** A whole number inside the range it is allowed to be in. */
    bool whole(JsonVariantConst value, long low, long high, long &out)
    {
        if (value.isNull() || value.is<bool>() || value.is<const char *>()) return false;
        if (!value.is<long>() && !value.is<int>()) return false;
        long found = value.as<long>();
        if (found < low || found > high) return false;
        out = found;
        return true;
    }

    /** A boolean, and not the several things that convert to one.
     *  `flag`, not `boolean`: Arduino.h typedefs that name to `bool`. */
    bool flag(JsonVariantConst value, bool &out)
    {
        if (!value.is<bool>()) return false;
        out = value.as<bool>();
        return true;
    }

    /** A string with something in it. */
    bool text(JsonVariantConst value, String &out)
    {
        if (!value.is<const char *>()) return false;
        out = value.as<String>();
        return out.length() > 0;
    }
}

bool FactoryLuminance::begin()
{
    // Nothing about the profile in use is cleared here, and that is the whole
    // shape of this function: every check below works on a *candidate*, and
    // the model, the identity and the status are replaced together at the very
    // end or not at all. Half-replacing them would leave a clock regulating on
    // one measurement while reporting the identity of another - and on a
    // reload it would drop a working profile because its replacement was bad.

    // What the residuals were learned against, read whether or not a profile
    // loads: a clock whose profile has gone missing still has to be able to
    // say which one its corrections belong to.
    Preferences preferences;
    if (preferences.begin(FACTORY_NAMESPACE, true))
    {
        recorded_ = preferences.getString(FACTORY_KEY, "");
        preferences.end();
    }

    if (!LittleFS.exists(FACTORY_PATH))
    {
        return refuse("factoryMissing",
                      "no " FACTORY_PATH " in the filesystem image - it "
                      "predates the colour-aware model");
    }

    File file = LittleFS.open(FACTORY_PATH, "r");
    if (!file) return refuse("factoryUnreadable", "the file will not open");

    size_t size = file.size();
    if (size > FACTORY_MAX_BYTES || size <= sizeof(FACTORY_HEAD))
    {
        file.close();
        return refuse("factoryTooBig", "the file is not the size a profile is");
    }

    // `raw`, not `text` - there is a text() reader in this file and a local
    // of that name would shadow it exactly where the shadowing is invisible.
    String raw;
    raw.reserve(size + 1);
    while (file.available()) raw += (char)file.read();
    file.close();

    // 1. The layout, before anything is trusted to be where it looks.
    const size_t headLength = strlen(FACTORY_HEAD);
    const size_t markLength = strlen(FACTORY_MARK);
    const size_t payloadAt = headLength + 64 + markLength;
    if (!raw.startsWith(FACTORY_HEAD) || raw.length() <= payloadAt + 1
        || !raw.endsWith("}")
        || raw.substring(headLength + 64, payloadAt) != FACTORY_MARK)
    {
        return refuse("factoryLayout",
                      "the file does not have the layout the checksum rests on");
    }

    // 2. The checksum, over exactly the payload substring.
    String stated = raw.substring(headLength, headLength + 64);
    String body = raw.substring(payloadAt, raw.length() - 1);
    if (digestOf((const uint8_t *)body.c_str(), body.length()) != stated)
    {
        return refuse("factoryChecksum",
                      "the file does not match its own checksum: it has been "
                      "edited or truncated since it was generated");
    }

    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok)
    {
        return refuse("factoryUnreadable", "the payload is not readable JSON");
    }

    // 3. The schema, the model, the stack. Each read as the type it has to be:
    //    a schema field carrying the string "1" is not schema 1, it is a file
    //    written by something else.
    long schema = 0;
    if (!whole(doc["runtimeSchema"], FACTORY_RUNTIME_SCHEMA, FACTORY_RUNTIME_SCHEMA, schema)
        || !whole(doc["schemaVersion"], FACTORY_SCHEMA, FACTORY_SCHEMA, schema))
    {
        return refuse("factorySchema",
                      "this profile is a schema this firmware does not read");
    }
    String model, stack;
    if (!text(doc["modelId"], model) || model != FACTORY_MODEL_ID)
    {
        return refuse("factoryModel",
                      "this profile is a model this firmware does not evaluate");
    }
    if (!text(doc["stackId"], stack))
    {
        return refuse("factoryModel",
                      "this profile names no optical stack, so nothing can be "
                      "matched against a clock");
    }
    String fade;
    if (!text(doc["satFade"]["kind"], fade) || fade != "linear")
    {
        return refuse("factoryModel",
                      "this profile fades the colour residual by a curve this "
                      "firmware does not know");
    }

    // 4. The shape, into a struct of its own. Nothing is written into the
    //    model in use until every field has been read: a refusal half way
    //    through must leave the clock on what it had, not between two
    //    profiles - which is a state nothing here can describe.
    FactoryProfile::Profile built{};
    long value = 0;

    JsonArrayConst knots = doc["hueKnots"];
    if (knots.isNull() || knots.size() < 2 || knots.size() > FACTORY_MAX_HUES)
    {
        return refuse("factoryShape", "the hue knots are not a row this can hold");
    }
    built.hueCount = (uint8_t)knots.size();
    if (!whole(doc["huePeriod"], 1, 3600, value))
    {
        return refuse("factoryShape", "the hue period is not a whole number of degrees");
    }
    built.huePeriod = (uint16_t)value;
    uint8_t at = 0;
    for (JsonVariantConst knot : knots)
    {
        if (!whole(knot, 0, built.huePeriod - 1, value))
        {
            return refuse("factoryShape", "a hue knot is not a hue");
        }
        built.hueKnot[at++] = (uint16_t)value;
    }

    if (!whole(doc["percentRange"]["min"], 0, 100, value))
    {
        return refuse("factoryShape", "the bottom of the range is not a percentage");
    }
    built.percentMin = (uint8_t)value;
    if (!whole(doc["percentRange"]["max"], 0, 100, value))
    {
        return refuse("factoryShape", "the top of the range is not a percentage");
    }
    built.percentMax = (uint8_t)value;

    if (!whole(doc["satFade"]["zeroAtSat"], 0, 100, value))
    {
        return refuse("factoryShape", "the bottom of the fade is not a saturation");
    }
    built.satZero = (uint8_t)value;
    if (!whole(doc["satFade"]["fullAtSat"], 0, 100, value))
    {
        return refuse("factoryShape", "the top of the fade is not a saturation");
    }
    built.satFull = (uint8_t)value;

    JsonArrayConst levels = doc["levels"];
    if (levels.isNull() || levels.size() < 2 || levels.size() > FACTORY_MAX_LEVELS)
    {
        return refuse("factoryShape", "the ambient levels are not a grid this can hold");
    }
    built.levelCount = (uint8_t)levels.size();
    uint8_t index = 0;
    for (JsonObjectConst level : levels)
    {
        FactoryProfile::Level &into = built.level[index++];
        if (!number(level["logLux"], into.logLux) || !number(level["white"], into.white))
        {
            return refuse("factoryShape", "a level carries something that is not a number");
        }
        JsonArrayConst residuals = level["residuals"];
        JsonArrayConst bounds = level["bounds"];
        if (residuals.isNull() || bounds.isNull()
            || residuals.size() != built.hueCount || bounds.size() != built.hueCount)
        {
            return refuse("factoryShape",
                          "a level's residuals do not match the hue knots");
        }
        uint8_t k = 0;
        for (JsonVariantConst one : residuals)
        {
            if (!number(one, into.residual[k++]))
            {
                return refuse("factoryShape", "a residual is not a number");
            }
        }
        k = 0;
        for (JsonVariantConst one : bounds)
        {
            // A boolean, and nothing that merely converts to one. Read as
            // "anything truthy" a string would become a bound and every answer
            // touching that corner would say "at least" without cause.
            //
            // `flag()` rather than `whole(one, 0, 1, ...)`, which was the first
            // version and is exactly wrong in both directions: `whole()`
            // refuses `is<bool>()` on purpose, so it accepted the 1 and 0
            // nothing writes and refused the `true` and `false` the generator
            // does. That is a file the project ships and the clock will not
            // load - and it fails as `factoryShape` on a clock, once, where
            // nothing here would ever have seen it. The generator's
            // `load_runtime` refuses the same shapes; see _exact_flag() there.
            bool marked = false;
            if (!flag(one, marked))
            {
                return refuse("factoryShape", "a bound flag is not a flag");
            }
            into.bound[k++] = marked ? 1 : 0;
        }
        bool censored = false;
        if (!flag(level["censored"], censored))
        {
            return refuse("factoryShape", "a level does not say whether it is censored");
        }
        into.censored = censored ? 1 : 0;
    }

    JsonArrayConst driveLevels = doc["drive"]["levels"];
    JsonArrayConst driveResponse = doc["drive"]["response"];
    if (driveLevels.isNull() || driveResponse.isNull()
        || driveLevels.size() != driveResponse.size()
        || driveLevels.size() == 0 || driveLevels.size() > FACTORY_MAX_DRIVE)
    {
        return refuse("factoryShape", "the drive table is not one this can hold");
    }
    built.driveCount = (uint8_t)driveLevels.size();
    at = 0;
    for (JsonVariantConst one : driveLevels)
    {
        if (!whole(one, 0, 255, value))
        {
            return refuse("factoryShape", "a drive level is not an eight-bit value");
        }
        built.driveLevel[at++] = (uint8_t)value;
    }
    at = 0;
    for (JsonVariantConst one : driveResponse)
    {
        if (!number(one, built.driveResponse[at++]))
        {
            return refuse("factoryShape", "a drive response is not a number");
        }
    }

    JsonArrayConst weights = doc["weights"];
    if (weights.isNull() || weights.size() != 3)
    {
        return refuse("factoryShape", "the luminance weights are not three numbers");
    }
    at = 0;
    for (JsonVariantConst one : weights)
    {
        if (!number(one, built.weight[at++]))
        {
            return refuse("factoryShape", "a luminance weight is not a number");
        }
    }

    // The same list scripts/factory_luminance.py refuses on, in the same order,
    // and it ends with the one check that needs every number already known to
    // be a number: whether the grid rises with light. Measured on the grid,
    // never read out of `status` - see FactoryProfile::worstDip().
    double dip = FactoryProfile::worstDip(built);
    if (!FactoryProfile::valid(built))
    {
        if (dip > FACTORY_MAX_DIP)
        {
            return refuse("factoryNotMonotone",
                          "the grid falls between two ambient levels, so a "
                          "clock regulating on it would get dimmer as the sun "
                          "came up");
        }
        return refuse("factoryShape",
                      "the grid is not one the evaluator may walk: levels out "
                      "of order, knots unevenly spaced, or a range with no span");
    }

    // The identity, before anything is committed. A profile that cannot say
    // which measurement it is has to be refused rather than loaded namelessly:
    // the corrections in NVS are keyed by that checksum, so an empty one would
    // make every stored correction look as though it belonged to this profile,
    // whatever it was really learned on. That is the one failure the
    // checksum-keyed record exists to prevent.
    String candidateId, candidateChecksum;
    if (!text(doc["profileId"], candidateId))
    {
        return refuse("factoryModel",
                      "this profile has no id, so nothing can name which "
                      "measurement the clock is running");
    }
    if (!text(doc["sourceChecksum"], candidateChecksum))
    {
        return refuse("factoryModel",
                      "this profile carries no source checksum, and the stored "
                      "corrections are keyed by it - without one they would all "
                      "look as though they belonged to this profile");
    }

    // The status, also into locals. It is read leniently on purpose: a missing
    // acceptance figure is a profile built before the evaluation was attached,
    // not a broken one, and refusing it would withhold a good model over a
    // note about it.
    JsonObjectConst status = doc["status"];
    bool candidateMonotone = true;
    bool candidateAccepted = false;
    long statusValue = 0;
    if (!flag(status["monotone"], candidateMonotone)) candidateMonotone = true;
    if (!flag(status["acceptanceMet"], candidateAccepted)) candidateAccepted = false;
    int candidateMaxError = whole(status["maxError"], 0, 100, statusValue)
                          ? (int)statusValue : -1;
    int candidateWorstHue = whole(status["worstHue"], 0, 359, statusValue)
                          ? (int)statusValue : -1;

    // Only now, and all of it together. Everything above either succeeded or
    // left the clock exactly as it was.
    model_ = built;
    gridDip_ = dip;
    loaded_ = true;
    error_ = "";
    profileId_ = candidateId;
    stackId_ = stack;
    checksum_ = candidateChecksum;
    monotone_ = candidateMonotone;
    acceptanceMet_ = candidateAccepted;
    maxError_ = candidateMaxError;
    worstHue_ = candidateWorstHue;

    // The status travels with the model and is never folded into an answer.
    // Two things in it read alike and are not: `monotone` is about the
    // **observations** the profile was fitted to, where one hue falls a
    // quarter of a decade, and the grid is monotone all the same because the
    // isotonic step pooled the levels that disagreement sits between. The
    // clock reports both and takes neither on trust - gridDip_ above is
    // measured, not read.

    debugA("Factory profile %s (%s): %d levels, %d hues, %d..%d %%%s",
           profileId_.c_str(), stackId_.c_str(), model_.levelCount,
           model_.hueCount, model_.percentMin, model_.percentMax,
           acceptanceMet_ ? "" : ", acceptance goal not met");
    if (!monotone_)
    {
        debugI("Factory profile: the observations behind it are not monotone; "
               "the grid that was shipped is (worst dip %.2e)", gridDip_);
    }
    return true;
}

bool FactoryLuminance::available() { return loaded_; }
const char *FactoryLuminance::error() { return error_.c_str(); }
const char *FactoryLuminance::profileId() { return profileId_.c_str(); }
const char *FactoryLuminance::stackId() { return stackId_.c_str(); }
const char *FactoryLuminance::sourceChecksum() { return checksum_.c_str(); }
bool FactoryLuminance::observationsMonotone() { return monotone_; }
double FactoryLuminance::gridDip() { return gridDip_; }
bool FactoryLuminance::gridMonotone() { return loaded_ && gridDip_ <= FACTORY_MAX_DIP; }
bool FactoryLuminance::acceptanceMet() { return acceptanceMet_; }
int FactoryLuminance::maxError() { return maxError_; }
int FactoryLuminance::worstHue() { return worstHue_; }
const FactoryProfile::Profile &FactoryLuminance::profile() { return model_; }

bool FactoryLuminance::evaluate(float lux, uint16_t hue, uint8_t sat,
                                FactoryProfile::Answer &out)
{
    if (!loaded_) return false;
    return FactoryProfile::evaluate(model_, (double)lux, hue, sat, out);
}

bool FactoryLuminance::record()
{
    if (!loaded_) return false;

    Preferences preferences;
    if (!preferences.begin(FACTORY_NAMESPACE, false))
    {
        debugE("Factory profile: cannot open NVS");
        return false;
    }
    if (preferences.getString(FACTORY_KEY, "") != checksum_)
    {
        preferences.putString(FACTORY_KEY, checksum_);
    }
    preferences.end();
    recorded_ = checksum_;
    return true;
}

const char *FactoryLuminance::recordedChecksum() { return recorded_.c_str(); }
