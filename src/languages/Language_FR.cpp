/**
 * Language_FR
 * French.
 *
 * The hour word and the word for "hour" have to agree, which is the special
 * case the old renderer kept in FR_hours(): "une heure" but "deux heures", and
 * midi and minuit take neither. It lives in hour() below now.
 *
 * @mc       ESP32S3
 * @author   Christian Aschoff / caschoff _AT_ mac _DOT_ com (panel)
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  20.8.2026
 * @updated  20.8.2026
 */
#include "Language.h"

namespace
{
    /*
     *   01234567890
     * 0 ILNESTODEUX
     * 1 QUATRETROIS
     * 2 NEUFUNESEPT
     * 3 HUITSIXCINQ
     * 4 MIDIXMINUIT
     * 5 ONZERHEURES
     * 6 MOINSOLEDIX
     * 7 ETRQUARTPMD
     * 8 VINGT-CINQU
     * 9 ETSDEMIEPAM
     */
    enum
    {
        IL, EST, ET, LE, MOINS, TRAIT, HEURE, HEURES,
        CINQ, DIX, QUART, VINGT, DEMI,
        H_UNE, H_DEUX, H_TROIS, H_QUATRE, H_CINQ, H_SIX,
        H_SEPT, H_HUIT, H_NEUF, H_DIX, H_ONZE, H_MIDI, H_MINUIT
    };

    const Word WORDS[] = {
        { 0, 0, "IL" },
        { 0, 3, "EST" },
        { 7, 0, "ET" },
        { 6, 6, "LE" },
        { 6, 0, "MOINS" },
        { 8, 5, "-" },
        { 5, 5, "HEURE" },
        { 5, 5, "HEURES" },

        { 8, 6, "CINQ" },
        { 6, 8, "DIX" },
        { 7, 3, "QUART" },
        { 8, 0, "VINGT" },
        { 9, 3, "DEMI" },

        { 2, 4, "UNE" },
        { 0, 7, "DEUX" },
        { 1, 6, "TROIS" },
        { 1, 0, "QUATRE" },
        { 3, 7, "CINQ" },
        { 3, 4, "SIX" },
        { 2, 7, "SEPT" },
        { 3, 0, "HUIT" },
        { 2, 0, "NEUF" },
        { 4, 2, "DIX" },
        { 5, 0, "ONZE" },
        { 4, 0, "MIDI" },
        { 4, 5, "MINUIT" }
    };

    /**
     * The hour, and the word that agrees with it. Both are decided from the
     * value as it arrives - the old code passed the same unclamped number to
     * setHours() and FR_hours() and relied on them agreeing, which they did
     * only because everything here stays inside 0..24.
     */
    void hour(int8_t hours, Face &face)
    {
        while (hours < 0) hours += 12;
        while (hours > 24) hours -= 12;

        if (hours == 0 || hours == 24)
        {
            face.light(H_MINUIT);
            return;                     // minuit takes no "heures"
        }
        if (hours == 12)
        {
            face.light(H_MIDI);
            return;
        }

        face.light(hours % 12 == 1 ? HEURE : HEURES);

        switch (hours % 12)
        {
            case 1:  face.light(H_UNE); break;
            case 2:  face.light(H_DEUX); break;
            case 3:  face.light(H_TROIS); break;
            case 4:  face.light(H_QUATRE); break;
            case 5:  face.light(H_CINQ); break;
            case 6:  face.light(H_SIX); break;
            case 7:  face.light(H_SEPT); break;
            case 8:  face.light(H_HUIT); break;
            case 9:  face.light(H_NEUF); break;
            case 10: face.light(H_DIX); break;
            case 11: face.light(H_ONZE); break;
        }
    }

    void render(int8_t hours, uint8_t minutes, Face &face)
    {
        face.light(IL);
        face.light(EST);

        switch (minutes / 5)
        {
            case 0:
                hour(hours, face);
                break;
            case 1:
                hour(hours, face); face.light(CINQ);
                break;
            case 2:
                hour(hours, face); face.light(DIX);
                break;
            case 3:
                hour(hours, face); face.light(ET); face.light(QUART);
                break;
            case 4:
                hour(hours, face); face.light(VINGT);
                break;
            case 5:
                hour(hours, face);
                face.light(VINGT); face.light(TRAIT); face.light(CINQ);
                break;
            case 6:
                hour(hours, face); face.light(ET); face.light(DEMI);
                break;
            case 7:
                hour(hours + 1, face);
                face.light(MOINS); face.light(VINGT); face.light(TRAIT); face.light(CINQ);
                break;
            case 8:
                hour(hours + 1, face); face.light(MOINS); face.light(VINGT);
                break;
            case 9:
                hour(hours + 1, face);
                face.light(MOINS); face.light(LE); face.light(QUART);
                break;
            case 10:
                hour(hours + 1, face); face.light(MOINS); face.light(DIX);
                break;
            case 11:
                hour(hours + 1, face); face.light(MOINS); face.light(CINQ);
                break;
        }
    }
}

extern const Language LANGUAGE_FRENCH = {
    "fr", "Français", "fr",
    {
        "ILNESTODEUX",
        "QUATRETROIS",
        "NEUFUNESEPT",
        "HUITSIXCINQ",
        "MIDIXMINUIT",
        "ONZERHEURES",
        "MOINSOLEDIX",
        "ETRQUARTPMD",
        "VINGT-CINQU",
        "ETSDEMIEPAM"
    },
    WORDS, sizeof(WORDS) / sizeof(WORDS[0]), render
};
