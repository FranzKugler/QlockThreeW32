/**
 * Language_DE
 * German, and the three dialects that share its panel.
 *
 * One panel, four entries. Standard German, Swabian, Bavarian and Saxon differ
 * only in how they name a few of the twelve five-minute steps, and the panel
 * carries the letters for all of them - which is why DREIVIERTEL sits across
 * the whole of row 2 and VIERTEL inside it.
 *
 * Where they differ:
 *
 *              quarter past   20 past          20 to             quarter to
 *   standard   viertel nach   zwanzig nach     zwanzig vor       viertel vor
 *   Swabian    viertel <h+1>  zwanzig nach     zwanzig vor       dreiviertel
 *   Bavarian   viertel nach   zwanzig nach     zwanzig vor       dreiviertel
 *   Saxon      viertel <h+1>  zehn vor halb    zehn nach halb    dreiviertel
 *
 * @mc       ESP32S3
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
     * 0 ESKISTAFÜNF
     * 1 ZEHNZWANZIG
     * 2 DREIVIERTEL
     * 3 VORFUNKNACH
     * 4 HALBAELFÜNF
     * 5 EINSXAMZWEI
     * 6 DREIAUJVIER
     * 7 SECHSNLACHT
     * 8 SIEBENZWÖLF
     * 9 ZEHNEUNKUHR
     */
    enum
    {
        ES, IST, VOR, NACH, UHR,
        FUENF, ZEHN, VIERTEL, ZWANZIG, HALB, DREIVIERTEL,
        H_EIN, H_EINS, H_ZWEI, H_DREI, H_VIER, H_FUENF, H_SECHS,
        H_SIEBEN, H_ACHT, H_NEUN, H_ZEHN, H_ELF, H_ZWOELF
    };

    const Word WORDS[] = {
        { 0, 0, "ES" },
        { 0, 3, "IST" },
        { 3, 0, "VOR" },
        { 3, 7, "NACH" },
        { 9, 8, "UHR" },

        { 0, 7, "FÜNF" },
        { 1, 0, "ZEHN" },
        { 2, 4, "VIERTEL" },
        { 1, 4, "ZWANZIG" },
        { 4, 0, "HALB" },
        { 2, 0, "DREIVIERTEL" },

        { 5, 0, "EIN" },
        { 5, 0, "EINS" },
        { 5, 7, "ZWEI" },
        { 6, 0, "DREI" },
        { 6, 7, "VIER" },
        { 4, 7, "FÜNF" },
        { 7, 0, "SECHS" },
        { 8, 0, "SIEBEN" },
        { 7, 7, "ACHT" },
        { 9, 3, "NEUN" },
        { 9, 0, "ZEHN" },
        { 4, 5, "ELF" },
        { 8, 6, "ZWÖLF" }
    };

    /** Which of the four this is, for the handful of places it matters. */
    enum Dialect { STANDARD, SWABIAN, BAVARIAN, SAXON };

    /**
     * The hour word. `full` is the top of the hour, where German says "ein
     * Uhr" rather than "eins" - the only place the two forms differ.
     */
    void hour(int8_t hours, bool full, Face &face)
    {
        // setMinutes() clamps once, but the rules above add an hour and can
        // push it back out; "halb zwölf" at 23:30 asks for hour 24.
        while (hours < 0) hours += 12;
        while (hours > 24) hours -= 12;

        if (full) face.light(UHR);

        switch (hours % 12)
        {
            case 0:  face.light(H_ZWOELF); break;
            case 1:  face.light(full ? H_EIN : H_EINS); break;
            case 2:  face.light(H_ZWEI); break;
            case 3:  face.light(H_DREI); break;
            case 4:  face.light(H_VIER); break;
            case 5:  face.light(H_FUENF); break;
            case 6:  face.light(H_SECHS); break;
            case 7:  face.light(H_SIEBEN); break;
            case 8:  face.light(H_ACHT); break;
            case 9:  face.light(H_NEUN); break;
            case 10: face.light(H_ZEHN); break;
            case 11: face.light(H_ELF); break;
        }
    }

    void render(int8_t hours, uint8_t minutes, Face &face, Dialect dialect)
    {
        face.light(ES);
        face.light(IST);

        switch (minutes / 5)
        {
            case 0:
                hour(hours, true, face);
                break;
            case 1:
                face.light(FUENF); face.light(NACH);
                hour(hours, false, face);
                break;
            case 2:
                face.light(ZEHN); face.light(NACH);
                hour(hours, false, face);
                break;
            case 3:
                face.light(VIERTEL);
                if (dialect == SWABIAN || dialect == SAXON)
                {
                    hour(hours + 1, false, face);
                }
                else
                {
                    face.light(NACH);
                    hour(hours, false, face);
                }
                break;
            case 4:
                if (dialect == SAXON)
                {
                    face.light(ZEHN); face.light(VOR); face.light(HALB);
                    hour(hours + 1, false, face);
                }
                else
                {
                    face.light(ZWANZIG); face.light(NACH);
                    hour(hours, false, face);
                }
                break;
            case 5:
                face.light(FUENF); face.light(VOR); face.light(HALB);
                hour(hours + 1, false, face);
                break;
            case 6:
                face.light(HALB);
                hour(hours + 1, false, face);
                break;
            case 7:
                face.light(FUENF); face.light(NACH); face.light(HALB);
                hour(hours + 1, false, face);
                break;
            case 8:
                if (dialect == SAXON)
                {
                    face.light(ZEHN); face.light(NACH); face.light(HALB);
                }
                else
                {
                    face.light(ZWANZIG); face.light(VOR);
                }
                hour(hours + 1, false, face);
                break;
            case 9:
                face.light(dialect == STANDARD ? VIERTEL : DREIVIERTEL);
                if (dialect == STANDARD) face.light(VOR);
                hour(hours + 1, false, face);
                break;
            case 10:
                face.light(ZEHN); face.light(VOR);
                hour(hours + 1, false, face);
                break;
            case 11:
                face.light(FUENF); face.light(VOR);
                hour(hours + 1, false, face);
                break;
        }
    }

    void renderStandard(int8_t h, uint8_t m, Face &f) { render(h, m, f, STANDARD); }
    void renderSwabian(int8_t h, uint8_t m, Face &f)  { render(h, m, f, SWABIAN); }
    void renderBavarian(int8_t h, uint8_t m, Face &f) { render(h, m, f, BAVARIAN); }
    void renderSaxon(int8_t h, uint8_t m, Face &f)    { render(h, m, f, SAXON); }

    const char *const ROWS[PANEL_ROWS] = {
        "ESKISTAFÜNF",
        "ZEHNZWANZIG",
        "DREIVIERTEL",
        "VORFUNKNACH",
        "HALBAELFÜNF",
        "EINSXAMZWEI",
        "DREIAUJVIER",
        "SECHSNLACHT",
        "SIEBENZWÖLF",
        "ZEHNEUNKUHR"
    };

    const uint8_t WORD_COUNT = sizeof(WORDS) / sizeof(WORDS[0]);
}

