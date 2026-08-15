/**
 * LDR
 * Class for reading a light dependent resistor.
 *
 * @mc       ESP32S3
 * @author   Christian Aschoff / caschoff _AT_ mac _DOT_ com
 * @version  2.0
 * @created  18.3.2012
 * @updated  15.8.2026
 *
 * Version history:
 * V 1.1:  - Optimised for memory usage.
 * V 1.2:  - Improved debugging.
 * V 1.3:  - Clamping of the LDR values when autoscale == false.
 * V 1.4:  - The LDR now maps the values itself, which reduces flicker in awkward lighting.
 * V 1.5:  - The LDR returns values between 0 and 100%, which is easier to understand.
 * V 1.6:  - Added hysteresis so that borderline lighting does not cause flicker.
 * V 1.7:  - Introduced isInverted.
 * V 1.8:  - Removed support for the old Arduino IDE (up to 1.0.6).
 * V 2.0:  - Consolidated for ESP32-S3 / WS2812B, comments translated to English.
 */
#ifndef LDR_H
#define LDR_H

#include "Arduino.h"
#include <BH1750.h>

#define LDR_CHECK_RATE 200
//#define mod(x, y) ((x%y+y)%y)

class LDR {
public:
    LDR();
    float lightLevel();
    byte  calculateDisplayBrightness();

private:
    BH1750 lightMeter;
    
    byte lastLDRValue;
    int linearSensorValues[10];
    int linearDisplayValues[10];
    int linearPoints;
    float linearSlope;
    float linearOffset;
};

#endif
