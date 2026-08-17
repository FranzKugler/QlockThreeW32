/**
 * DisplayModes
 * What the face can be showing.
 *
 * Shared vocabulary rather than a module: main .cpp renders according to these
 * and WebRoutes.cpp accepts them from the web UI, so they cannot live in
 * either. The numbers are the ones the web UI sends - see MODE_VALUES in
 * web/src/sections/Display.svelte, which has to agree with this list.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  17.8.2026
 * @updated  17.8.2026
 */
#ifndef DISPLAYMODES_H
#define DISPLAYMODES_H

#include "Arduino.h"

#define STD_MODE_BLANK      0
#define STD_MODE_NIGHT      0
#define STD_MODE_NORMAL     1
#define STD_MODE_SECONDS    2
#define EXT_MODE_TEST       3
// 4 and 5 were the uptime and DCF-sync-age displays, both left over from the
// AVR days when a DCF77 receiver drove the clock and it crashed often enough
// to want hours-since-boot on the face. Neither had anything to report any
// more. The numbers are deliberately not reused: a clock updating from 2.0.1
// may still have 4 stored as its mode.
#define EXT_MODE_NORMAL_WIFISTATUS 6

/**
 * True for a mode the render switch still has a case for. Defined in
 * main .cpp, next to the switch it has to agree with.
 */
bool isKnownMode(byte candidate);

#endif
