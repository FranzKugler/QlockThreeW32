/**
 * OtaUpdate
 * Everything that puts a new image on the clock, and nothing else.
 *
 * Three ways in, all handled here: an image uploaded through the web UI, the
 * manifest of a release channel polled on a timer, and the restart that has to
 * follow either. `espota` is not here - that one is ArduinoOTA's own affair and
 * is started in setup().
 *
 * The module keeps its own state and reaches out to exactly two things it does
 * not own, the web server it hangs its routes on and the settings that hold
 * the channel. What it needs beyond that - whether the network is up, what the
 * local hour is - is passed in, so nothing in here has to know about WiFi
 * handling or timezones.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  17.8.2026
 * @updated  17.8.2026
 */
#ifndef OTAUPDATE_H
#define OTAUPDATE_H

#include "Arduino.h"

namespace Ota
{
    /**
     * Reads the version of the web UI out of the filesystem and registers the
     * /ota routes. Call once from setup(), after LittleFS is mounted.
     */
    void begin();

    /**
     * The periodic check of the release channel and, if it is switched on, the
     * automatic install. Call from loop().
     *
     * @param online     whether the clock currently has a network
     * @param localHour  local hour, 0..23, used for the night window
     */
    void poll(bool online, int localHour);

    /**
     * Asks for a restart a moment from now, late enough that the HTTP response
     * being written right now still reaches the browser. Also used by the
     * rename in the WLAN tab, which is not an update but needs the same delay.
     */
    void scheduleRestart();

    /** True from the moment a restart is asked for until it happens. */
    bool restartPending();

    /** True once the moment has come. loop() does the restarting itself. */
    bool restartDue();
}

#endif
