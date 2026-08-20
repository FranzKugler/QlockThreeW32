/**
 * Renderer
 * This class renders the words onto the matrix.
 *
 * @mc       ESP32S3
 * @author   Christian Aschoff / caschoff _AT_ mac _DOT_ com
 * @version  2.0
 * @created  21.1.2013
 * @updated  15.8.2026
 *
 * Version history:
 * V 1.0:  - Created.
 * V 1.1:  - Added Spanish.
 * V 1.2:  - setMinutes: hours changed to char so time zone offsets work.
 * V 1.3:  - Merged all German variants to save space.
 *         - Fixed a bug in the Italian rendering.
 * V 1.4:  - Hour clamping (which exists because of the time zone offset) widened to 0 <= h <= 24, thanks to a hint from the forum.
 * V 1.5:  - Removed support for the old Arduino IDE (up to 1.0.6).
 * V 1.6:  - Hour clamping to 0 <= h <= 24 also applied in setHours, see http://diskussion.christians-bastel-laden.de/viewtopic.php?f=17&t=2028
 * V 2.0:  - Consolidated for ESP32-S3 / WS2812B, comments translated to English.
 */
#include "Renderer.h"
#include "languages/Language.h"

// 
// #define DEBUG
#include "Debug.h"

Renderer::Renderer() {
}

/**
 * Produce a random pattern (for testing the LEDs)
 */
void Renderer::scrambleScreenBuffer(word matrix[16]) {
    for (byte i = 0; i < 16; i++) {
        matrix[i] = random(65536);
    }
}

/**
 * Clear the matrix (to save power, improve DCF77
 * reception etc.)
 */
void Renderer::clearScreenBuffer(word matrix[16]) {
    for (byte i = 0; i < 16; i++) {
        matrix[i] = 0;
    }
}

/**
 * Switch the whole matrix on (for testing the LEDs)
 */
void Renderer::setAllScreenBuffer(word matrix[16]) {
    for (byte i = 0; i < 16; i++) {
        matrix[i] = 65535;
    }
}

/**
 * Sets the word minutes, depending on hours/minutes.
 */
/**
 * Lights the words for this time.
 *
 * Every language renders itself; see Language.h. What used to be a 1300 line
 * switch over language and minute is now the lookup below.
 */
void Renderer::setMinutes(char hours, byte minutes, byte language, word matrix[16]) {
    const Language *plugin = Languages::find(language);
    if (plugin == nullptr) return;   // a number NVS holds that no longer exists

    // Clamped once here rather than in every language: the time zone offset
    // can push the hour outside the day, and the rules below add an hour of
    // their own on top of that.
    while (hours < 0) {
        hours += 12;
    }
    while (hours > 24) {
        hours -= 12;
    }

    Face face(plugin->words, plugin->wordCount, matrix);
    plugin->render(hours, minutes, face);
}

void Renderer::setCorners(byte minutes, boolean cw, word matrix[16]) {
    if (cw) {
        // clockwise
        switch (minutes % 5) {
            case 0:
                break;
            case 1:
                matrix[1] |= 0b0000000000011111; // 1
                break;
            case 2:
                matrix[1] |= 0b0000000000011111; // 1
                matrix[0] |= 0b0000000000011111; // 2
                break;
            case 3:
                matrix[1] |= 0b0000000000011111; // 1
                matrix[0] |= 0b0000000000011111; // 2
                matrix[3] |= 0b0000000000011111; // 3
                break;
            case 4:
                matrix[1] |= 0b0000000000011111; // 1
                matrix[0] |= 0b0000000000011111; // 2
                matrix[3] |= 0b0000000000011111; // 3
                matrix[2] |= 0b0000000000011111; // 4
                break;
        }
    } else {
        // counter-clockwise
        switch (minutes % 5) {
            case 0:
                break;
            case 1:
                matrix[0] |= 0b0000000000011111; // 1
                break;
            case 2:
                matrix[0] |= 0b0000000000011111; // 1
                matrix[1] |= 0b0000000000011111; // 2
                break;
            case 3:
                matrix[0] |= 0b0000000000011111; // 1
                matrix[1] |= 0b0000000000011111; // 2
                matrix[2] |= 0b0000000000011111; // 3
                break;
            case 4:
                matrix[0] |= 0b0000000000011111; // 1
                matrix[1] |= 0b0000000000011111; // 2
                matrix[2] |= 0b0000000000011111; // 3
                matrix[3] |= 0b0000000000011111; // 4
                break;
        }
    }
}

/**
 * In alarm setting mode certain words have to go, e.g. "ES IST" in German.
 */
