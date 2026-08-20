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
 * Wiring: fed in at the top left, then serpentine downwards, then the four
 * corner LEDs.
 *
 * The corner order in this comment used to read "bottom left, top left, top
 * right, bottom right" and that is not what the clock does. It is also not
 * worth restating here, because the path from a frame buffer row to a lit
 * corner passes through _setPixel(), which swaps 110 with 112 on top of the
 * mapping in writeScreenBufferToMatrix. The one place that says which row is
 * which corner of the face is Renderer::setCorners, and it says so from
 * having been checked against the clock.
 *
 */
#include "LedDriverWS2812FastLED.h"

// #define DEBUG
// #include "Debug.h"

// Debug and the debugX macros, plus the ring the web UI reads them out of.
#include "LogBuffer.h"

#define NUM_PIXEL 114

/*
 * The four corner LEDs, named by where they are on the face rather than by
 * where they sit on the strip. Which frame buffer row is which corner is
 * written down at Renderer::setCorners and comes from the clock; these are
 * that mapping followed through _setPixel(), which swaps 110 with 112:
 *
 *     matrix[1] top left      -> _setPixel(110) -> _leds[112]
 *     matrix[0] top right     -> _setPixel(111) -> _leds[111]
 *     matrix[3] bottom right  -> _setPixel(112) -> _leds[110]
 *     matrix[2] bottom left   -> _setPixel(113) -> _leds[113]
 *
 * Only the coloured corners need them: that mode writes the pixels itself
 * instead of going through the frame buffer, so it has no rows to read.
 */
#define CORNER_TOP_LEFT     112
#define CORNER_TOP_RIGHT    111
#define CORNER_BOTTOM_RIGHT 110
#define CORNER_BOTTOM_LEFT  113

// The colour a corner settles on once the next one has lit; the newest one
// is the one that moves. Cyan on FastLED's wheel.
#define CORNER_SETTLED_HUE 180

#ifndef LED_OUTPUT_PIN
	#define LED_OUTPUT_PIN 2
#endif

/**
 * Initialisation.
 *
 * @param data pin the data line is attached to.
 */
LedDriverWS2812FastLED::LedDriverWS2812FastLED(void)
{
	_leds = (CRGB*)malloc(NUM_PIXEL*sizeof(CRGB));
	//FastLED.addLeds<NEOPIXEL, 16>(_leds, NUM_PIXEL);
	
	FastLED.addLeds<NEOPIXEL, LED_OUTPUT_PIN>(_leds, NUM_PIXEL);
	FastLED.setMaxPowerInVoltsAndMilliamps(5, 2500);

	setColor(250, 255, 200);
	setColorCorners(false, true);
	updateFunkStatus(0);
}

/**
 * init() is called from the main program's init().
 * The LED driver should be brought into a defined
 * initial state here.
 */
void LedDriverWS2812FastLED::init()
{
	setBrightness(50);
	clearData();
	wakeUp();
}

void LedDriverWS2812FastLED::printSignature()
{
	debugI("FastLED - WS2812B on Pin %d\n", LED_OUTPUT_PIN);
}

/**
 * Write the frame buffer to the LED matrix.
 *
 * @param onChange TRUE if the frame buffer changed,
 *                  FALSE if this was a refresh call.
 */
