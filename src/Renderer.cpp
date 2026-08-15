/**
 * Renderer
 * This class renders the words onto the matrix.
 *
 * @mc       ESP32S3
 * @author   Christian Aschoff / caschoff _AT_ mac _DOT_ com
 * @version  2.0
 * @created  21.1.2013
 * @updated  15.8.2026
 *
 * Version history:
 * V 1.0:  - Created.
 * V 1.1:  - Added Spanish.
 * V 1.2:  - setMinutes: hours changed to char so time zone offsets work.
 * V 1.3:  - Merged all German variants to save space.
 *         - Fixed a bug in the Italian rendering.
 * V 1.4:  - Hour clamping (which exists because of the time zone offset) widened to 0 <= h <= 24, thanks to a hint from the forum.
 * V 1.5:  - Removed support for the old Arduino IDE (up to 1.0.6).
 * V 1.6:  - Hour clamping to 0 <= h <= 24 also applied in setHours, see http://diskussion.christians-bastel-laden.de/viewtopic.php?f=17&t=2028
 * V 2.0:  - Consolidated for ESP32-S3 / WS2812B, comments translated to English.
 */
#include "Renderer.h"

#include "Woerter_DE.h"
// #include "Woerter_DE_MKF.h"
#include "Woerter_CH.h"
#include "Woerter_EN.h"
#include "Woerter_FR.h"
#include "Woerter_IT.h"
#include "Woerter_NL.h"
#include "Woerter_ES.h"

// #define DEBUG
#include "Debug.h"

Renderer::Renderer() {
}

/**
 * Produce a random pattern (for testing the LEDs)
 */
void Renderer::scrambleScreenBuffer(word matrix[16]) {
    for (byte i = 0; i < 16; i++) {
        matrix[i] = random(65536);
    }
}

/**
 * Clear the matrix (to save power, improve DCF77
 * reception etc.)
 */
void Renderer::clearScreenBuffer(word matrix[16]) {
    for (byte i = 0; i < 16; i++) {
        matrix[i] = 0;
    }
}

/**
 * Switch the whole matrix on (for testing the LEDs)
 */
void Renderer::setAllScreenBuffer(word matrix[16]) {
    for (byte i = 0; i < 16; i++) {
        matrix[i] = 65535;
    }
}

/**
 * Sets the word minutes, depending on hours/minutes.
 */
