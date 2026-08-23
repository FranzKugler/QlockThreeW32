/**
 * Coupling
 * How much of what the light sensor reads is the clock's own face.
 *
 * The sensor sits behind the panel, and so do the LEDs. On the clock this was
 * written for, taking the display from 20 % to 100 % with the room untouched
 * moved the reading from 8.29 lx to 22.80 lx - **+175 %**, or 0.44 decades of
 * light produced by nothing but the brightness slider. On the default curve
 * that asks for 24 % more brightness, which produces more light again. That is
 * a positive feedback loop, and it also poisons what the automatic learns: a
 * point kept ten seconds after a nudge is mostly the display's own
 * contribution at the brightness just chosen.
 *
 * Measured rather than reasoned about, through the /lab interface:
 *
 *  - The coupling is **local and steeply peaked**, not diffuse. The sensor's
 *    own cell reads 240 lx over dark; a whole row two away reads 0.002 lx.
 *    Ten cells out of a hundred and ten carry all of it.
 *  - It is **wavelength dependent**, and not slightly: at two cells' distance
 *    red carries almost twice as far as blue. Hence three coefficients per
 *    cell rather than one - a single white number would be about 9 % out on a
 *    green face.
 *  - It **superposes**, which is what makes a sum legitimate: white came out
 *    1.3 % from red plus green plus blue, and the word SIEBEN 0.9 % from the
 *    sum of its six cells.
 *  - The drive response is **neither linear nor a gamma**. Half drive gives
 *    0.48 of full, a quarter 0.216, an eighth 0.082, and 16 gives 0.024 where
 *    a proportional lamp would give 0.063. Nothing fits it, so it is a table.
 *    It is not a nicety: 20 % brightness through the display gamma comes out
 *    as a drive of about seven, so the dim hours live entirely in the part of
 *    the curve where proportionality is 22 % wrong and worse.
 *
 * With the model in place the same sweep moves 0.02 decades instead of 0.44 -
 * under one per cent of brightness, which is below the step the setting is
 * quantised to.
 *
 * **The numbers belong to one clock**, the same as its panel letters: they
 * depend on where the sensor was fitted and what sits between it and the LEDs.
 * They are therefore measured on the clock and stored in NVS, never compiled
 * in. A clock with nothing stored compensates nothing and behaves exactly as
 * it did before this existed - which is also the honest default, because a
 * guessed map would be worse than none.
 *
 * Produced by `python scripts/lab.py <clock> calibrate`, which uploads it.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.2
 * @created  23.8.2026
 * @updated  23.8.2026
 */
#ifndef COUPLING_H
#define COUPLING_H

#include <Arduino.h>

// Cells the map may hold. Ten carried everything above a thousandth of the
// peak on the clock this was measured on; the room is for a clock whose sensor
// sits further behind the sheet and spreads the light wider.
#define COUPLING_MAX_CELLS 24

// Points the drive response table may have. Thirteen were measured, from 255
// down to 4, below which the counts are in the noise.
#define COUPLING_MAX_LEVELS 20

namespace Coupling
{
    /** Loads the map from NVS. Call from setup(), after the driver. */
    void begin();

    /** True when a map is stored and usable. */
    bool available();

    /** How many cells it describes, for the read-out. */
    uint8_t cells();

    /**
     * What the face on the strip right now is putting into the sensor, in lux.
     *
     * Read off the driver's own pixels rather than off the frame buffer, so
     * the brightness and the colour are already in them and this needs to know
     * nothing about either. Answers 0 when no map is stored.
     *
     * Called from the sampling task on core 0 while the pixels are written on
     * core 1. There is no lock: the worst a torn read can produce is one
     * pixel from the previous frame, which is a fraction of a per cent of one
     * sample, and a lock here would be held across a hundred pixel reads on
     * the path that must never delay the strip.
     */
    float contribution();

    /**
     * Replaces the map from the JSON the calibration script produces, and
     * stores it. Returns false if it will not parse or holds nothing usable.
     */
    bool store(const String &json);

    /** The stored record as it went in, for the read-out. Empty when none. */
    String stored();

    /** Throws the map away; the clock stops compensating. */
    void reset();
}

#endif
