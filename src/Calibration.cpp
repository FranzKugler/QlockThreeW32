/**
 * Calibration - see Calibration.h for the three passes and why they are needed.
 */
#include "Calibration.h"
#include "Coupling.h"
#include "DisplayModes.h"
#include "LedDriverWS2812FastLED.h"
#include "LightSensor.h"
#include "LogBuffer.h"
#include "LabRoutes.h"

#include <ArduinoJson.h>

#define CAL_TASK_STACK 8192

// The drive levels the response table is sampled at, descending. Below 4 the
// contribution is under half a per cent of full and the counts are noise, so
// the table stops there and anything smaller is interpolated down to zero.
static const uint8_t DRIVE_LEVELS[] = { 255, 192, 128, 96, 64, 48, 32, 24, 16, 12, 8, 6, 4 };
#define DRIVE_STEPS ((uint8_t)(sizeof(DRIVE_LEVELS) / sizeof(DRIVE_LEVELS[0])))

#define ROWS 10
#define COLUMNS 11
#define CELL_COUNT (ROWS * COLUMNS)

extern LedDriverWS2812FastLED ledDriver;
extern AmbientLight ambientLight;
extern byte mode;
extern bool needsUpdateFromRtc;

namespace
{
    TaskHandle_t task = nullptr;

    volatile bool active = false;
    volatile bool stopWanted = false;
    volatile uint8_t phase_ = Calibration::IDLE;
    volatile uint16_t done_ = 0;
    volatile uint16_t total_ = 0;
    volatile float ambient_ = 0.0f;
    volatile uint8_t rung_ = 0;
    volatile uint8_t kept_ = 0;
    char error_[32] = "";

    byte modeBefore = STD_MODE_NORMAL;

    // The coarse pass, out of the task's stack: 110 floats is not a lot, but it
    // sits beside an ArduinoJson document and a FreeRTOS stack is not the heap.
    float coarse[CELL_COUNT];
    uint8_t keptRow[COUPLING_MAX_CELLS];
    uint8_t keptCol[COUPLING_MAX_CELLS];
    float keptRed[COUPLING_MAX_CELLS];
    float keptGreen[COUPLING_MAX_CELLS];
    float keptBlue[COUPLING_MAX_CELLS];
    float driveResponse[DRIVE_STEPS];

    void fail(const char *code)
    {
        strncpy(error_, code, sizeof(error_) - 1);
        error_[sizeof(error_) - 1] = '\0';
        phase_ = Calibration::FAILED;
    }

    void step(uint8_t phase, uint16_t total)
    {
        phase_ = phase;
        done_ = 0;
        total_ = total;
    }

    // ------ the strip ------

    void show()
    {
        ledDriver.showRaw();
        vTaskDelay(pdMS_TO_TICKS(CAL_SETTLE_MS));
    }

    void dark()
    {
        ledDriver.clearRaw();
        show();
    }

    void light(uint8_t row, uint8_t column, uint8_t r, uint8_t g, uint8_t b)
    {
        ledDriver.clearRaw();
        byte pixel = LedDriverWS2812FastLED::physicalFor(row, column);
        if (pixel != 255) ledDriver.setPixelRaw(pixel, CRGB(r, g, b));
        show();
    }

    void lightRow(uint8_t row)
    {
        ledDriver.clearRaw();
        for (uint8_t c = 0; c < COLUMNS; c++)
        {
            byte pixel = LedDriverWS2812FastLED::physicalFor(row, c);
            if (pixel != 255) ledDriver.setPixelRaw(pixel, CRGB(255, 255, 255));
        }
        show();
    }

    // ------ the sensor ------

    struct Reading
    {
        float lux;
        uint16_t ch0, ch1;
        uint16_t ms;
    };

    Reading measure()
    {
        Reading out;
        out.lux = ambientLight.readNow();

        LightSensor *sensor = ambientLight.device();
        out.ch0 = out.ch1 = 0;
        out.ms = 200;
        if (sensor)
        {
            sensor->readChannels(out.ch0, out.ch1);
            out.ms = sensor->integrationMs();
        }
        done_++;
        return out;
    }

