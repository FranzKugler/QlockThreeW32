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
 * What is done with the reading is not here. Luminance.h owns the curve from
 * lux to display brightness and how it is learned; this file measures, smooths
 * and stops. The two lived together for a while and the seam was worth cutting:
 * the curve is arithmetic that can be reasoned about at a desk, and the
 * measurement is a chip on a wire.
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
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>   // one bus, two tasks; see AmbientLight below

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

    // ------ what the lab interface needs, and nothing else uses ------
    //
    // Optional, because they are not things every chip can do. The default
    // answers say "not this one" rather than pretending, so a lab client can
    // ask and be told no instead of reading a number that means nothing.

    /**
     * The raw channel counts behind the last readLux().
     *
     * The point of having them is that the two channels see the world
     * differently: WS2812B put out almost no infrared, room light and daylight
     * put out plenty. If the infrared channel barely moves when the display
     * does, it is an ambient reading the clock cannot pollute - which is worth
     * a great deal on a clock whose sensor can see its own face.
     */
    virtual bool readChannels(uint16_t &full, uint16_t &infrared) { (void)full; (void)infrared; return false; }

    /** How many sensitivity steps this chip is driven through. 0 for none. */
    virtual uint8_t rungCount() const { return 0; }

    /** Where it is standing now. */
    virtual uint8_t rung() const { return 0; }

    /**
     * Pins the sensitivity, or -1 to let it range again.
     *
     * A measurement series wants this: auto-ranging changes gain and
     * integration time underneath a scan, and two readings taken on different
     * rungs are not comparable in counts, only in lux - and the lux
     * calculation is exactly what we are trying to check.
     */
    virtual bool pinRung(int8_t which) { (void)which; return false; }

    /** Integration time of the current rung, in milliseconds. */
    virtual uint16_t integrationMs() const { return 0; }
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
 * ams TSL2591 on I2C, address 0x29.
 *
 * Resolves to roughly 188 uLx at full sensitivity - a factor of twenty below
 * the VEML7700 and six hundred below the BH1750 this project started with.
 * That only matters at the dark end, and the dark end is the whole point:
 * behind a front panel an evening living room arrives as a fraction of a lux,
 * and a sensor reporting a flat 0 there cannot tell dusk from night, so the
 * automatic sits at its floor all evening.
 *
 * 0x29 does not collide with the VEML7700 at 0x10. Both can be wired to the
 * same two lines, which is how one gets judged against the other.
 *
 * Unlike the VEML7700 library this one has no auto-ranging, so the class does
 * it - see readLux() for why that turns out to be an advantage rather than a
 * chore.
 */
class Tsl2591Sensor : public LightSensor
{
public:
    bool begin() override;
    float readLux() override;
    const char *name() const override { return "TSL2591"; }

    bool readChannels(uint16_t &full, uint16_t &infrared) override;
    uint8_t rungCount() const override;
    uint8_t rung() const override { return _rung; }
    bool pinRung(int8_t which) override;
    uint16_t integrationMs() const override;

private:
    /** Pushes the current rung's gain and integration time to the chip. */
    void applyRung();

    void *device = nullptr; // Adafruit_TSL2591, kept out of the header
    byte _rung = 0;         // position on the sensitivity ladder in the .cpp
    bool _pinned = false;   // the lab is holding it still
    uint16_t _lastFull = 0, _lastIr = 0;
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

    /**
     * The light to *learn* from, averaged over a few seconds rather than half
     * a minute.
     *
     * Not the same question as lux(). The regulator must not follow a passing
     * shadow; a person correcting the brightness is telling the clock about
     * the light in the room now. Storing a point against the thirty second
     * average puts it at a light level the room has already left - see
     * TEACHING_SECONDS in the .cpp for the session where that was measured.
     */
    float teachingLux() const { return quick; }

    const char *name() const { return sensor ? sensor->name() : "none"; }

    /** The sensor itself, for the lab. Null when none is fitted. */
    LightSensor *device() const { return sensorOk ? sensor : nullptr; }

    /**
     * Takes one measurement now, on the calling task, unsmoothed.
     *
     * The background sampler exists because auto-ranging blocks, and the
     * smoothing exists because a regulator should not chase noise. Both are
     * wrong for a measurement: a scan wants the value belonging to the frame
     * that is on the strip at this moment, not a thirty-second average of the
     * frames before it. This blocks for one integration time, up to about
     * 600 ms - which is why it is only ever called from a lab request.
     */
    float readNow();

    /**
     * Keeps the background sampler off the bus.
     *
     * Two tasks on one I2C bus need a lock in any case, and the sampler would
     * also undo a pinned rung between two frames of a scan. While held it
     * skips its turn entirely.
     */
    void hold(bool on) { held = on; }
    bool isHeld() const { return held; }

private:
    static void sampleTask(void *self);

    LightSensor *sensor = nullptr;
    bool sensorOk = false;

    // Written by the task on core 0, read by loop() on core 1. Single 32 bit
    // values, so a torn read is not possible on this core and no lock is
    // needed for what the web UI does with them.
    volatile float smoothed = 0.0f;
    volatile float quick = 0.0f;
    volatile float lastRaw = 0.0f;
    volatile uint32_t sampleCount = 0;

    // The bus is shared between the sampler on core 0 and a lab request on
    // core 1, and a half-finished I2C transaction is not a small problem.
    SemaphoreHandle_t busLock = nullptr;
    volatile bool held = false;
};

#endif
