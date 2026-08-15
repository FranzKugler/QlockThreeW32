/**
 * Debug
 * Class for more elegant debugging.
 *
 * @mc       ESP32S3
 * @author   Christian Aschoff / caschoff _AT_ mac _DOT_ com
 * @version  2.0
 * @created  21.1.2013
 * @updated  15.8.2026
 *
 * Version history:
 * V 1.0:  - Created.
 * V 1.1:  - Allowed two arguments.
 * V 2.0:  - Consolidated for ESP32-S3 / WS2812B, comments translated to English.
 */
#ifdef DEBUG
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINT2(x, y) Serial.print(x, y)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTLN2(x, y) Serial.println(x, y)
    #define DEBUG_FLUSH() Serial.flush()
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINT2(x, y)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTLN2(x, y)
    #define DEBUG_FLUSH()
#endif
