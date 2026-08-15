/**
 * Zahlen
 * Definition of the digits for the QLOCKTWO seconds display.
 * Like the words, the digits are bit masks for the matrix.
 *
 * @mc       ESP32S3
 * @autor    Christian Aschoff / caschoff _AT_ mac _DOT_ com
 * @version  2.0
 * @created  18.2.2011
 * @updated  15.8.2026
 *
 * Version history:
 * V 1.1:  - Added A/M for switching the LDR between auto and manual (thanks to Alexander).
 * V 1.2:  - Moved the bitmaps into PROGMEM.
 * V 1.3:  - Moved the letters from V 1.1 into their own file and completed the alphabet.
 * V 2.0:  - Consolidated for ESP32-S3 / WS2812B, comments translated to English.
 */
#ifndef ZAHLEN_H
#define ZAHLEN_H

extern const char ziffern[][7] PROGMEM;
const char ziffern[][7] = {
    { // 0:0
        0b00001110,
        0b00010001,
        0b00010011,
        0b00010101,
        0b00011001,
        0b00010001,
        0b00001110
    }
    ,
    { // 1:1
        0b00000100,
        0b00001100,
        0b00000100,
        0b00000100,
        0b00000100,
        0b00000100,
        0b00001110
    }
    ,
    { // 2:2
        0b00001110,
        0b00010001,
        0b00000001,
        0b00000010,
        0b00000100,
        0b00001000,
        0b00011111
    }
    ,
    { // 3:3
        0b00011111,
        0b00000010,
        0b00000100,
        0b00000010,
        0b00000001,
        0b00010001,
        0b00001110
    }
    ,
    { // 4:4
        0b00000010,
        0b00000110,
        0b00001010,
        0b00010010,
        0b00011111,
        0b00000010,
        0b00000010
    }
    ,
    { // 5:5
        0b00011111,
        0b00010000,
        0b00011110,
        0b00000001,
        0b00000001,
        0b00010001,
        0b00001110
    }
    ,
    { // 6:6
        0b00000110,
        0b00001000,
        0b00010000,
        0b00011110,
        0b00010001,
        0b00010001,
        0b00001110
    }
    ,
    { // 7:7
        0b00011111,
        0b00000001,
        0b00000010,
        0b00000100,
        0b00001000,
        0b00001000,
        0b00001000
    }
    ,
    { // 8:8
        0b00001110,
        0b00010001,
        0b00010001,
        0b00001110,
        0b00010001,
        0b00010001,
        0b00001110
    }
    ,
    { // 9:9
        0b00001110,
        0b00010001,
        0b00010001,
        0b00001111,
        0b00000001,
        0b00000010,
        0b00001100
    }
};

#endif