    /** True when a reading is against the stop and is a floor, not a value. */
    bool saturated(const Reading &r)
    {
        uint16_t ceiling = (r.ms == 100) ? 36863 : 65535;
        return r.ch0 >= (uint16_t)(ceiling * 0.9f) || r.ch1 >= (uint16_t)(ceiling * 0.9f);
    }

    /**
     * Pins a rung and throws the first reading away.
     *
     * The gain and the integration time are pushed to the chip immediately, but
     * the conversion already in flight was started under the old ones. Using it
     * would make the rung search compare a frame against the wrong scale, which
     * is the exact class of mistake this whole pass exists to avoid.
     */
    void pin(int8_t which)
    {
        LightSensor *sensor = ambientLight.device();
        if (!sensor) return;
        sensor->pinRung(which);
        ambientLight.readNow();
    }

    // ------ the passes ------

    /** Refuses rather than measuring a map through daylight. */
    bool checkAmbient()
    {
        step(Calibration::AMBIENT, 1);
        dark();
        vTaskDelay(pdMS_TO_TICKS(400));
        Reading r = measure();
        ambient_ = r.lux;

        if (r.lux < 0.0f || r.lux > CAL_MAX_AMBIENT_LUX)
        {
            fail("calibTooBright");
            return false;
        }
        return true;
    }

    /**
     * Finds the rung the whole run will use, from the strongest single cell.
     *
     * **Pinned at the blindest rung throughout, and compared in counts.** The
     * first version searched with auto-ranging and got it exactly wrong: a
     * frame that forces the ladder to move returns -1 rather than a
     * measurement - deliberately, because that reading belongs to two
     * different gains - and -1 loses a contest for the *strongest* cell. So
     * the brightest cells looked like the weakest, a rung was chosen to suit a
     * middling one, and in the pass that followed the real peaks saturated,
     * came back as zero, and dropped out of the map altogether. The clock
     * reported twenty cells and none of the three that matter.
     *
     * At the bottom rung nothing on this strip can saturate, so every frame is
     * a real measurement and the ordering is trustworthy. Then the ladder is
     * climbed as far as the strongest cell allows, which is where the cells
     * that barely register have the most resolution left.
     *
     * This is the same mistake the scan script made once, in the other
     * direction, and it is worth stating twice: a scan whose rung can move
     * under it lies confidently.
     */
    bool chooseRung(uint8_t &bestRow, uint8_t &bestColumn)
    {
        LightSensor *sensor = ambientLight.device();
        if (!sensor) { fail("calibNoSensor"); return false; }

        step(Calibration::RANGE, ROWS + COLUMNS + sensor->rungCount() + 3);
        pin(0);

        int32_t best = -1;
        bestRow = 0;
        for (uint8_t r = 0; r < ROWS; r++)
        {
            if (stopWanted) return false;
            lightRow(r);
            int32_t counts = (int32_t)measure().ch0;
            if (counts > best) { best = counts; bestRow = r; }
        }

        best = -1;
        bestColumn = 0;
        for (uint8_t c = 0; c < COLUMNS; c++)
        {
            if (stopWanted) return false;
            light(bestRow, c, 255, 255, 255);
            int32_t counts = (int32_t)measure().ch0;
            if (counts > best) { best = counts; bestColumn = c; }
        }

        // Up from the blind end, keeping the last rung that still fits. Not
        // down from the sensitive end: that direction has to recognise
        // saturation to stop, and a saturated reading is exactly the one that
        // cannot be trusted to say so.
        light(bestRow, bestColumn, 255, 255, 255);
        int8_t chosen = 0;
        for (uint8_t which = 0; which < sensor->rungCount(); which++)
        {
            if (stopWanted) return false;
            pin((int8_t)which);
            Reading r = measure();
            if (saturated(r)) break;
            chosen = (int8_t)which;
        }

        pin(chosen);
        rung_ = (uint8_t)chosen;
        Reading check = measure();
        debugA("Calibration: strongest cell (%d,%d), rung %d, %d counts, %.3f lx",
               bestRow, bestColumn, chosen, check.ch0, check.lux);
        return true;
    }

