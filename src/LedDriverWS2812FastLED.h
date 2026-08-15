/**
 * LedDriverWS2812FastLED
 * Implementation based on WS2812B strips, as used by the Adafruit NeoPixels.
 *
 * Adafruit's UeberGuide is well worth reading:
 * https://learn.adafruit.com/adafruit-neopixel-uberguide/overview
 *
 * @mc       ESP32S3
 * @autor    Christian Aschoff / caschoff _AT_ mac _DOT_ com
 * @version  2.0
 * @created  5.1.2015
 * @updated  15.8.2026
 *
 * Version history:
 * V 1.0:  - Created.
 * V 1.1:  - Brightness getter brought in line.
 * V 1.2:  - Removed support for the old Arduino IDE (up to 1.0.6).
 * V 2.0:  - Consolidated for ESP32-S3 / WS2812B, comments translated to English.
 *
 * Wiring: fed in at the top left, then serpentine downwards,
 * then the corners: bottom left, top left, top right, bottom right.
 *
 */
#ifndef LED_DRIVER_WS2812_FASTLED_H
#define LED_DRIVER_WS2812_FASTLED_H

#define FASTLED_INTERRUPT_RETRY_COUNT 1

#include "Arduino.h"

#define FASTLED_INTERNAL
#define FASTLED_ALLOW_INTERRUPTS 0
#include "FastLED.h"

#define NUM_PIXEL 114

class LedDriverWS2812FastLED {
public:
	LedDriverWS2812FastLED(void);

    void init();

    void printSignature();

    void writeScreenBufferToMatrix(word matrix[16], boolean onChange);
    void updateFunkStatus(byte status);

	void setColorHS(byte hue, byte sat);
    void setColorCorners(boolean flag, boolean cw) {_colorCorners = flag; _cw = cw;}
    void setTimeForCorners(byte Minute, byte Second) {_minute = Minute; _second = Second;}

	void setBrightness(byte brightnessInPercent);
    byte getBrightness();

    void setLinesToWrite(byte linesToWrite);

    void shutDown();
    void wakeUp();

    void clearData();

    void setColor(byte red, byte green, byte blue);
    byte getRed();
    byte getGreen();
    byte getBlue();

    void setPixelInScreenBuffer(byte x, byte y, word matrix[16]);
    boolean getPixelFromScreenBuffer(byte x, byte y, word matrix[16]);

private:
    byte _brightnessInPercent;
    boolean _dirty;
    boolean _colorCorners;
    boolean _cw;
    byte _minute;
    byte _second;
    byte _funkStatus;
    byte _red, _green, _blue;

    void _setPixel(byte x, byte y, CRGB c);
 	void _setPixel(byte num, CRGB c);

    uint32_t _wheel(byte wheelPos);

    byte _brightnessScaleColor(byte colorPart);

  	CRGB *_leds;
};

#endif
