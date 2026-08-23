/**
 * Calibration
 * The clock measuring its own coupling, without a script and without a laptop.
 *
 * Coupling.h says why the numbers matter and what they are. This is where they
 * come from on a clock that is not the one they were first measured on - and
 * that is the whole point of it existing: the coefficients say "cell (7,5)
 * puts 69.1 lx into the sensor in red", which is true only while the sensor
 * sits behind that particular letter. Another clock, another place, other
 * louvres, and all thirty numbers are wrong. A calibration that needs Python
 * on the same network is a calibration most clocks will never get.
 *
 * Three passes, about ninety seconds:
 *
 *   1. every cell in white, one at a time - which of them reach the sensor
 *   2. the handful that do, once per colour channel - the coefficients
 *   3. the strongest of them at thirteen drive levels - the response table
 *
 * Two things the script was handed and this has to work out for itself:
 *
 *  - **The sensitivity rung.** The script has COARSE_RUNG = 4 written into it,
 *    because the person who wrote it knew that suited one clock. A scan on the
 *    wrong rung lies confidently: the first search for the sensor put it two
 *    cells away because a bright row saturated and the ladder moved between a
 *    frame and its dark reference. So there is a pass zero - rows, then the
 *    cells of the strongest row, then the rung stepped down until the
 *    strongest single cell no longer runs out of scale.
 *  - **A dark room.** A cell at one part in a thousand delivers fractions of a
 *    lux; in daylight that is under the noise, and a dark frame beside every
 *    measurement removes the drift but not the noise. So the ambient is
 *    measured first and the run **refuses** rather than producing a map that
 *    looks plausible and is not.
 *
 * It runs in a task on core 0, like the OTA download and for the same reason:
 * the web server here is synchronous, and ninety seconds of blocked loop()
 * would mean no progress to show and a clock that answers nothing.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.2
 * @created  24.8.2026
 * @updated  24.8.2026
 */
#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>

// How much ambient light the run tolerates before it refuses, in lux behind
// the panel. A covered clock in a dark room measures thousandths; a lit room
// measures single figures. One lux is well clear of both, and refusing is the
// point: a map measured through daylight is worse than no map, because nothing
// afterwards says it is wrong.
#define CAL_MAX_AMBIENT_LUX 1.0f

// Milliseconds a frame is given to be seen before it is measured. The strip
// latches immediately; this is for the sensor's integration to belong to the
// frame rather than to the one before it.
#define CAL_SETTLE_MS 120

// How often a dark reference is taken during the long pass. Every cell would
// double a thirty second pass to sixty; a covered clock does not drift over
// the fifteen seconds this leaves between references.
#define CAL_DARK_EVERY 16

// Cells below this share of the strongest one are dropped. At a thousandth of
// the peak a whole face of them would still be a tenth of the one cell next to
// the sensor, and every cell kept costs three more measurements.
#define CAL_KEEP_PERMILLE 1.0f

namespace Calibration
{
    /** Where the run has got to, for the progress display. */
    enum Phase : uint8_t
    {
        IDLE = 0,
        AMBIENT,     // is it dark enough to measure at all
        RANGE,       // which rung, found from the strongest cell
        CELLS,       // every cell in white
        CHANNELS,    // the survivors, per colour
        DRIVE,       // the response table
        STORING,
        DONE,
        FAILED
    };

    /**
     * Starts a run. False when one is already going, the lab holds the strip,
     * or there is no sensor - `error()` says which.
     */
    bool start();

    /** Asks a running calibration to stop at the next frame. */
    void cancel();

    /** True while the strip belongs to a calibration. */
    bool running();

    Phase phase();

    /** Frames done and expected in the current phase, for a progress bar. */
    uint16_t done();
    uint16_t total();

    /** The ambient reading the run refused on, or measured before starting. */
    float ambient();

    /** Which rung it settled on, and how many cells survived. Read after DONE. */
    uint8_t rung();
    uint8_t kept();

    /** An error code for the UI, or "" - the same vocabulary as the REST API. */
    const char *error();
}

#endif
