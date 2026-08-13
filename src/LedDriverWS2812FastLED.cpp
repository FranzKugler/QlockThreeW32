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
#include "LedDriverWS2812FastLED.h"

// #define DEBUG
// #include "Debug.h"

#include <RemoteDebug.h>
extern RemoteDebug Debug;

#define NUM_PIXEL 114
#ifndef LED_OUTPUT_PIN
	#define LED_OUTPUT_PIN 2
#endif

/**
 * Initialisierung.
 *
 * @param data Pin, an dem die Data-Line haengt.
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
 * init() wird im Hauptprogramm in init() aufgerufen.
 * Hier sollten die LED-Treiber in eine definierten
 * Ausgangszustand gebracht werden.
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
 * Den Bildschirm-Puffer auf die LED-Matrix schreiben.
 *
 * @param onChange: TRUE, wenn es Aenderungen in dem Bildschirm-Puffer gab,
 *                  FALSE, wenn es ein Refresh-Aufruf war.
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
			// wir muessen die Eck-LEDs umsetzten...
			if ((matrix[1] & 0b0000000000011111) == 0b0000000000011111)
			{
				//_setPixel(110, color); // 1
				_setPixel(110, color); // 1
			}
			if ((matrix[0] & 0b0000000000011111) == 0b0000000000011111)
			{
				//_setPixel(111, color); // 2
				_setPixel(111, color); // 2
			}
			if ((matrix[3] & 0b0000000000011111) == 0b0000000000011111)
			{
				//_setPixel(112, color); // 3
				_setPixel(112, color); // 3
			}
			if ((matrix[2] & 0b0000000000011111) == 0b0000000000011111)
			{
				//_setPixel(113, color); // 4
				_setPixel(113, color); // 4
			}
		}
		else
		{
			int brightness = _brightnessInPercent * 255 / 100;
			switch (_minute % 5)
			{
			case 0:
				if (_cw)
				{
					_leds[110] = CHSV(0, 0, 0);
					_leds[111] = CHSV(0, 0, 0);
					_leds[112] = CHSV(3 * _second, 255, brightness);
					_leds[113] = CHSV(0, 0, 0);
				}
				else
				{

				}
				break;
			case 1:
				_leds[110] = CHSV(0, 0, 0);
				_leds[111] = CHSV(3 * _second, 255, brightness);
				_leds[112] = CHSV(180, 255, brightness);
				_leds[113] = CHSV(0, 0, 0);
				break;
			case 2:
				_leds[110] = CHSV(3 * _second, 255, brightness);
				_leds[111] = CHSV(180, 255, brightness);
				_leds[112] = CHSV(180, 255, brightness);
				_leds[113] = CHSV(0, 0, 0);
				break;
			case 3:
				_leds[110] = CHSV(180, 255, brightness);
				_leds[111] = CHSV(180, 255, brightness);
				_leds[112] = CHSV(180, 255, brightness);
				_leds[113] = CHSV(3 * _second, 255, brightness);
				break;
			case 4:
				_leds[110] = CHSV(3 * _second, 255, brightness);
				_leds[111] = CHSV(3 * _second, 255, brightness);
				_leds[112] = CHSV(3 * _second, 255, brightness);
				_leds[113] = CHSV(3 * _second, 255, brightness);
				break;
			default:
				break;
			}
		}

		if (_funkStatus)
		{
			int brightness = _brightnessInPercent * 255 / 100;
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
* Die Farbe des Displays anpassen.
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
 * Die Helligkeit des Displays anpassen.
 *
 * @param brightnessInPercent Die Helligkeit.
 */
void LedDriverWS2812FastLED::setBrightness(byte brightnessInPercent)
{
	if (brightnessInPercent != _brightnessInPercent)
	{
		_brightnessInPercent = brightnessInPercent;
		_dirty = true;
	}
}

/**
 * Die aktuelle Helligkeit bekommen.
 */
byte LedDriverWS2812FastLED::getBrightness()
{
	return _brightnessInPercent;
}

/**
 * Anpassung der Groesse des Bildspeichers.
 *
 * @param linesToWrite Wieviel Zeilen aus dem Bildspeicher sollen
 *                     geschrieben werden?
 */
void LedDriverWS2812FastLED::setLinesToWrite(byte linesToWrite)
{}

/**
 * Das Display ausschalten.
 */
void LedDriverWS2812FastLED::shutDown()
{
	FastLED.clear();
	FastLED.show();
}

/**
 * Das Display einschalten.
 */
void LedDriverWS2812FastLED::wakeUp()
{}

/**
 * Den Dateninhalt des LED-Treibers loeschen.
 */
void LedDriverWS2812FastLED::clearData()
{
	FastLED.clear();
	FastLED.show();
}

/**
 * Einen X/Y-koordinierten Pixel in der Matrix setzen.
 */
void LedDriverWS2812FastLED::_setPixel(byte x, byte y, CRGB c)
{
	_setPixel((10 - x) + 11 * (9 - y), c);
}

/**
 * Einen Pixel im Streifen setzten (die Eck-LEDs sind am Ende).
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
 * Funktion fuer saubere 'Regenbogen'-Farben.
 * Kopiert aus den Adafruit-Beispielen (strand).
 */
uint32_t LedDriverWS2812FastLED::_wheel(byte wheelPos)
{
	return 0;
}

/**
 * Hilfsfunktion fuer das Skalieren der Farben.
 */
byte LedDriverWS2812FastLED::_brightnessScaleColor(byte colorPart)
{
	return map(_brightnessInPercent, 0, 100, 0, colorPart);
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
