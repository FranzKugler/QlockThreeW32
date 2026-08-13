/**
 * LedDriverWS2812FastLED
 * Implementierung auf der Basis von WS2812B-Streifen wie sie die Adafruit-Neo-Pixel verwenden.
 *
 * Es lohnt sich in jedem Fall, den UeberGuide von Adafruit zu lesen:
 * https://learn.adafruit.com/adafruit-neopixel-uberguide/overview
 *
 * @mc       Arduino/RBBB
 * @autor    Christian Aschoff / caschoff _AT_ mac _DOT_ com
 * @version  1.2
 * @created  5.1.2015
 * @updated  16.2.2015
 *
 * Versionshistorie:
 * V 1.0:  - Erstellt.
 * V 1.1:  - Getter fuer Helligkeit nachgezogen.
 * V 1.2:  - Unterstuetzung fuer die alte Arduino-IDE (bis 1.0.6) entfernt.
 *
 * Verkabelung: Einspeisung oben links, dann schlangenfoermig runter,
 * dann Ecke unten links, oben links, oben rechts, unten rechts.
 *
 */
#ifndef LED_DRIVER_WS2812_FASTLED_H
#define LED_DRIVER_WS2812_FASTLED_H

#define FASTLED_INTERRUPT_RETRY_COUNT 1

#include "Arduino.h"
#include "LedDriver.h"

#define FASTLED_INTERNAL
#define FASTLED_ALLOW_INTERRUPTS 0
#include "FastLED.h"

#define NUM_PIXEL 114

class LedDriverWS2812FastLED : public LedDriver {
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

private:
    byte _brightnessInPercent;
    boolean _dirty;
    boolean _colorCorners;
    boolean _cw;
    byte _minute;
    byte _second;
    byte _funkStatus;

    void _setPixel(byte x, byte y, CRGB c);
 	void _setPixel(byte num, CRGB c);

    uint32_t _wheel(byte wheelPos);

    byte _brightnessScaleColor(byte colorPart);

  	CRGB *_leds;
};

#endif
