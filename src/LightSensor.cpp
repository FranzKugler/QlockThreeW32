/**
 * LightSensor
 * See LightSensor.h for what this is and why the sampling runs in a task.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  17.8.2026
 * @updated  17.8.2026
 */
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include <Adafruit_TSL2591.h>

#include "LightSensor.h"

// Debug and the debugX macros, plus the ring the web UI reads them out of.
#include "LogBuffer.h"

// How often the sensor is asked. Ambient light does not change quickly and
// every reading costs an auto-ranging round, so this is deliberately slow.
#define SAMPLE_INTERVAL_MS 2000

// Time constant of the smoothing, in seconds. Someone walking past, a cloud,
// a door opening - none of that should reach the display, and a clock that
// visibly follows such things is the usual complaint about automatic
// brightness. Converted to an EMA weight from the sample interval below.
#define SMOOTHING_SECONDS 30.0f

// Enough for the Adafruit library and the I2C stack underneath it.
#define SAMPLE_TASK_STACK 4096


bool Veml7700Sensor::begin()
{
    Adafruit_VEML7700 *veml = new Adafruit_VEML7700();
    if (!veml->begin(&Wire))
    {
        delete veml;
        return false;
    }
    device = veml;
    return true;
}

float Veml7700Sensor::readLux()
{
    if (!device) return -1.0f;
    // Auto-ranging: picks gain and integration time, then applies Vishay's
    // correction for the non-linearity above roughly a thousand lux. Blocking,
    // which is why this is only ever called from the sampling task.
    return ((Adafruit_VEML7700 *)device)->readLux(VEML_LUX_AUTO);
}


// --- TSL2591 -------------------------------------------------------------
//
// The sensitivity ladder, least sensitive first. Gain carries the range - its
// steps are factors of 25, 17 and 23 - and integration time fills in between
// them with factors of 2 and 3. Written out as a list rather than computed from
// the two axes because not every combination is worth having: the 300 and
// 500 ms steps buy a third of a decade for a third of a second and are left
// out, and the low gains do not need the long times.
static const struct
{
    tsl2591Gain_t gain;
    tsl2591IntegrationTime_t time;
} LADDER[] = {
    { TSL2591_GAIN_LOW,  TSL2591_INTEGRATIONTIME_100MS },   // ~88 klx full scale
    { TSL2591_GAIN_LOW,  TSL2591_INTEGRATIONTIME_200MS },
    { TSL2591_GAIN_MED,  TSL2591_INTEGRATIONTIME_100MS },
    { TSL2591_GAIN_MED,  TSL2591_INTEGRATIONTIME_200MS },
    { TSL2591_GAIN_HIGH, TSL2591_INTEGRATIONTIME_100MS },
    { TSL2591_GAIN_HIGH, TSL2591_INTEGRATIONTIME_200MS },   // start here
    { TSL2591_GAIN_HIGH, TSL2591_INTEGRATIONTIME_400MS },
    { TSL2591_GAIN_MAX,  TSL2591_INTEGRATIONTIME_200MS },
    { TSL2591_GAIN_MAX,  TSL2591_INTEGRATIONTIME_400MS },
    { TSL2591_GAIN_MAX,  TSL2591_INTEGRATIONTIME_600MS },   // ~188 uLx resolution
};
#define LADDER_STEPS ((byte)(sizeof(LADDER) / sizeof(LADDER[0])))

// Where to start. In the middle, because either end is four steps from the
// other and there is no way to guess which end a given clock sits at before
// the first reading.
#define LADDER_START 5

// The ADC saturates at 36863 counts at 100 ms and at 65535 above it. Backing
// off at nine tenths of that keeps the reading out of the region where the
// two channels stop tracking each other and the lux calculation goes wrong.
#define SATURATED_FRACTION 0.9f

