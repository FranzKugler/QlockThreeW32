/**
 * LedDriverWS2812FastLED
 * Implementation based on WS2812B strips, as used by the Adafruit NeoPixels.
 *
 * Adafruit's UeberGuide is well worth reading:
 * https://learn.adafruit.com/adafruit-neopixel-uberguide/overview
 *
 * @mc       ESP32S3
 * @author   Christian Aschoff / caschoff _AT_ mac _DOT_ com
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

    /**
     * The hue one corner shows in the coloured-corner mode, by its place on
     * the face - 0 top left, 1 top right, 2 bottom right, 3 bottom left, which
     * is reading order. False for a dark corner, and `hue` is then untouched.
     *
     * Public because GET /panel needs it: the web UI's preview drew the
     * corners in the plain display colour, having no way to ask what they
     * actually show. The implementation says the rest.
     */
    boolean cornerHue(byte corner, byte &hue) const;

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

    // ------ direct access, for the lab interface ------
    //
    // Raw means raw: no gamma, no brightness scaling, no colour setting. A
    // measuring instrument has to put out the number it was given, or the
    // reading measures this class instead of the clock.

    /**
     * Where a letter cell sits on the strip.
     *
     * The authority on the wiring, and the only place that computes it. The
     * strip starts at the bottom right (the R of the German panel), meanders
     * left, up one row, back right, and ends at the top right (the F); the
     * four corner LEDs follow it as 110..113 in the order bottom right, top
     * right, top left, bottom left.
     *
     * @param row  0 at the top, 9 at the bottom
     * @param col  0 at the left, 10 at the right
     * @return     0..109, or 255 for a cell that does not exist
     */
    static byte physicalFor(byte row, byte col);

    /** Writes one physical pixel. Does not show; call showRaw() when done. */
    void setPixelRaw(byte index, CRGB colour);

    /** What is in one physical pixel right now. */
    CRGB getPixelRaw(byte index) const;

    /** All pixels black. Does not show. */
    void clearRaw();

    /** Pushes whatever is in the buffer to the strip. */
    void showRaw();

    /**
     * Milliwatts the current buffer would draw, by FastLED's own estimate.
     *
     * Worth having next to a measurement: the power cap works by scaling the
     * global brightness down, so a frame over budget is not the frame that was
     * asked for, and a reading taken from it would be quietly wrong.
     */
    uint32_t estimatedDrawMilliwatts() const;

    void setPixelInScreenBuffer(byte x, byte y, word matrix[16]);
    boolean getPixelFromScreenBuffer(byte x, byte y, word matrix[16]);

private:
    byte _brightnessInPercent = 0;
    // The setting after gamma, 0..255, which is what actually drives the LEDs.
    // Kept alongside rather than computed per pixel: it only changes when the
    // setting does.
    byte _brightnessScaled = 0;
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
    static byte _gammaScale(byte percent);

  	CRGB *_leds;
};

#endif
