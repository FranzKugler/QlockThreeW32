/**
 * Language_CH
 * Bernese German.
 *
 * The panel carries UHR in its bottom right corner and nothing ever lights it:
 * Bernese German says "es isch zwöufi", not "zwölf Uhr". The letters are there
 * because the panel was drawn from the German one.
 *
 * @mc       ESP32S3
 * @author   Thomas Schuler / thomas.schuler _AT_ vtg _DOT_ admin _DOT_ ch (panel)
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
     * 0 ESKISCHAFÜF
     * 1 VIERTUBFZÄÄ
     * 2 ZWÄNZGSIVOR
     * 3 ABOHAUBIEGE
     * 4 EISZWÖISDRÜ
     * 5 VIERIFÜFIQT
     * 6 SÄCHSISIBNI
     * 7 ACHTINÜNIEL
     * 8 ZÄNIERBEUFI
     * 9 ZWÖUFINAUHR
     */
    enum
    {
        ES, ISCH, VOR, AB,
        FUEF, ZAEAE, VIERTU, ZWAENZG, HAUBI,
        H_EIS, H_ZWOEI, H_DRUE, H_VIERI, H_FUEFI, H_SAECHSI,
        H_SIBNI, H_ACHTI, H_NUENI, H_ZAENI, H_EUFI, H_ZWOEUFI
    };

    const Word WORDS[] = {
        { 0, 0, "ES" },
        { 0, 3, "ISCH" },
        { 2, 8, "VOR" },
        { 3, 0, "AB" },

        { 0, 8, "FÜF" },
        { 1, 8, "ZÄÄ" },
        { 1, 0, "VIERTU" },
        { 2, 0, "ZWÄNZG" },
        { 3, 3, "HAUBI" },

        { 4, 0, "EIS" },
        { 4, 3, "ZWÖI" },
        { 4, 8, "DRÜ" },
        { 5, 0, "VIERI" },
        { 5, 5, "FÜFI" },
        { 6, 0, "SÄCHSI" },
        { 6, 6, "SIBNI" },
        { 7, 0, "ACHTI" },
        { 7, 5, "NÜNI" },
        { 8, 0, "ZÄNI" },
        { 8, 7, "EUFI" },
        { 9, 0, "ZWÖUFI" }
    };

    /** No "Uhr" at the top of the hour, so `full` is not used here. */
    void hour(int8_t hours, Face &face)
    {
        while (hours < 0) hours += 12;
        while (hours > 24) hours -= 12;

        switch (hours % 12)
        {
            case 0:  face.light(H_ZWOEUFI); break;
            case 1:  face.light(H_EIS); break;
            case 2:  face.light(H_ZWOEI); break;
            case 3:  face.light(H_DRUE); break;
            case 4:  face.light(H_VIERI); break;
            case 5:  face.light(H_FUEFI); break;
            case 6:  face.light(H_SAECHSI); break;
            case 7:  face.light(H_SIBNI); break;
            case 8:  face.light(H_ACHTI); break;
            case 9:  face.light(H_NUENI); break;
            case 10: face.light(H_ZAENI); break;
            case 11: face.light(H_EUFI); break;
        }
    }

    void render(int8_t hours, uint8_t minutes, Face &face)
    {
        face.light(ES);
        face.light(ISCH);

        switch (minutes / 5)
        {
            case 0:  hour(hours, face); break;
            case 1:  face.light(FUEF); face.light(AB); hour(hours, face); break;
            case 2:  face.light(ZAEAE); face.light(AB); hour(hours, face); break;
            case 3:  face.light(VIERTU); face.light(AB); hour(hours, face); break;
            case 4:  face.light(ZWAENZG); face.light(AB); hour(hours, face); break;
            case 5:
                face.light(FUEF); face.light(VOR); face.light(HAUBI);
                hour(hours + 1, face);
                break;
            case 6:
                face.light(HAUBI);
                hour(hours + 1, face);
                break;
            case 7:
                face.light(FUEF); face.light(AB); face.light(HAUBI);
                hour(hours + 1, face);
                break;
            case 8:  face.light(ZWAENZG); face.light(VOR); hour(hours + 1, face); break;
            case 9:  face.light(VIERTU); face.light(VOR); hour(hours + 1, face); break;
            case 10: face.light(ZAEAE); face.light(VOR); hour(hours + 1, face); break;
            case 11: face.light(FUEF); face.light(VOR); hour(hours + 1, face); break;
        }
    }
}

extern const Language LANGUAGE_SWISS = {
    "de-CH", "Schwiizerdütsch", "de",
    {
        "ESKISCHAFÜF",
        "VIERTUBFZÄÄ",
        "ZWÄNZGSIVOR",
        "ABOHAUBIEGE",
        "EISZWÖISDRÜ",
        "VIERIFÜFIQT",
        "SÄCHSISIBNI",
        "ACHTINÜNIEL",
        "ZÄNIERBEUFI",
        "ZWÖUFINAUHR"
    },
    WORDS, sizeof(WORDS) / sizeof(WORDS[0]), render
};
