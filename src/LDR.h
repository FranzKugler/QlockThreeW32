/**
 * LDR
 * Klasse fuer den Zugriff auf einen lichtabhaengigen Widerstand.
 *
 * @mc       Arduino/RBBB
 * @autor    Christian Aschoff / caschoff _AT_ mac _DOT_ com
 * @version  1.8
 * @created  18.3.2012
 * @updated  16.2.2015
 *
 * Versionshistorie:
 * V 1.1:  - Optimierung hinsichtlich Speicherbedarf.
 * V 1.2:  - Verbessertes Debugging.
 * V 1.3:  - Beschraenkund der LDR-Werte bei autoscale == false.
 * V 1.4:  - Der LDR mapped die Werte jetzt selbst, dadurch wird flackern bei unguenstigen Lichtverhaeltnissen vermindert.
 * V 1.5:  - Der LDR gibt Werte zwischen 0 und 100% zurueck, das ist besser verstaendlich.
 * V 1.6:  - Hysterese eingefuert, damit bei kippeligen Lichtverhaeltnissen kein Flackern auftritt.
 * V 1.7:  - isInverted eingefuehrt.
 * V 1.8:  - Unterstuetzung fuer die alte Arduino-IDE (bis 1.0.6) entfernt.
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