// Four entries over one panel. The rows are repeated rather than shared
// through a pointer so that each Language is one self-contained record - the
// four lines below are the whole difference between the dialects.
//
// `extern` on the definitions, not only on the declarations in Languages.cpp:
// a const object at namespace scope has internal linkage in C++ unless it is
// spelled out, and the table would not find them.
extern const Language LANGUAGE_GERMAN = {
    "de-DE", "Deutsch", "de",
    { ROWS[0], ROWS[1], ROWS[2], ROWS[3], ROWS[4], ROWS[5], ROWS[6], ROWS[7], ROWS[8], ROWS[9] },
    WORDS, WORD_COUNT, renderStandard
};

extern const Language LANGUAGE_SWABIAN = {
    "de-SW", "Schwäbisch", "de",
    { ROWS[0], ROWS[1], ROWS[2], ROWS[3], ROWS[4], ROWS[5], ROWS[6], ROWS[7], ROWS[8], ROWS[9] },
    WORDS, WORD_COUNT, renderSwabian
};

extern const Language LANGUAGE_BAVARIAN = {
    "de-BA", "Bayrisch", "de",
    { ROWS[0], ROWS[1], ROWS[2], ROWS[3], ROWS[4], ROWS[5], ROWS[6], ROWS[7], ROWS[8], ROWS[9] },
    WORDS, WORD_COUNT, renderBavarian
};

extern const Language LANGUAGE_SAXON = {
    "de-SA", "Sächsisch", "de",
    { ROWS[0], ROWS[1], ROWS[2], ROWS[3], ROWS[4], ROWS[5], ROWS[6], ROWS[7], ROWS[8], ROWS[9] },
    WORDS, WORD_COUNT, renderSaxon
};