void Renderer::setMinutes(char hours, byte minutes, byte language, word matrix[16]) {
    while (hours < 0) {
        hours += 12;
    }
    while (hours > 24) {
        hours -= 12;
    }

    switch (language) {
            //
            // German: standard German
            //
        case LANGUAGE_DE_DE:
        case LANGUAGE_DE_SW:
        case LANGUAGE_DE_BA:
        case LANGUAGE_DE_SA:
            DE_ESIST;
			DEBUG_PRINT("Es ist ");
            switch (minutes / 5) {
                case 0:
                    // full hour
                    setHours(hours, true, language, matrix);
                    break;
                case 1:
                    // 5 past
                    DE_FUENF;
                    DE_NACH;
					DEBUG_PRINT("fuenf nach ");
                    setHours(hours, false, language, matrix);
                    break;
                case 2:
                    // 10 past
                    DE_ZEHN;
                    DE_NACH;
					DEBUG_PRINT("zehn nach ");
                    setHours(hours, false, language, matrix);
                    break;
                case 3:
                    // quarter past
                    if ((language == LANGUAGE_DE_SW) || (language == LANGUAGE_DE_SA)) {
                        DE_VIERTEL;
						DEBUG_PRINT("viertel ");
                        setHours(hours + 1, false, language, matrix);
                    } else {
                        DE_VIERTEL;
                        DE_NACH;
						DEBUG_PRINT("viertel nach ");
                        setHours(hours, false, language, matrix);
                    }
                    break;
                case 4:
                    // 20 past
                    if (language == LANGUAGE_DE_SA) {
                        DE_ZEHN;
                        DE_VOR;
                        DE_HALB;
						DEBUG_PRINT("zehn vor halb ");
                        setHours(hours + 1, false, language, matrix);
                    } else {
                        DE_ZWANZIG;
                        DE_NACH;
						DEBUG_PRINT("zwanzig nach ");
                        setHours(hours, false, language, matrix);
                    }
                    break;
                case 5:
                    // 5 to half
                    DE_FUENF;
                    DE_VOR;
                    DE_HALB;
					DEBUG_PRINT("fuenf vor halb ");
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 6:
                    // half
                    DE_HALB;
					DEBUG_PRINT("halb ");
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 7:
                    // 5 past half
                    DE_FUENF;
                    DE_NACH;
                    DE_HALB;
					DEBUG_PRINT("fuenf nach halb ");
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 8:
                    // 20 to
                    if (language == LANGUAGE_DE_SA) {
                        DE_ZEHN;
                        DE_NACH;
                        DE_HALB;
						DEBUG_PRINT("zehn nach halb ");
                        setHours(hours + 1, false, language, matrix);
                    } else {
                        DE_ZWANZIG;
                        DE_VOR;
						DEBUG_PRINT("zwanzig vor ");
                        setHours(hours + 1, false, language, matrix);
                    }
                    break;
                case 9:
                    // quarter to
                    if ((language == LANGUAGE_DE_SW) || (language == LANGUAGE_DE_BA) || (language == LANGUAGE_DE_SA)) {
                        DE_DREIVIERTEL;
						DEBUG_PRINT("dreiviertel ");
                        setHours(hours + 1, false, language, matrix);
                    } else {
                        DE_VIERTEL;
                        DE_VOR;
						DEBUG_PRINT("viertel vor ");
                        setHours(hours + 1, false, language, matrix);
                    }
                    break;
                case 10:
                    // 10 to
                    DE_ZEHN;
                    DE_VOR;
					DEBUG_PRINT("zehn vor ");
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 11:
                    // 5 to
                    DE_FUENF;
                    DE_VOR;
					DEBUG_PRINT("fuenf vor ");
                    setHours(hours + 1, false, language, matrix);
                    break;
            }
            break;
            //
            // Switzerland: Bernese German
            //
        case LANGUAGE_CH:
            CH_ESISCH;

            switch (minutes / 5) {
                case 0:
                    // full hour
                    setHours(hours, true, language, matrix);
                    break;
                case 1:
                    // 5 past
                    CH_FUEF;
                    CH_AB;
                    setHours(hours, false, language, matrix);
                    break;
                case 2:
                    // 10 past
                    CH_ZAEAE;
                    CH_AB;
                    setHours(hours, false, language, matrix);
                    break;
                case 3:
                    // quarter past
                    CH_VIERTU;
                    CH_AB;
                    setHours(hours, false, language, matrix);
                    break;
                case 4:
                    // 20 past
                    CH_ZWAENZG;
                    CH_AB;
                    setHours(hours, false, language, matrix);
                    break;
                case 5:
                    // 5 to half
                    CH_FUEF;
                    CH_VOR;
                    CH_HAUBI;
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 6:
                    // half
                    CH_HAUBI;
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 7:
                    // 5 past half
                    CH_FUEF;
                    CH_AB;
                    CH_HAUBI;
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 8:
                    // 20 to
                    CH_ZWAENZG;
                    CH_VOR;
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 9:
                    // quarter to
                    CH_VIERTU;
                    CH_VOR;
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 10:
                    // 10 to
                    CH_ZAEAE;
                    CH_VOR;
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 11:
                    // 5 to
                    CH_FUEF;
                    CH_VOR;
                    setHours(hours + 1, false, language, matrix);
                    break;
            }
            break;
            //
            // English
            //
        case LANGUAGE_EN:
            EN_ITIS;

            switch (minutes / 5) {
                case 0:
                    // full hour
                    setHours(hours, true, language, matrix);
                    break;
                case 1:
                    // 5 past
                    EN_FIVE;
                    EN_PAST;
                    setHours(hours, false, language, matrix);
                    break;
                case 2:
                    // 10 past
                    EN_TEN;
                    EN_PAST;
                    setHours(hours, false, language, matrix);
                    break;
                case 3:
                    // quarter past
                    EN_A;
                    EN_QUATER;
                    EN_PAST;
                    setHours(hours, false, language, matrix);
                    break;
                case 4:
                    // 20 past
                    EN_TWENTY;
                    EN_PAST;
                    setHours(hours, false, language, matrix);
                    break;
                case 5:
                    // 5 to half
                    EN_TWENTY;
                    EN_FIVE;
                    EN_PAST;
                    setHours(hours, false, language, matrix);
                    break;
                case 6:
                    // half
                    EN_HALF;
                    EN_PAST;
                    setHours(hours, false, language, matrix);
                    break;
                case 7:
                    // 5 past half
                    EN_TWENTY;
                    EN_FIVE;
                    EN_TO;
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 8:
                    // 20 to
                    EN_TWENTY;
                    EN_TO;
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 9:
                    // quarter to
                    EN_A;
                    EN_QUATER;
                    EN_TO;
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 10:
                    // 10 to
                    EN_TEN;
                    EN_TO;
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 11:
                    // 5 to
                    EN_FIVE;
                    EN_TO;
                    setHours(hours + 1, false, language, matrix);
                    break;
            }
            break;
            //
            // French
            //
        case LANGUAGE_FR:
            FR_ILEST;

            switch (minutes / 5) {
                case 0:
                    // full hour
                    setHours(hours, true, language, matrix);
                    FR_hours(hours, matrix);
                    break;
                case 1:
                    // 5 past
                    setHours(hours, false, language, matrix);
                    FR_hours(hours, matrix);
                    FR_CINQ;
                    break;
                case 2:
                    // 10 past
                    setHours(hours, false, language, matrix);
                    FR_hours(hours, matrix);
                    FR_DIX;
                    break;
                case 3:
                    // quarter past
                    setHours(hours, false, language, matrix);
                    FR_hours(hours, matrix);
                    FR_ET;
                    FR_QUART;
                    break;
                case 4:
                    // 20 past
                    setHours(hours, false, language, matrix);
                    FR_hours(hours, matrix);
                    FR_VINGT;
                    break;
                case 5:
                    // 5 to half
                    setHours(hours, false, language, matrix);
                    FR_hours(hours, matrix);
                    FR_VINGT;
                    FR_TRAIT;
                    FR_CINQ;
                    break;
                case 6:
                    // half
                    setHours(hours, false, language, matrix);
                    FR_hours(hours, matrix);
                    FR_ET;
                    FR_DEMI;
                    break;
                case 7:
                    // 5 past half
                    setHours(hours + 1, false, language, matrix);
                    FR_hours(hours + 1, matrix);
                    FR_MOINS;
                    FR_VINGT;
                    FR_TRAIT;
                    FR_CINQ;
                    break;
                case 8:
                    // 20 to
                    setHours(hours + 1, false, language, matrix);
                    FR_hours(hours + 1, matrix);
                    FR_MOINS;
                    FR_VINGT;
                    break;
                case 9:
                    // quarter to
                    setHours(hours + 1, false, language, matrix);
                    FR_hours(hours + 1, matrix);
                    FR_MOINS;
                    FR_LE;
                    FR_QUART;
                    break;
                case 10:
                    // 10 to
                    setHours(hours + 1, false, language, matrix);
                    FR_hours(hours + 1, matrix);
                    FR_MOINS;
                    FR_DIX;
                    break;
                case 11:
                    // 5 to
                    setHours(hours + 1, false, language, matrix);
                    FR_hours(hours + 1, matrix);
                    FR_MOINS;
                    FR_CINQ;
                    break;
            }
            break;
            //
            // Italian
            //
        case LANGUAGE_IT:
            switch (minutes / 5) {
                case 0:
                    // full hour
                    setHours(hours, true, language, matrix);
                    IT_hours(hours, matrix);
                    break;
                case 1:
                    // 5 past
                    IT_E2;
                    IT_CINQUE;
                    setHours(hours, false, language, matrix);
                    IT_hours(hours, matrix);
                    break;
                case 2:
                    // 10 past
                    IT_E2;
                    IT_DIECI;
                    setHours(hours, false, language, matrix);
                    IT_hours(hours, matrix);
                    break;
                case 3:
                    // quarter past
                    IT_E2;
                    IT_UN;
                    IT_QUARTO;
                    setHours(hours, false, language, matrix);
                    IT_hours(hours, matrix);
                    break;
                case 4:
                    // 20 past
                    IT_E2;
                    IT_VENTI;
                    setHours(hours, false, language, matrix);
                    IT_hours(hours, matrix);
                    break;
                case 5:
                    // 5 to half
                    IT_E2;
                    IT_VENTI;
                    IT_CINQUE;
                    setHours(hours, false, language, matrix);
                    IT_hours(hours, matrix);
                    break;
                case 6:
                    // half
                    IT_E2;
                    IT_MEZZA;
                    setHours(hours, false, language, matrix);
                    IT_hours(hours, matrix);
                    break;
                case 7:
                    // 5 past half
                    IT_MENO;
                    IT_VENTI;
                    IT_CINQUE;
                    setHours(hours + 1, false, language, matrix);
                    IT_hours(hours + 1, matrix);
                    break;
                case 8:
                    // 20 to
                    IT_MENO;
                    IT_VENTI;
                    setHours(hours + 1, false, language, matrix);
                    IT_hours(hours + 1, matrix);
                    break;
                case 9:
                    // quarter to
                    IT_MENO;
                    IT_UN;
                    IT_QUARTO;
                    setHours(hours + 1, false, language, matrix);
                    IT_hours(hours + 1, matrix);
                    break;
                case 10:
                    // 10 to
                    IT_MENO;
                    IT_DIECI;
                    setHours(hours + 1, false, language, matrix);
                    IT_hours(hours + 1, matrix);
                    break;
                case 11:
                    // 5 to
                    IT_MENO;
                    IT_CINQUE;
                    setHours(hours + 1, false, language, matrix);
                    IT_hours(hours + 1, matrix);
                    break;
            }
            break;
            //
            // Dutch
            //
        case LANGUAGE_NL:
            NL_HETIS;

            switch (minutes / 5) {
                case 0:
                    // full hour
                    setHours(hours, true, language, matrix);
                    break;
                case 1:
                    // 5 past
                    NL_VIJF;
                    NL_OVER;
                    setHours(hours, false, language, matrix);
                    break;
                case 2:
                    // 10 past
                    NL_TIEN;
                    NL_OVER;
                    setHours(hours, false, language, matrix);
                    break;
                case 3:
                    // quarter past
                    NL_KWART;
                    NL_OVER2;
                    setHours(hours, false, language, matrix);
                    break;
                case 4:
                    // 10 to half
                    NL_TIEN;
                    NL_VOOR;
                    NL_HALF;
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 5:
                    // 5 to half
                    NL_VIJF;
                    NL_VOOR;
                    NL_HALF;
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 6:
                    // half
                    NL_HALF;
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 7:
                    // 5 past half
                    NL_VIJF;
                    NL_OVER;
                    NL_HALF;
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 8:
                    // 20 to
                    NL_TIEN;
                    NL_OVER;
                    NL_HALF;
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 9:
                    // quarter to
                    NL_KWART;
                    NL_VOOR2;
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 10:
                    // 10 to
                    NL_TIEN;
                    NL_VOOR;
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 11:
                    // 5 to
                    NL_VIJF;
                    NL_VOOR;
                    setHours(hours + 1, false, language, matrix);
                    break;
            }
            break;
            //
            // Spanish
            //
        case LANGUAGE_ES:
            switch (minutes / 5) {
                case 0:
                    // full hour
                    ES_hours(hours, matrix);
                    setHours(hours, false, language, matrix);
                    break;
                case 1:
                    // 5 past
                    ES_Y;
                    ES_CINCO;
                    ES_hours(hours, matrix);
                    setHours(hours, false, language, matrix);
                    break;
                case 2:
                    // 10 past
                    ES_Y;
                    ES_DIEZ;
                    ES_hours(hours, matrix);
                    setHours(hours, false, language, matrix);
                    break;
                case 3:
                    // quarter past
                    ES_Y;
                    ES_CUARTO;
                    ES_hours(hours, matrix);
                    setHours(hours, false, language, matrix);
                    break;
                case 4:
                    // 20 past
                    ES_Y;
                    ES_VEINTE;
                    ES_hours(hours, matrix);
                    setHours(hours, false, language, matrix);
                    break;
                case 5:
                    // 5 to half
                    ES_Y;
                    ES_VEINTICINCO;
                    ES_hours(hours, matrix);
                    setHours(hours, false, language, matrix);
                    break;
                case 6:
                    // half
                    ES_Y;
                    ES_MEDIA;
                    ES_hours(hours, matrix);
                    setHours(hours, false, language, matrix);
                    break;
                case 7:
                    // 5 past half
                    ES_MENOS;
                    ES_VEINTICINCO;
                    ES_hours(hours + 1, matrix);
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 8:
                    // 20 to
                    ES_MENOS;
                    ES_VEINTE;
                    ES_hours(hours + 1, matrix);
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 9:
                    // quarter to
                    ES_MENOS;
                    ES_CUARTO;
                    ES_hours(hours + 1, matrix);
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 10:
                    // 10 to
                    ES_MENOS;
                    ES_DIEZ;
                    ES_hours(hours + 1, matrix);
                    setHours(hours + 1, false, language, matrix);
                    break;
                case 11:
                    // 5 to
                    ES_MENOS;
                    ES_CINCO;
                    ES_hours(hours + 1, matrix);
                    setHours(hours + 1, false, language, matrix);
                    break;
            }
            break;
    }
}

