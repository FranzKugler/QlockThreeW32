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
#ifndef RENDERER_H
#define RENDERER_H

#include <Arduino.h>

#define LANGUAGE_DE_DE 0
#define LANGUAGE_DE_SW 1
#define LANGUAGE_DE_BA 2
#define LANGUAGE_DE_SA 3
#define LANGUAGE_CH    4
#define LANGUAGE_EN    5
#define LANGUAGE_FR    6
#define LANGUAGE_IT    7
#define LANGUAGE_NL    8
#define LANGUAGE_ES    9
// Ten languages, numbered 0..9 - this said 9, which is the highest number and
// not the count. Nothing reads it yet; a bounds check that did would have cut
// off Spanish.
#define LANGUAGE_COUNT 10

class Renderer {
public:
    Renderer();

    void setMinutes(char hours, byte minutes, byte language, word matrix[16]);
    void setCorners(byte minutes, boolean cw, word matrix[16]);

    void cleanWordsForAlarmSettingMode(byte language, word matrix[16]);

    void scrambleScreenBuffer(word matrix[16]);
    void clearScreenBuffer(word matrix[16]);
    void setAllScreenBuffer(word matrix[16]);

private:
    void setHours(char hours, boolean glatt, byte language, word matrix[16]);

    // Spezialfaelle
    void FR_hours(byte hours, word matrix[16]);
    void IT_hours(byte hours, word matrix[16]);
    void ES_hours(byte hours, word matrix[16]);
};

#endif