void LedDriverWS2812FastLED::writeScreenBufferToMatrix(word matrix[16], boolean onChange)
{
	if (onChange || _dirty)
	{
		_dirty = false;
		FastLED.clear();

		CRGB color = CRGB(_brightnessScaleColor(getRed()), _brightnessScaleColor(getGreen()), _brightnessScaleColor(getBlue()));

		for (byte y = 0; y < 10; y++)
		{
			for (byte x = 5; x < 16; x++)
			{
				word t = 1 << x;
				if ((matrix[y] & t) == t)
				{
					_setPixel(15 - x, y, color);
				}
			}
		}

		if (!_colorCorners)
		{
			// The corner rows carry no letters, so they are handed to _setPixel
			// whole rather than column by column. Which row is which corner is
			// stated at Renderer::setCorners; the numbers below only have to
			// agree with it, and were checked against the clock.
			if ((matrix[1] & 0b0000000000011111) == 0b0000000000011111)
			{
				_setPixel(110, color); // top left
			}
			if ((matrix[0] & 0b0000000000011111) == 0b0000000000011111)
			{
				_setPixel(111, color); // top right
			}
			if ((matrix[3] & 0b0000000000011111) == 0b0000000000011111)
			{
				_setPixel(112, color); // bottom right
			}
			if ((matrix[2] & 0b0000000000011111) == 0b0000000000011111)
			{
				_setPixel(113, color); // bottom left
			}
		}
		else
		{
			/*
			 * The coloured corners: the same count of minutes the plain corners
			 * show, but with the newest one cycling through the hues once a
			 * minute and the ones before it held at a fixed colour.
			 *
			 * It has to count the same as Renderer::setCorners does, and used
			 * to count one too many - a remainder of 0 lit a corner, so every
			 * corner came on a minute early and the fourth never went out. It
			 * also ignored the direction: the only branch that asked about it
			 * had an empty else, so counter-clockwise ran clockwise. Both are
			 * gone now that the order is a table rather than a switch.
			 *
			 * This is the one place that writes corner pixels directly. It
			 * cannot go through the frame buffer, because the hue differs per
			 * corner and a row is one bit.
			 */
			const byte clockwise[4] = {
				CORNER_TOP_LEFT, CORNER_TOP_RIGHT, CORNER_BOTTOM_RIGHT, CORNER_BOTTOM_LEFT
			};
			const byte counterClockwise[4] = {
				CORNER_TOP_RIGHT, CORNER_TOP_LEFT, CORNER_BOTTOM_LEFT, CORNER_BOTTOM_RIGHT
			};
			const byte *order = _cw ? clockwise : counterClockwise;

			int brightness = _brightnessScaled;  // same gamma as the letters
			byte lit = _minute % 5;

			for (byte i = 0; i < 4; i++)
			{
				if (i + 1 < lit)
				{
					_leds[order[i]] = CHSV(CORNER_SETTLED_HUE, 255, brightness);
				}
				else if (i + 1 == lit)
				{
					_leds[order[i]] = CHSV(3 * _second, 255, brightness);
				}
				else
				{
					_leds[order[i]] = CHSV(0, 0, 0);
				}
			}
		}

		if (_funkStatus)
		{
			int brightness = _brightnessScaled;  // same gamma as the letters
			if ((_funkStatus & 0x0F) == 1)
				color = CHSV(96, 255, brightness); 
			else if ((_funkStatus & 0x0F) == 2)
				color = CHSV(0, 255, brightness); 
			else
				color = CHSV(0,0,0);
				
			if ((_funkStatus & 0x80) && (_second % 2))
				color = CHSV(0,0,0);

			_setPixel(3, 3, color);
			_setPixel(4, 3, color);
			_setPixel(5, 3, color);
			_setPixel(6, 3, color);
		}
		FastLED.show();
	}
}

void LedDriverWS2812FastLED::updateFunkStatus(byte status)
{
	_funkStatus = status;
}

/**
* Adjust the display colour.
*
* @param hue Hue
* @param sat Sat
*/
void LedDriverWS2812FastLED::setColorHS(byte hue, byte sat)
{
	CRGB pixel = CHSV(hue, sat, 255);
	setColor(pixel.r, pixel.g, pixel.b);
}


/**
 * Adjust the display brightness.
 *
 * @param brightnessInPercent the brightness.
 */
void LedDriverWS2812FastLED::setBrightness(byte brightnessInPercent)
{
	if (brightnessInPercent != _brightnessInPercent)
	{
		_brightnessInPercent = brightnessInPercent;
		_brightnessScaled = _gammaScale(brightnessInPercent);
		_dirty = true;
	}
}