/**
 * Sets the hours, depending on hours. 'glatt' (flat) means
 * it is exactly that hour, so we have to add 'UHR'
 * and use EIN instead of EINS when it is 1.
 * (In German, at least.)
 * Other linguistic special cases follow further down
 * in the code...
 */
void Renderer::setHours(char hours, boolean glatt, byte language, word matrix[16]) {
    // setMinutes above already handles this, but because of
    // special cases in the time zone handling and an hour
    // addition above, the hour can end up out of range
    // again here 
    // 
    // 
    while (hours < 0) {
        hours += 12;
    }
    while (hours > 24) {
        hours -= 12;
    }

    switch (language) {
            //
            // German (standard, Swabian, Bavarian)
            //
        case LANGUAGE_DE_DE:
        case LANGUAGE_DE_SW:
        case LANGUAGE_DE_BA:
        case LANGUAGE_DE_SA:
            if (glatt) {
                DE_UHR;
            }

            switch (hours) {
                case 0:
                case 12:
                case 24:
                    DE_H_ZWOELF;
                    DEBUG_PRINT("zwoelf ");
                    break;
                case 1:
                case 13:
                    if (glatt) {
                        DE_H_EIN;
						            DEBUG_PRINT("ein ");
                    } else {
                        DE_H_EINS;
						            DEBUG_PRINT("eins ");
                    }
                    break;
                case 2:
                case 14:
                    DE_H_ZWEI;
					          DEBUG_PRINT("zwei ");
                    break;
                case 3:
                case 15:
                    DE_H_DREI;
					          DEBUG_PRINT("drei ");
                    break;
                case 4:
                case 16:
                    DE_H_VIER;
					          DEBUG_PRINT("vier ");
                    break;
                case 5:
                case 17:
                    DE_H_FUENF;
					          DEBUG_PRINT("fuenf ");
                    break;
                case 6:
                case 18:
                    DE_H_SECHS;
					          DEBUG_PRINT("sechs ");
                    break;
                case 7:
                case 19:
                    DE_H_SIEBEN;
					          DEBUG_PRINT("sieben ");
                    break;
                case 8:
                case 20:
                    DE_H_ACHT;
					          DEBUG_PRINT("acht ");
                    break;
                case 9:
                case 21:
                    DE_H_NEUN;
					          DEBUG_PRINT("neun ");
                    break;
                case 10:
                case 22:
                    DE_H_ZEHN;
					          DEBUG_PRINT("zehn ");
                    break;
                case 11:
                case 23:
                    DE_H_ELF;
					          DEBUG_PRINT("elf ");
                    break;
            }
			
			if (glatt) {
				DE_UHR;
				DEBUG_PRINT("Uhr");
			}
			DEBUG_PRINTLN("");

            break;
            //
            // Switzerland: Bernese German
            //
        case LANGUAGE_CH:
            switch (hours) {
                case 0:
                case 12:
                case 24:
                    CH_H_ZWOEUFI;
                    break;
                case 1:
                case 13:
                    CH_H_EIS;
                    break;
                case 2:
                case 14:
                    CH_H_ZWOEI;
                    break;
                case 3:
                case 15:
                    CH_H_DRUE;
                    break;
                case 4:
                case 16:
                    CH_H_VIER;
                    break;
                case 5:
                case 17:
                    CH_H_FUEFI;
                    break;
                case 6:
                case 18:
                    CH_H_SAECHSI;
                    break;
                case 7:
                case 19:
                    CH_H_SIEBNI;
                    break;
                case 8:
                case 20:
                    CH_H_ACHTI;
                    break;
                case 9:
                case 21:
                    CH_H_NUENI;
                    break;
                case 10:
                case 22:
                    CH_H_ZAENI;
                    break;
                case 11:
                case 23:
                    CH_H_EUFI;
                    break;
            }
            break;
            //
            // English
            //
        case LANGUAGE_EN:
            if (glatt) {
                EN_OCLOCK;
            }

            switch (hours) {
                case 0:
                case 12:
                case 24:
                    EN_H_TWELVE;
                    break;
                case 1:
                case 13:
                    EN_H_ONE;
                    break;
                case 2:
                case 14:
                    EN_H_TWO;
                    break;
                case 3:
                case 15:
                    EN_H_THREE;
                    break;
                case 4:
                case 16:
                    EN_H_FOUR;
                    break;
                case 5:
                case 17:
                    EN_H_FIVE;
                    break;
                case 6:
                case 18:
                    EN_H_SIX;
                    break;
                case 7:
                case 19:
                    EN_H_SEVEN;
                    break;
                case 8:
                case 20:
                    EN_H_EIGHT;
                    break;
                case 9:
                case 21:
                    EN_H_NINE;
                    break;
                case 10:
                case 22:
                    EN_H_TEN;
                    break;
                case 11:
                case 23:
                    EN_H_ELEVEN;
                    break;
            }
            break;
            //
            // French
        case LANGUAGE_FR:
            switch (hours) {
                case 0:
                case 24:
                    FR_H_MINUIT;
                    break;
                case 12:
                    FR_H_MIDI;
                    break;
                case 1:
                case 13:
                    FR_H_UNE;
                    break;
                case 2:
                case 14:
                    FR_H_DEUX;
                    break;
                case 3:
                case 15:
                    FR_H_TROIS;
                    break;
                case 4:
                case 16:
                    FR_H_QUATRE;
                    break;
                case 5:
                case 17:
                    FR_H_CINQ;
                    break;
                case 6:
                case 18:
                    FR_H_SIX;
                    break;
                case 7:
                case 19:
                    FR_H_SEPT;
                    break;
                case 8:
                case 20:
                    FR_H_HUIT;
                    break;
                case 9:
                case 21:
                    FR_H_NEUF;
                    break;
                case 10:
                case 22:
                    FR_H_DIX;
                    break;
                case 11:
                case 23:
                    FR_H_ONZE;
                    break;
            }
            break;
            //
            // Italian
            //
        case LANGUAGE_IT:
            switch (hours) {
                case 0:
                case 12:
                case 24:
                    IT_H_DODICI;
                    break;
                case 1:
                case 13:
                    IT_H_LUNA;
                    break;
                case 2:
                case 14:
                    IT_H_DUE;
                    break;
                case 3:
                case 15:
                    IT_H_TRE;
                    break;
                case 4:
                case 16:
                    IT_H_QUATTRO;
                    break;
                case 5:
                case 17:
                    IT_H_CINQUE;
                    break;
                case 6:
                case 18:
                    IT_H_SEI;
                    break;
                case 7:
                case 19:
                    IT_H_SETTE;
                    break;
                case 8:
                case 20:
                    IT_H_OTTO;
                    break;
                case 9:
                case 21:
                    IT_H_NOVE;
                    break;
                case 10:
                case 22:
                    IT_H_DIECI;
                    break;
                case 11:
                case 23:
                    IT_H_UNDICI;
                    break;
            }
            break;
            //
            // Dutch
            //
        case LANGUAGE_NL:
            if (glatt) {
                NL_UUR;
            }

            switch (hours) {
                case 0:
                case 12:
                case 24:
                    NL_H_TWAALF;
                    break;
                case 1:
                case 13:
                    NL_H_EEN;
                    break;
                case 2:
                case 14:
                    NL_H_TWEE;
                    break;
                case 3:
                case 15:
                    NL_H_DRIE;
                    break;
                case 4:
                case 16:
                    NL_H_VIER;
                    break;
                case 5:
                case 17:
                    NL_H_VIJF;
                    break;
                case 6:
                case 18:
                    NL_H_ZES;
                    break;
                case 7:
                case 19:
                    NL_H_ZEVEN;
                    break;
                case 8:
                case 20:
                    NL_H_ACHT;
                    break;
                case 9:
                case 21:
                    NL_H_NEGEN;
                    break;
                case 10:
                case 22:
                    NL_H_TIEN;
                    break;
                case 11:
                case 23:
                    NL_H_ELF;
                    break;
            }
            break;
            //
            // Spanish
            //
        case LANGUAGE_ES:
            switch (hours) {
                case 0:
                case 12:
                case 24:
                    ES_H_DOCE;
                    break;
                case 1:
                case 13:
                    ES_H_UNA;
                    break;
                case 2:
                case 14:
                    ES_H_DOS;
                    break;
                case 3:
                case 15:
                    ES_H_TRES;
                    break;
                case 4:
                case 16:
                    ES_H_CUATRO;
                    break;
                case 5:
                case 17:
                    ES_H_CINCO;
                    break;
                case 6:
                case 18:
                    ES_H_SEIS;
                    break;
                case 7:
                case 19:
                    ES_H_SIETE;
                    break;
                case 8:
                case 20:
                    ES_H_OCHO;
                    break;
                case 9:
                case 21:
                    ES_H_NUEVE;
                    break;
                case 10:
                case 22:
                    ES_H_DIEZ;
                    break;
                case 11:
                case 23:
                    ES_H_ONCE;
                    break;
            }
            break;
    }
}