    /** Every cell in white, against a dark reference taken along the way. */
    bool scanCells()
    {
        step(Calibration::CELLS, CELL_COUNT + CELL_COUNT / CAL_DARK_EVERY + 1);

        float floorCounts = 0.0f;
        for (uint16_t i = 0; i < CELL_COUNT; i++)
        {
            if (stopWanted) return false;

            if (i % CAL_DARK_EVERY == 0)
            {
                dark();
                floorCounts = (float)measure().ch0;
            }

            uint8_t row = (uint8_t)(i / COLUMNS);
            uint8_t column = (uint8_t)(i % COLUMNS);
            light(row, column, 255, 255, 255);

            // Counts rather than lux, because this pass only ranks cells and
            // counts stay meaningful down at the noise floor where the lux
            // calculation has nothing left to work with.
            Reading r = measure();
            if (saturated(r)) { fail("calibSaturated"); return false; }
            float counts = (float)r.ch0 - floorCounts;
            coarse[i] = counts > 0.0f ? counts : 0.0f;
        }

        float peak = 0.0f;
        for (uint16_t i = 0; i < CELL_COUNT; i++) if (coarse[i] > peak) peak = coarse[i];
        if (peak <= 0.0f) { fail("calibNoCoupling"); return false; }

        // The strongest first, so that a map truncated at COUPLING_MAX_CELLS
        // keeps the cells that matter rather than the ones that sort first.
        kept_ = 0;
        float threshold = peak * CAL_KEEP_PERMILLE / 1000.0f;
        while (kept_ < COUPLING_MAX_CELLS)
        {
            int16_t bestAt = -1;
            float bestLux = threshold;
            for (uint16_t i = 0; i < CELL_COUNT; i++)
            {
                if (coarse[i] < bestLux) continue;
                bool already = false;
                for (uint8_t k = 0; k < kept_; k++)
                {
                    if (keptRow[k] == i / COLUMNS && keptCol[k] == i % COLUMNS) already = true;
                }
                if (already) continue;
                bestAt = (int16_t)i;
                bestLux = coarse[i];
            }
            if (bestAt < 0) break;
            keptRow[kept_] = (uint8_t)(bestAt / COLUMNS);
            keptCol[kept_] = (uint8_t)(bestAt % COLUMNS);
            kept_++;
        }

        debugA("Calibration: %d cells above %.1f permille of %.0f counts, "
               "strongest (%d,%d)",
               kept_, CAL_KEEP_PERMILLE, peak, keptRow[0], keptCol[0]);
        return kept_ > 0;
    }

    /** The survivors, once per channel. Three coefficients, not one: the
     *  coupling is wavelength dependent and at two cells' distance red carries
     *  almost twice as far as blue. */
    bool scanChannels()
    {
        step(Calibration::CHANNELS, (uint16_t)kept_ * 3 + 1);

        dark();
        float floorLux = measure().lux;

        for (uint8_t k = 0; k < kept_; k++)
        {
            if (stopWanted) return false;

            light(keptRow[k], keptCol[k], 255, 0, 0);
            keptRed[k] = max(0.0f, measure().lux - floorLux);

            light(keptRow[k], keptCol[k], 0, 255, 0);
            keptGreen[k] = max(0.0f, measure().lux - floorLux);

            light(keptRow[k], keptCol[k], 0, 0, 255);
            keptBlue[k] = max(0.0f, measure().lux - floorLux);
        }
        return true;
    }

    /**
     * How the light out of one LED depends on the eight bit value written.
     *
     * Neither linear nor a gamma, and it belongs to the LED rather than to the
     * colour - measured separately for white, red, green and blue it came out
     * the same to a few parts in a thousand. Measured on the strongest cell,
     * which is the only one with enough signal left at a drive of four.
     */
    bool scanDrive()
    {
        step(Calibration::DRIVE, DRIVE_STEPS + 1);

        uint8_t row = keptRow[0], column = keptCol[0];

        dark();
        float floorCounts = (float)measure().ch0;

        float top = 0.0f;
        for (uint8_t i = 0; i < DRIVE_STEPS; i++)
        {
            if (stopWanted) return false;
            uint8_t v = DRIVE_LEVELS[i];
            light(row, column, v, v, v);
            float counts = (float)measure().ch0 - floorCounts;
            if (counts < 0.0f) counts = 0.0f;
            if (i == 0) top = counts;
            driveResponse[i] = top > 0.0f ? counts / top : 0.0f;
        }

        if (top <= 0.0f) { fail("calibNoCoupling"); return false; }
        return true;
    }