// Below this many counts the reading is mostly quantisation noise and the next
// rung up is worth the wait. Not zero: a handful of counts still carries
// information, and stepping up costs a whole sample interval.
#define TOO_FEW_COUNTS 80


bool Tsl2591Sensor::begin()
{
    Adafruit_TSL2591 *tsl = new Adafruit_TSL2591(2591);
    if (!tsl->begin(&Wire))
    {
        delete tsl;
        return false;
    }
    device = tsl;
    _rung = LADDER_START;
    applyRung();
    return true;
}

void Tsl2591Sensor::applyRung()
{
    if (!device) return;
    ((Adafruit_TSL2591 *)device)->setGain(LADDER[_rung].gain);
    ((Adafruit_TSL2591 *)device)->setTiming(LADDER[_rung].time);
}

/**
 * One measurement, and at most one step of the range.
 *
 * The library offers no auto-ranging, which turns out to be the better deal
 * here. Walking the whole ladder in one call is what makes the VEML7700 block
 * for over a second; this takes exactly one integration time - 600 ms at the
 * worst - and moves a single rung, leaving the rest to the next sample.
 *
 * It can afford to: the reading is smoothed over 30 s anyway, so converging
 * across a few samples costs nothing that is visible on the face. A sample
 * taken while saturated or nearly dark is reported as a failed read (negative),
 * and the sampling task drops those rather than feeding them to the average.
 */
float Tsl2591Sensor::readLux()
{
    if (!device) return -1.0f;
    Adafruit_TSL2591 *tsl = (Adafruit_TSL2591 *)device;

    // Both channels in one transaction, so they belong to the same integration
    // window. Blocking for the integration time - only ever called on the task.
    uint32_t both = tsl->getFullLuminosity();
    uint16_t ir = both >> 16;
    uint16_t full = both & 0xFFFF;

    // Kept for readChannels(). The lab wants the counts these lux were
    // computed from, because the whole question is whether the lux calculation
    // is telling the truth about a face the sensor can see.
    _lastFull = full;
    _lastIr = ir;

    const uint16_t maxCounts =
        LADDER[_rung].time == TSL2591_INTEGRATIONTIME_100MS ? 36863 : 65535;
    const uint16_t ceiling = (uint16_t)(maxCounts * SATURATED_FRACTION);

    // Pinned means pinned: a scan compares counts across frames, and a rung
    // that moves between two of them makes them incomparable. Saturation is
    // then reported as it is rather than ranged away from.
    if (_pinned) return tsl->calculateLux(full, ir);

    if (full >= ceiling || ir >= ceiling)
    {
        if (_rung > 0)
        {
            _rung--;
            applyRung();
            return -1.0f;       // this one is not a measurement, drop it
        }
        // Already as blind as it gets - direct sun on the sensor. Fall through
        // and report what it says; the curve clamps at the calibrated end.
    }
    else if (full < TOO_FEW_COUNTS && _rung + 1 < LADDER_STEPS)
    {
        _rung++;
        applyRung();
        return -1.0f;
    }

    float lux = tsl->calculateLux(full, ir);
    // The library reports overflow as a negative, which is the same thing the
    // ceiling above catches - but it applies its own limits, so let it speak.
    if (lux < 0.0f) return -1.0f;
    return lux;
}

bool Tsl2591Sensor::readChannels(uint16_t &full, uint16_t &infrared)
{
    full = _lastFull;
    infrared = _lastIr;
    return true;
}

uint8_t Tsl2591Sensor::rungCount() const { return LADDER_STEPS; }

uint16_t Tsl2591Sensor::integrationMs() const
{
    switch (LADDER[_rung].time)
    {
        case TSL2591_INTEGRATIONTIME_100MS: return 100;
        case TSL2591_INTEGRATIONTIME_200MS: return 200;
        case TSL2591_INTEGRATIONTIME_300MS: return 300;
        case TSL2591_INTEGRATIONTIME_400MS: return 400;
        case TSL2591_INTEGRATIONTIME_500MS: return 500;
        default:                            return 600;
    }
}