/**
 * Sets the four corner dots, depending on minutes % 5 (the remainder).
 *
 * @param cw TRUE -> clockwise.
 *             FALSE -> counter-clockwise.
 */
void Renderer::setCorners(byte minutes, boolean cw, word matrix[16]) {
    if (cw) {
        // clockwise
        switch (minutes % 5) {
            case 0:
                break;
            case 1:
                matrix[1] |= 0b0000000000011111; // 1
                break;
            case 2:
                matrix[1] |= 0b0000000000011111; // 1
                matrix[0] |= 0b0000000000011111; // 2
                break;
            case 3:
                matrix[1] |= 0b0000000000011111; // 1
                matrix[0] |= 0b0000000000011111; // 2
                matrix[3] |= 0b0000000000011111; // 3
                break;
            case 4:
                matrix[1] |= 0b0000000000011111; // 1
                matrix[0] |= 0b0000000000011111; // 2
                matrix[3] |= 0b0000000000011111; // 3
                matrix[2] |= 0b0000000000011111; // 4
                break;
        }
    } else {
        // counter-clockwise
        switch (minutes % 5) {
            case 0:
                break;
            case 1:
                matrix[0] |= 0b0000000000011111; // 1
                break;
            case 2:
                matrix[0] |= 0b0000000000011111; // 1
                matrix[1] |= 0b0000000000011111; // 2
                break;
            case 3:
                matrix[0] |= 0b0000000000011111; // 1
                matrix[1] |= 0b0000000000011111; // 2
                matrix[2] |= 0b0000000000011111; // 3
                break;
            case 4:
                matrix[0] |= 0b0000000000011111; // 1
                matrix[1] |= 0b0000000000011111; // 2
                matrix[2] |= 0b0000000000011111; // 3
                matrix[3] |= 0b0000000000011111; // 4
                break;
        }
    }
}

