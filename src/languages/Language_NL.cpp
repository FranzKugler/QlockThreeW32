/**
 * Language_NL
 * Dutch.
 *
 * The panel has OVER and VOOR twice, once above the hour block and once below,
 * so that "kwart over" and "kwart voor" read on the same line as KWART while
 * "vijf over half" reads on its own. Hence OVER/OVER2 and VOOR/VOOR2.
 *
 * @mc       ESP32S3
 * @author   Rudolf Klimesch (panel, after Christian Aschoff)
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
     * 0 HETKISAVIJF
     * 1 TIENBTZVOOR
     * 2 OVERMEKWART
     * 3 HALFSPWOVER
     * 4 VOORTHGEENS
     * 5 TWEEPVCDRIE
     * 6 VIERVIJFZES
     * 7 ZEVENONEGEN
     * 8 ACHTTIENELF
     * 9 TWAALFBFUUR
     */
    enum
    {
        HET, IS, UUR,
        VIJF, TIEN, KWART, HALF,
        OVER, VOOR, OVER2, VOOR2,
        H_EEN, H_EENS, H_TWEE, H_DRIE, H_VIER, H_VIJF, H_ZES,
        H_ZEVEN, H_ACHT, H_NEGEN, H_TIEN, H_ELF, H_TWAALF
    };

    const Word WORDS[] = {
        { 0, 0, "HET" },
        { 0, 4, "IS" },
        { 9, 8, "UUR" },

        { 0, 7, "VIJF" },
        { 1, 0, "TIEN" },
        { 2, 6, "KWART" },
        { 3, 0, "HALF" },

        { 2, 0, "OVER" },    // above the hour block, for "vijf over"
        { 1, 7, "VOOR" },
        { 3, 7, "OVER" },    // beside KWART, for "kwart over"
        { 4, 0, "VOOR" },

        { 4, 7, "EEN" },
        { 4, 7, "EENS" },
        { 5, 0, "TWEE" },
        { 5, 7, "DRIE" },
        { 6, 0, "VIER" },
        { 6, 4, "VIJF" },
        { 6, 8, "ZES" },
        { 7, 0, "ZEVEN" },
        { 8, 0, "ACHT" },
        { 7, 6, "NEGEN" },
        { 8, 4, "TIEN" },
        { 8, 8, "ELF" },
        { 9, 0, "TWAALF" }
    };

    void hour(int8_t hours, bool full, Face &face)
    {
        while (hours < 0) hours += 12;
        while (hours > 24) hours -= 12;

        if (full) face.light(UUR);

        switch (hours % 12)
        {
            case 0:  face.light(H_TWAALF); break;
            // EEN rather than EENS at every hour: unlike German, the old code
            // never used the longer form.
            case 1:  face.light(H_EEN); break;
            case 2:  face.light(H_TWEE); break;
            case 3:  face.light(H_DRIE); break;
            case 4:  face.light(H_VIER); break;
            case 5:  face.light(H_VIJF); break;
            case 6:  face.light(H_ZES); break;
            case 7:  face.light(H_ZEVEN); break;
            case 8:  face.light(H_ACHT); break;
            case 9:  face.light(H_NEGEN); break;
            case 10: face.light(H_TIEN); break;
            case 11: face.light(H_ELF); break;
        }
    }

    void render(int8_t hours, uint8_t minutes, Face &face)
    {
        face.light(HET);
        face.light(IS);

        switch (minutes / 5)
        {
            case 0:
                hour(hours, true, face);
                break;
            case 1:
                face.light(VIJF); face.light(OVER);
                hour(hours, false, face);
                break;
            case 2:
                face.light(TIEN); face.light(OVER);
                hour(hours, false, face);
                break;
            case 3:
                face.light(KWART); face.light(OVER2);
                hour(hours, false, face);
                break;
            case 4:
                face.light(TIEN); face.light(VOOR); face.light(HALF);
                hour(hours + 1, false, face);
                break;
            case 5:
                face.light(VIJF); face.light(VOOR); face.light(HALF);
                hour(hours + 1, false, face);
                break;
            case 6:
                face.light(HALF);
                hour(hours + 1, false, face);
                break;
            case 7:
                face.light(VIJF); face.light(OVER); face.light(HALF);
                hour(hours + 1, false, face);
                break;
            case 8:
                face.light(TIEN); face.light(OVER); face.light(HALF);
                hour(hours + 1, false, face);
                break;
            case 9:
                face.light(KWART); face.light(VOOR2);
                hour(hours + 1, false, face);
                break;
            case 10:
                face.light(TIEN); face.light(VOOR);
                hour(hours + 1, false, face);
                break;
            case 11:
                face.light(VIJF); face.light(VOOR);
                hour(hours + 1, false, face);
                break;
        }
    }
}

extern const Language LANGUAGE_DUTCH = {
    "nl", "Nederlands", "nl",
    {
        "HETKISAVIJF",
        "TIENBTZVOOR",
        "OVERMEKWART",
        "HALFSPWOVER",
        "VOORTHGEENS",
        "TWEEPVCDRIE",
        "VIERVIJFZES",
        "ZEVENONEGEN",
        "ACHTTIENELF",
        "TWAALFBFUUR"
    },
    WORDS, sizeof(WORDS) / sizeof(WORDS[0]), render
};