bool Tsl2591Sensor::pinRung(int8_t which)
{
    if (!device) return false;

    if (which < 0)
    {
        _pinned = false;
        return true;
    }
    if (which >= (int8_t)LADDER_STEPS) return false;

    _rung = (byte)which;
    _pinned = true;
    applyRung();
    return true;
}


void AmbientLight::begin()
{
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    // Each candidate in turn, the first that answers wins. One firmware serves
    // every build of the clock and they do not all carry the same chip, so the
    // choice belongs at run time rather than in a build flag - and a sensor
    // swapped on the bench needs no rebuild.
    //
    // The TSL2591 is asked first on purpose. The two do not share an address
    // (0x29 against 0x10), so a clock with both wired up answers twice, and
    // then the more sensitive one is the one to keep.
    LightSensor *candidates[] = { new Tsl2591Sensor(), new Veml7700Sensor() };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++)
    {
        if (sensor == nullptr && candidates[i]->begin())
            sensor = candidates[i];
        else
            delete candidates[i];
    }
    sensorOk = sensor != nullptr;

    if (!sensorOk)
    {
        // Not an error worth stopping for: most clocks have no sensor fitted,
        // and everything else works without one.
        debugA("No ambient light sensor on I2C (SDA %d, SCL %d), measurement is off",
               I2C_SDA_PIN, I2C_SCL_PIN);
        return;
    }

    debugA("%s found, sampling every %d ms", sensor->name(), SAMPLE_INTERVAL_MS);

    busLock = xSemaphoreCreateMutex();

    // Core 0, next to the OTA download and away from loop() and the web
    // server on core 1 - a reading takes long enough to be felt otherwise.
    xTaskCreatePinnedToCore(sampleTask, "light", SAMPLE_TASK_STACK, this, 1, NULL, 0);
}

void AmbientLight::sampleTask(void *self)
{
    AmbientLight *light = (AmbientLight *)self;

    // dt / (tau + dt), the usual first order lag written as a weight.
    const float dt = SAMPLE_INTERVAL_MS / 1000.0f;
    const float weight = dt / (SMOOTHING_SECONDS + dt);

    for (;;)
    {
        // The lab holds the bus and the rung while it measures. Skipping the
        // turn entirely is better than blocking on the lock: a scan can hold
        // it for a minute, and a sampler queued up behind it would then push
        // a stale frame's reading into the average the moment it is let go.
        if (light->held)
        {
            vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
            continue;
        }

        xSemaphoreTake(light->busLock, portMAX_DELAY);
        float lux = light->sensor->readLux();
        xSemaphoreGive(light->busLock);

        if (lux >= 0.0f)
        {
            light->lastRaw = lux;
            // The first reading seeds the average, or it would crawl up from
            // zero over the first few minutes after every restart.
            light->smoothed = light->sampleCount == 0
                                  ? lux
                                  : light->smoothed + weight * (lux - light->smoothed);
            light->sampleCount++;
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
    }
}


/**
 * One measurement, here and now, on the calling task.
 *
 * Everything the sampler does for the regulator is wrong for a measurement:
 * the smoothing hides the frame that is actually on the strip, and the two
 * second cadence is far too slow for a scan. This blocks for one integration
 * time - up to about 600 ms on the most sensitive rung - which is why it is
 * only ever reached from a lab request, and why the web server is unresponsive
 * while a sweep runs.
 *
 * The lock is the same one the sampler takes. Even with hold() set, a sample
 * may already be in flight when the lab arrives.
 */
float AmbientLight::readNow()
{
    if (!sensorOk || sensor == nullptr) return -1.0f;
    if (busLock == nullptr) return sensor->readLux();

    xSemaphoreTake(busLock, portMAX_DELAY);
    float lux = sensor->readLux();
    xSemaphoreGive(busLock);
    return lux;
}