/**
 * The setting as a drive value, with gamma.
 *
 * Perceived brightness follows roughly a power law, so driving the LEDs
 * proportionally to the setting does not feel proportional: half way up the
 * slider looked far brighter than half, and everything interesting happened in
 * the bottom third. Raising the setting to 2.2 spreads the useful range over
 * the whole slider.
 *
 * The floor matters as much as the curve: without it a setting of 1 to 3 per
 * cent rounds to zero and the clock goes dark while the web UI says it is on.
 */
byte LedDriverWS2812FastLED::_gammaScale(byte percent)
{
	if (percent == 0) return 0;
	if (percent >= 100) return 255;

	long value = lroundf(255.0f * powf(percent / 100.0f, 2.2f));
	if (value < 1) value = 1;
	return (byte)value;
}

/**
 * Get the current brightness.
 */
byte LedDriverWS2812FastLED::getBrightness()
{
	return _brightnessInPercent;
}

/**
 * Adjust the size of the frame buffer.
 *
 * @param linesToWrite how many rows of the frame buffer
 *                     should be written?
 */
void LedDriverWS2812FastLED::setLinesToWrite(byte linesToWrite)
{}

/**
 * Switch the display off.
 */
void LedDriverWS2812FastLED::shutDown()
{
	FastLED.clear();
	FastLED.show();
}

/**
 * Switch the display on.
 */
void LedDriverWS2812FastLED::wakeUp()
{}

/**
 * Clear the LED driver's data.
 */
void LedDriverWS2812FastLED::clearData()
{
	FastLED.clear();
	FastLED.show();
}

/**
 * Set a pixel in the matrix by x/y coordinate.
 */
void LedDriverWS2812FastLED::_setPixel(byte x, byte y, CRGB c)
{
	_setPixel((10 - x) + 11 * (9 - y), c);
}

/**
 * Set a pixel in the strip (the corner LEDs come last).
 */
void LedDriverWS2812FastLED::_setPixel(byte num, CRGB c)
{
	if (num < 110)
	{
		if ((num / 11) % 2 == 0)
		{
			_leds[num] = c;
		}
		else
		{
			_leds[((num / 11) * 11) + 10 - (num % 11)] = c;
		}
	}
	else
	{
		switch (num)
		{
		case 110:
			_leds[112] = c;
			break;
		case 111:
			_leds[111] = c;
			break;
		case 112:
			_leds[110] = c;
			break;
		case 113:
			_leds[113] = c;
			break;
		}
	}
}

/**
 * Function for clean 'rainbow' colours.
 * Copied from the Adafruit examples (strand).
 */
uint32_t LedDriverWS2812FastLED::_wheel(byte wheelPos)
{
	return 0;
}

/**
 * Helper for scaling the colours.
 */
byte LedDriverWS2812FastLED::_brightnessScaleColor(byte colorPart)
{
	return (byte)(((int)colorPart * (int)_brightnessScaled + 127) / 255);
}

void LedDriverWS2812FastLED::setColor(byte red, byte green, byte blue) {
    _red = red;
    _green = green;
    _blue = blue;
}

byte LedDriverWS2812FastLED::getRed() {
    return _red;
}

byte LedDriverWS2812FastLED::getGreen() {
    return _green;
}

byte LedDriverWS2812FastLED::getBlue() {
    return _blue;
}

void LedDriverWS2812FastLED::setPixelInScreenBuffer(byte x, byte y, word matrix[16]) {
    matrix[y] |= 0b1000000000000000 >> x;
}

boolean LedDriverWS2812FastLED::getPixelFromScreenBuffer(byte x, byte y, word matrix[16]) {
    return (matrix[y] & (0b1000000000000000 >> x)) == (0b1000000000000000 >> x);
}
