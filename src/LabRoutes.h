/**
 * LabRoutes
 * Direct control of the strip and the light sensor, for measuring the clock.
 *
 * This exists because of one number: on the clock it was written for, turning
 * the display from 20 % to 100 % moved the light sensor's own reading from
 * 0.42 lx to 16.79 lx in an unchanged room. The sensor can see the face it is
 * supposed to be regulating, which closes a positive feedback loop and poisons
 * everything the automatic brightness learns. How much, from which cells, and
 * whether the infrared channel escapes it are all questions with numbers for
 * answers - and none of those numbers can be guessed from the source.
 *
 * So this is an instrument, not a feature. It is shaped by that:
 *
 *  - **Raw means raw.** No gamma, no brightness scaling, no colour setting. A
 *    pixel written here goes to the strip as the number it was given, because
 *    an instrument that applies two correction curves measures itself.
 *  - **The sensor is read synchronously and unsmoothed**, with the sensitivity
 *    rung pinnable. The regulator's thirty-second average is the right answer
 *    to a different question, and a scan comparing counts across frames cannot
 *    have the gain moving underneath it.
 *  - **A sweep is one request.** Setting a frame, waiting, and measuring in
 *    three round trips puts network jitter inside the measurement; a hundred
 *    frames in one request does not. The clock is unresponsive while it runs,
 *    which is a fair price for a tool nobody uses by accident.
 *  - **Nothing here is a setting.** Lab mode is never written to NVS, is not
 *    in the display picker, and is left by a request or by a restart.
 *
 * It stays in the firmware rather than living on a branch. The coupling
 * between a face and its sensor is a property of one clock's geometry - the
 * same reason the panel letters are - so every clock needs this measured, not
 * just this one. What is learned here is meant to end up as a guided calibration
 * run; these are the primitives it will use.
 *
 * Behind expert mode, like everything else that can make the clock look broken.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.2
 * @created  22.8.2026
 * @updated  22.8.2026
 */
#ifndef LABROUTES_H
#define LABROUTES_H

#include <Arduino.h>

// Most frames one sweep may carry. A row-and-column scan is 21, a cell by cell
// scan of the whole panel is 110; the cap is what keeps the response inside a
// heap an OTA update also wants, and the clock inside a client's patience.
#define LAB_MAX_FRAMES 128

// Longest a single frame may be asked to settle for, and the longest a whole
// sweep may run. The web server is synchronous: while a sweep runs the clock
// answers nothing else, including the browser someone left open.
#define LAB_MAX_SETTLE_MS 2000
#define LAB_MAX_SWEEP_MS  90000

// What a frame may draw before it is refused, at 5 V. 7.5 W is 1.5 A, which is
// about 25 pixels of white - far more than any measurement here needs, and far
// enough below the supply to be safe.
//
// **Refused, not dimmed.** FastLED's own cap works by scaling the global
// brightness down, so a frame over budget comes out as a darker frame than was
// asked for and every number taken from it is quietly wrong. An instrument
// that silently changes the thing it is measuring is worse than one that says
// no. The number is not theoretical: the whole face at full white browned the
// clock out and reset it on the first try.
#define LAB_MAX_DRAW_MW 7500

namespace Lab
{
    /** Hangs the /lab handlers on the server. Call from setup(). */
    void begin();

    /**
     * True while the lab owns the strip.
     *
     * loop() asks, and leaves the frame buffer and the driver alone when the
     * answer is yes - otherwise the next tick would overwrite whatever a
     * script has just written, and every measurement would be of the clock
     * face rather than of the frame under test.
     */
    bool active();
}

#endif