    bool storeResult()
    {
        step(Calibration::STORING, 1);

        JsonDocument doc;
        JsonObject cells = doc["cells"].to<JsonObject>();
        for (uint8_t k = 0; k < kept_; k++)
        {
            char key[8];
            snprintf(key, sizeof(key), "%d,%d", keptRow[k], keptCol[k]);
            JsonObject one = cells[key].to<JsonObject>();
            one["r"] = keptRed[k];
            one["g"] = keptGreen[k];
            one["b"] = keptBlue[k];
        }

        JsonObject drive = doc["drive"].to<JsonObject>();
        JsonArray levels = drive["levels"].to<JsonArray>();
        JsonArray response = drive["response"].to<JsonArray>();
        for (uint8_t i = 0; i < DRIVE_STEPS; i++)
        {
            levels.add(DRIVE_LEVELS[i]);
            response.add(driveResponse[i]);
        }

        // What the run was measured under, so a stored map can be judged later
        // rather than only believed. Coupling ignores them.
        doc["rung"] = rung_;
        doc["ambient"] = ambient_;

        String out;
        serializeJson(doc, out);
        done_++;

        if (!Coupling::store(out)) { fail("calibStore"); return false; }
        return true;
    }

    void release()
    {
        pin(-1);
        ledDriver.clearRaw();
        ledDriver.showRaw();
        mode = modeBefore;
        needsUpdateFromRtc = true;
        ambientLight.hold(false);
        active = false;
        debugW("Calibration: gave the strip back, mode %d", mode);
    }

    void run(void *)
    {
        debugA("Calibration: started");

        uint8_t bestRow = 0, bestColumn = 0;
        bool ok = checkAmbient() &&
                  chooseRung(bestRow, bestColumn) &&
                  scanCells() &&
                  scanChannels() &&
                  scanDrive() &&
                  storeResult();

        if (ok)
        {
            phase_ = Calibration::DONE;
            debugA("Calibration: finished, %d cells on rung %d", kept_, rung_);
        }
        else if (stopWanted && phase_ != Calibration::FAILED)
        {
            fail("calibCancelled");
            debugW("Calibration: cancelled");
        }
        else
        {
            debugE("Calibration: failed, %s", error_);
        }

        release();
        task = nullptr;
        vTaskDelete(NULL);
    }
}

bool Calibration::start()
{
    if (active) { strncpy(error_, "calibBusy", sizeof(error_) - 1); return false; }
    if (Lab::active()) { strncpy(error_, "calibLabActive", sizeof(error_) - 1); return false; }
    if (!ambientLight.present()) { strncpy(error_, "calibNoSensor", sizeof(error_) - 1); return false; }

    error_[0] = '\0';
    stopWanted = false;
    done_ = total_ = 0;
    kept_ = 0;
    rung_ = 0;
    phase_ = Calibration::AMBIENT;

    // Taken before the task starts, so a status read between the two never
    // sees a run that owns nothing.
    active = true;
    modeBefore = mode;
    mode = EXT_MODE_LAB;
    ambientLight.hold(true);
    ledDriver.clearRaw();
    ledDriver.showRaw();

    // Core 0, beside the light sampler and the OTA download: the web server on
    // core 1 has to keep answering, or there is no progress to show.
    if (xTaskCreatePinnedToCore(run, "calib", CAL_TASK_STACK, NULL, 1, &task, 0) != pdPASS)
    {
        strncpy(error_, "calibNoTask", sizeof(error_) - 1);
        phase_ = Calibration::FAILED;
        release();
        return false;
    }
    return true;
}

void Calibration::cancel() { stopWanted = true; }

bool Calibration::running()          { return active; }
Calibration::Phase Calibration::phase() { return (Phase)phase_; }
uint16_t Calibration::done()         { return done_; }
uint16_t Calibration::total()        { return total_; }
float Calibration::ambient()         { return ambient_; }
uint8_t Calibration::rung()          { return rung_; }
uint8_t Calibration::kept()          { return kept_; }
const char *Calibration::error()     { return error_; }
