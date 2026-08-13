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
 */

#include "LDR.h"
#include <Wire.h>

#include <RemoteDebug.h>
extern RemoteDebug Debug;

/**
 * Initialisierung 
 */
LDR::LDR()
{
    Wire.begin(SDA, SCL);
    lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE_2);
}

/**
 * Welchen Wert hat der Lichtsensor (in LUX)
 */
float LDR::lightLevel() 
{
    return lightMeter.readLightLevel();
}

/*
 * 
 */
byte LDR::calculateDisplayBrightness()
{
    float ldrValue = lightLevel();
    byte actualBrightness;
    if (ldrValue != lastLDRValue)
    {
        lastLDRValue = ldrValue;
        actualBrightness = constrain((byte)(linearSlope*ldrValue + linearOffset), 5, 100);
        debugI("Helligkeit angepasst: LDR=%f, Brightness=%d\n", ldrValue, actualBrightness);
    } 
    return actualBrightness;  
}

/*
void LDR::setNewReference(int ldrValue, int displayBrightness)
{
    if(linearPoints < 10)
    {
        linearSensorValues[linearPoints] = ldrValue;
        linearDisplayValues[linearPoints] = displayBrightness;
        linearPoints++; if(linearPoints>9) linearPoints = 9;
    }

    if(linearPoints >= 2)
    {
        int xy=0, x=0, y=0, x2=0;
        for(int i=0; i<linearPoints; i++)
        {
            xy += linearSensorValues[i] * linearDisplayValues[i];
            x  += linearSensorValues[i];
            y  += linearDisplayValues[i];
            x2 += linearSensorValues[i] * linearSensorValues[i];
        }
        debugI("x=%d, y=%d, xy=%d, x2=%d, n=%d\n", x, y, xy, x2, linearPoints);
        linearSlope = (float)(linearPoints*xy - x*y) / (float)(linearPoints*x2 - x*x);
        linearOffset = (float)y/linearPoints - linearSlope*((float)x/linearPoints);
        debugI("m = %f, t = %f\n", linearSlope, linearOffset);
    }    
}
*/