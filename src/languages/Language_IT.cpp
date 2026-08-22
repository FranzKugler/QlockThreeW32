/**
 * Language_IT
 * Italian.
 *
 * "è l'una" for one o'clock but "sono le due" for every other hour - the
 * special case the old renderer kept in IT_hours(), now part of hour().
 *
 * The panel carries ORE in the top right and nothing lights it.
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
     * 0 SONORLEBORE
     * 1 ÈRL′UNASDUEZ   (the L′ is one cell)
     * 2 TREOTTONOVE
     * 3 DIECIUNDICI
     * 4 DODICISETTE
     * 5 QUATTROCSEI
     * 6 CINQUESMENO
     * 7 ECUNOQUARTO
     * 8 VENTICINQUE
     * 9 DIECIEMEZZA
     */
    enum
    {
        SONO, LE, E_SONO, E_MIN, MENO,
        UN, QUARTO, VENTI, CINQUE, DIECI, MEZZA,
        H_LUNA, H_DUE, H_TRE, H_QUATTRO, H_CINQUE, H_SEI,
        H_SETTE, H_OTTO, H_NOVE, H_DIECI, H_UNDICI, H_DODICI
    };

    const Word WORDS[] = {
        { 0, 0, "SONO" },
        { 0, 5, "LE" },
        { 1, 0, "È" },      // the "è" of "è l'una", row 1
        { 7, 0, "E" },      // the "e" of "e cinque", row 7
        { 6, 7, "MENO" },

        { 7, 2, "UN" },
        { 7, 5, "QUARTO" },
        { 8, 0, "VENTI" },
        { 8, 5, "CINQUE" },
        { 9, 0, "DIECI" },
        { 9, 6, "MEZZA" },

        { 1, 2, "L′UNA" },  // the L and its apostrophe share one cell
        { 1, 7, "DUE" },
        { 2, 0, "TRE" },
        { 5, 0, "QUATTRO" },
        { 6, 0, "CINQUE" },
        { 5, 8, "SEI" },
        { 4, 6, "SETTE" },
        { 2, 3, "OTTO" },
        { 2, 7, "NOVE" },
        { 3, 0, "DIECI" },
        { 3, 5, "UNDICI" },
        { 4, 0, "DODICI" }
    };

    /** The hour, and the "sono le" / "è" that agrees with it. */
    void hour(int8_t hours, Face &face)
    {
        while (hours < 0) hours += 12;
        while (hours > 24) hours -= 12;

        if (hours == 1 || hours == 13)
        {
            face.light(E_SONO);
        }
        else
        {
            face.light(SONO);
            face.light(LE);
        }

        switch (hours % 12)
        {
            case 0:  face.light(H_DODICI); break;
            case 1:  face.light(H_LUNA); break;
            case 2:  face.light(H_DUE); break;
            case 3:  face.light(H_TRE); break;
            case 4:  face.light(H_QUATTRO); break;
            case 5:  face.light(H_CINQUE); break;
            case 6:  face.light(H_SEI); break;
            case 7:  face.light(H_SETTE); break;
            case 8:  face.light(H_OTTO); break;
            case 9:  face.light(H_NOVE); break;
            case 10: face.light(H_DIECI); break;
            case 11: face.light(H_UNDICI); break;
        }
    }

    void render(int8_t hours, uint8_t minutes, Face &face)
    {
        switch (minutes / 5)
        {
            case 0:
                hour(hours, face);
                break;
            case 1:
                face.light(E_MIN); face.light(CINQUE);
                hour(hours, face);
                break;
            case 2:
                face.light(E_MIN); face.light(DIECI);
                hour(hours, face);
                break;
            case 3:
                face.light(E_MIN); face.light(UN); face.light(QUARTO);
                hour(hours, face);
                break;
            case 4:
                face.light(E_MIN); face.light(VENTI);
                hour(hours, face);
                break;
            case 5:
                face.light(E_MIN); face.light(VENTI); face.light(CINQUE);
                hour(hours, face);
                break;
            case 6:
                face.light(E_MIN); face.light(MEZZA);
                hour(hours, face);
                break;
            case 7:
                face.light(MENO); face.light(VENTI); face.light(CINQUE);
                hour(hours + 1, face);
                break;
            case 8:
                face.light(MENO); face.light(VENTI);
                hour(hours + 1, face);
                break;
            case 9:
                face.light(MENO); face.light(UN); face.light(QUARTO);
                hour(hours + 1, face);
                break;
            case 10:
                face.light(MENO); face.light(DIECI);
                hour(hours + 1, face);
                break;
            case 11:
                face.light(MENO); face.light(CINQUE);
                hour(hours + 1, face);
                break;
        }
    }
}

extern const Language LANGUAGE_ITALIAN = {
    "it", "Italiano", "it",
    {
        "SONORLEBORE",
        "ÈRL′UNASDUEZ",
        "TREOTTONOVE",
        "DIECIUNDICI",
        "DODICISETTE",
        "QUATTROCSEI",
        "CINQUESMENO",
        "ECUNOQUARTO",
        "VENTICINQUE",
        "DIECIEMEZZA"
    },
    WORDS, sizeof(WORDS) / sizeof(WORDS[0]), render
};
