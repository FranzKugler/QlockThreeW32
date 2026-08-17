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

#include "LightSensor.h"

#include <RemoteDebug.h>
extern RemoteDebug Debug;

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


byte brightnessForLux(float lux, float luxLow, byte brightLow,
                      float luxHigh, byte brightHigh)
{
    // NaN fails every comparison, so the test is written to let it through to
    // the floor rather than around it.
    if (!(lux > LUX_FLOOR)) lux = LUX_FLOOR;
    if (!(luxLow > LUX_FLOOR)) luxLow = LUX_FLOOR;

    // A stored curve whose points sit too close together would otherwise
    // divide by something near zero. The web UI refuses to write one, but the
    // endpoint is reachable without it and an old record could hold anything.
    if (!(luxHigh > luxLow * CALIBRATION_MIN_RATIO)) luxHigh = luxLow * CALIBRATION_MIN_RATIO;

    float position = (log10f(lux) - log10f(luxLow)) /
                     (log10f(luxHigh) - log10f(luxLow));
    if (position < 0.0f) position = 0.0f;
    if (position > 1.0f) position = 1.0f;

    float percent = brightLow + position * ((float)brightHigh - (float)brightLow);

    // Never 0: that is the display switching itself off, which is a mode the
    // user picks in the display tab and not something the light sensor decides.
    long value = lroundf(percent);
    if (value < 1) value = 1;
    if (value > 100) value = 100;
    return (byte)value;
}


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


void AmbientLight::begin()
{
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    // The one place a different chip would be chosen. Trying each in turn
    // would work too, once there is more than one.
    sensor = new Veml7700Sensor();
    sensorOk = sensor->begin();

    if (!sensorOk)
    {
        // Not an error worth stopping for: most clocks have no sensor fitted,
        // and everything else works without one.
        debugA("No %s found on I2C, ambient light measurement is off", sensor->name());
        return;
    }

    debugA("%s found, sampling every %d ms", sensor->name(), SAMPLE_INTERVAL_MS);

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
        float lux = light->sensor->readLux();

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