/**
 * In alarm setting mode certain words have to go, e.g. "ES IST" in German.
 */
void Renderer::cleanWordsForAlarmSettingMode(byte language, word matrix[16]) {
    switch (language) {
        case LANGUAGE_DE_DE:
        case LANGUAGE_DE_SW:
        case LANGUAGE_DE_BA:
        case LANGUAGE_DE_SA:
            matrix[0] &= 0b0010001111111111; // remove ES IST
            break;
        case LANGUAGE_CH:
            matrix[0] &= 0b0010000111111111; // remove ES ISCH
            break;
        case LANGUAGE_EN:
            matrix[0] &= 0b0010011111111111; // remove IT IS
            break;
        case LANGUAGE_FR:
            matrix[0] &= 0b0010001111111111; // remove IL EST
            break;
        case LANGUAGE_IT:
            matrix[0] &= 0b0000100111111111; // remove SONO LE
            matrix[1] &= 0b0111111111111111; // remove E (L'UNA)
            break;
        case LANGUAGE_NL:
            matrix[0] &= 0b0001001111111111; // remove HET IS
            break;
        case LANGUAGE_ES:
            matrix[0] &= 0b1000100011111111; // remove SON LAS
            matrix[0] &= 0b0011100111111111; // remove ES LA
            break;
    }
}

/**
 * Linguistic special case for French.
 */
void Renderer::FR_hours(byte hours, word matrix[16]) {
    if ((hours == 1) || (hours == 13)) {
        FR_HEURE;
    } else if ((hours == 0) || (hours == 12) || (hours == 24)) {
        // MIDI / MINUIT without HEURES
    } else {
        FR_HEURES;
    }
}

/**
 * Linguistic special case for Italian.
 */
void Renderer::IT_hours(byte hours, word matrix[16]) {
    if ((hours != 1) && (hours != 13)) {
        IT_SONOLE;
    } else {
        IT_E;
    }
}

/**
 * Linguistic special case for Spanish.
 */
void Renderer::ES_hours(byte hours, word matrix[16]) {
    if ((hours == 1) || (hours == 13)) {
        ES_ESLA;
    } else {
        ES_SONLAS;
    }
}
