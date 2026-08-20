/**
 * Language_EN
 * English.
 *
 * Note the word at row 2, column 6. The mask has always lit those four cells
 * for "five past", and the letters the panel puts under them are F, I, F, E.
 * Either the panel drawing is wrong or the mask is one column out - it cannot
 * be told from here, and this file states what the panel says rather than what
 * it ought to say. An English clock will read "FIFE PAST"; see the note in
 * CLAUDE.md.
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
     * 0 ITLISASTIME
     * 1 ACQUARTERDC
     * 2 TWENTYFIFEX
     * 3 HALFBTENFTO
     * 4 PASTERUNINE
     * 5 ONESIXTHREE
     * 6 FOURFIVETWO
     * 7 EIGHTELEVEN
     * 8 SEVENTWELVE
     * 9 TENSEOCLOCK
     */
    enum
    {
        IT_, IS, A, OCLOCK,
        QUARTER, TWENTY, FIVE, HALF, TEN, TO, PAST,
        H_ONE, H_TWO, H_THREE, H_FOUR, H_FIVE, H_SIX,
        H_SEVEN, H_EIGHT, H_NINE, H_TEN, H_ELEVEN, H_TWELVE
    };

    const Word WORDS[] = {
        { 0, 0, "IT" },
        { 0, 3, "IS" },
        { 1, 0, "A" },
        { 9, 5, "OCLOCK" },

        { 1, 2, "QUARTER" },
        { 2, 0, "TWENTY" },
        { 2, 6, "FIFE" },      // the panel really does spell it this way
        { 3, 0, "HALF" },
        { 3, 5, "TEN" },
        { 3, 9, "TO" },
        { 4, 0, "PAST" },

        { 5, 0, "ONE" },
        { 6, 8, "TWO" },
        { 5, 6, "THREE" },
        { 6, 0, "FOUR" },
        { 6, 4, "FIVE" },
        { 5, 3, "SIX" },
        { 8, 0, "SEVEN" },
        { 7, 0, "EIGHT" },
        { 4, 7, "NINE" },
        { 9, 0, "TEN" },
        { 7, 5, "ELEVEN" },
        { 8, 5, "TWELVE" }
    };

    void hour(int8_t hours, bool full, Face &face)
    {
        while (hours < 0) hours += 12;
        while (hours > 24) hours -= 12;

        if (full) face.light(OCLOCK);

        switch (hours % 12)
        {
            case 0:  face.light(H_TWELVE); break;
            case 1:  face.light(H_ONE); break;
            case 2:  face.light(H_TWO); break;
            case 3:  face.light(H_THREE); break;
            case 4:  face.light(H_FOUR); break;
            case 5:  face.light(H_FIVE); break;
            case 6:  face.light(H_SIX); break;
            case 7:  face.light(H_SEVEN); break;
            case 8:  face.light(H_EIGHT); break;
            case 9:  face.light(H_NINE); break;
            case 10: face.light(H_TEN); break;
            case 11: face.light(H_ELEVEN); break;
        }
    }

    void render(int8_t hours, uint8_t minutes, Face &face)
    {
        face.light(IT_);
        face.light(IS);

        switch (minutes / 5)
        {
            case 0:
                hour(hours, true, face);
                break;
            case 1:
                face.light(FIVE); face.light(PAST);
                hour(hours, false, face);
                break;
            case 2:
                face.light(TEN); face.light(PAST);
                hour(hours, false, face);
                break;
            case 3:
                face.light(A); face.light(QUARTER); face.light(PAST);
                hour(hours, false, face);
                break;
            case 4:
                face.light(TWENTY); face.light(PAST);
                hour(hours, false, face);
                break;
            case 5:
                face.light(TWENTY); face.light(FIVE); face.light(PAST);
                hour(hours, false, face);
                break;
            case 6:
                face.light(HALF); face.light(PAST);
                hour(hours, false, face);
                break;
            case 7:
                face.light(TWENTY); face.light(FIVE); face.light(TO);
                hour(hours + 1, false, face);
                break;
            case 8:
                face.light(TWENTY); face.light(TO);
                hour(hours + 1, false, face);
                break;
            case 9:
                face.light(A); face.light(QUARTER); face.light(TO);
                hour(hours + 1, false, face);
                break;
            case 10:
                face.light(TEN); face.light(TO);
                hour(hours + 1, false, face);
                break;
            case 11:
                face.light(FIVE); face.light(TO);
                hour(hours + 1, false, face);
                break;
        }
    }
}

extern const Language LANGUAGE_ENGLISH = {
    "en", "English", "en",
    {
        "ITLISASTIME",
        "ACQUARTERDC",
        "TWENTYFIFEX",
        "HALFBTENFTO",
        "PASTERUNINE",
        "ONESIXTHREE",
        "FOURFIVETWO",
        "EIGHTELEVEN",
        "SEVENTWELVE",
        "TENSEOCLOCK"
    },
    WORDS, sizeof(WORDS) / sizeof(WORDS[0]), render
};
