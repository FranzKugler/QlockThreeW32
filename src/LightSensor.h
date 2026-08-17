/**
 * LightSensor
 * Ambient light measurement.
 *
 * Two things live here. `LightSensor` is the interface a sensor has to satisfy
 * - one blocking reading in lux - so the part that matters can be swapped
 * without touching anything else. `Veml7700Sensor` is the one implementation
 * so far; a TSL2591 or an OPT3001 would be another class next to it and a
 * different line in `AmbientLight::begin()`.
 *
 * `AmbientLight` owns the sensor, samples it from a task of its own and
 * smooths the result. The task is not optional: auto-ranging on the VEML7700
 * changes gain and integration time and waits for a fresh measurement each
 * time, which can block for well over a second - and the clock's web server is
 * synchronous, so a blocked loop() is a clock that stops answering.
 *
 * Nothing regulates on this yet. The reading is measured, smoothed and served
 * to the web UI so the placement behind the front panel can be judged first:
 * a dark panel passes only a few percent of the visible light, and how much is
 * a property of each individual clock.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  17.8.2026
 * @updated  17.8.2026
 */
#ifndef LIGHTSENSOR_H
#define LIGHTSENSOR_H

#include "Arduino.h"

// The XIAO ESP32-S3 brings these out as D4 and D5 and the core already
// defaults to them; overridable because the firmware serves every build of the
// clock and the sensor does not sit in the same place in all of them.
#ifndef I2C_SDA_PIN
#define I2C_SDA_PIN 5
#endif
#ifndef I2C_SCL_PIN
#define I2C_SCL_PIN 6
#endif

// Readings below this are treated as darkness. The VEML7700 reports a plain 0
// in a closed room and log(0) has no answer, so the curve needs a floor - and
// a hundredth of a lux is far below anything the eye distinguishes anyway.
#define LUX_FLOOR 0.01f

// How far apart the two calibration points have to be, as a ratio. Two points
// taken in near enough the same light describe no slope at all, and dividing by
// that span would swing the brightness across its whole range on sensor noise.
// Two doublings is the least that still says something.
#define CALIBRATION_MIN_RATIO 4.0f

/**
 * Display brightness in percent for an ambient reading, from the two
 * calibration points.
 *
 * Brightness runs linearly in log(lux) between them, not in lux: perception is
 * roughly logarithmic (Weber-Fechner), and the range the clock has to cover
 * spans several decades - a dark bedroom and a sunlit room differ by a factor
 * of thousands, which no linear mapping survives. The result is clamped to the
 * two calibrated ends rather than extrapolated, so a sunbeam on the sensor
 * cannot drive the display past what the user asked for.
 *
 * Pure, so the curve can be reasoned about without a sensor present.
 */
byte brightnessForLux(float lux, float luxLow, byte brightLow,
                      float luxHigh, byte brightHigh);

/** One ambient light sensor, whichever chip it happens to be. */
class LightSensor
{
public:
    virtual ~LightSensor() {}

    /** Brings the chip up. False when it does not answer. */
    virtual bool begin() = 0;

    /** One measurement in lux, or a negative value when the read failed. */
    virtual float readLux() = 0;

    /** For the web UI, so it can say what it is talking to. */
    virtual const char *name() const = 0;
};

/**
 * Vishay VEML7700 on I2C, address 0x10 (fixed, so only one per bus).
 *
 * Reads through the library's auto-ranging mode, which walks gain and
 * integration time until the reading sits in a usable part of the range and
 * applies Vishay's correction for the non-linearity at the top end. That is
 * the whole reason for choosing this over the BH1750 that used to be here:
 * behind a dark front panel a lit living room can arrive as a handful of lux,
 * and the BH1750 resolves 1 lx.
 */
class Veml7700Sensor : public LightSensor
{
public:
    bool begin() override;
    float readLux() override;
    const char *name() const override { return "VEML7700"; }

private:
    void *device = nullptr; // Adafruit_VEML7700, kept out of the header
};

/**
 * Owns a sensor, samples it in the background and smooths the result.
 */
class AmbientLight
{
public:
    /** Starts the sensor and the sampling task. Safe to call when none is wired. */
    void begin();

    /** True once a sensor has answered at least one measurement. */
    bool available() const { return sampleCount > 0; }

    /** Whether a sensor was found at all. */
    bool present() const { return sensor != nullptr && sensorOk; }

    /** Smoothed ambient light in lux. */
    float lux() const { return smoothed; }

    /** The last raw measurement, unsmoothed - useful while placing the sensor. */
    float raw() const { return lastRaw; }

    const char *name() const { return sensor ? sensor->name() : "none"; }

private:
    static void sampleTask(void *self);

    LightSensor *sensor = nullptr;
    bool sensorOk = false;

    // Written by the task on core 0, read by loop() on core 1. Single 32 bit
    // values, so a torn read is not possible on this core and no lock is
    // needed for what the web UI does with them.
    volatile float smoothed = 0.0f;
    volatile float lastRaw = 0.0f;
    volatile uint32_t sampleCount = 0;
};

#endif
