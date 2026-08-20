/**
 * Languages
 * The table that turns a stored language number into a language.
 *
 * A new language is one file next to this one and one line in the table below.
 * It could be made to need no line at all, through statically constructed
 * objects that register themselves - but the order in which those run across
 * translation units is not defined in C++, and when that goes wrong it goes
 * wrong before there is any way to see it. A line in a table is a line and
 * never lies.
 *
 * A nullptr would mean that number has not been moved across from the old
 * switch in Renderer.cpp yet, and the renderer would fall back for it. There
 * are none left; the fallback and the Woerter_*.h macros go once the check in
 * RenderCheck.cpp has been run one last time.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  20.8.2026
 * @updated  20.8.2026
 */
#include "Language.h"
#include "../Renderer.h"
#include <string.h>

extern const Language LANGUAGE_GERMAN;
extern const Language LANGUAGE_SWABIAN;
extern const Language LANGUAGE_BAVARIAN;
extern const Language LANGUAGE_SAXON;
extern const Language LANGUAGE_SWISS;
extern const Language LANGUAGE_ENGLISH;
extern const Language LANGUAGE_FRENCH;
extern const Language LANGUAGE_ITALIAN;
extern const Language LANGUAGE_DUTCH;
extern const Language LANGUAGE_SPANISH;

namespace
{
    // Indexed by the LANGUAGE_* numbers in Renderer.h, which are stored in NVS
    // and must therefore keep their values.
    const Language *const TABLE[LANGUAGE_COUNT] = {
        &LANGUAGE_GERMAN,     // LANGUAGE_DE_DE 0
        &LANGUAGE_SWABIAN,    // LANGUAGE_DE_SW 1
        &LANGUAGE_BAVARIAN,   // LANGUAGE_DE_BA 2
        &LANGUAGE_SAXON,      // LANGUAGE_DE_SA 3
        &LANGUAGE_SWISS,      // LANGUAGE_CH    4
        &LANGUAGE_ENGLISH,    // LANGUAGE_EN    5
        &LANGUAGE_FRENCH,     // LANGUAGE_FR    6
        &LANGUAGE_ITALIAN,    // LANGUAGE_IT    7
        &LANGUAGE_DUTCH,      // LANGUAGE_NL    8
        &LANGUAGE_SPANISH     // LANGUAGE_ES    9
    };
}

bool Languages::samePanel(const Language *a, const Language *b)
{
    if (a == nullptr || b == nullptr) return false;
    if (a == b) return true;

    // The German dialects point at one shared array, so this usually settles
    // on the first comparison; ten short strings is cheap either way.
    for (uint8_t row = 0; row < PANEL_ROWS; row++)
    {
        if (strcmp(a->rows[row], b->rows[row]) != 0) return false;
    }
    return true;
}

const Language *Languages::find(byte language)
{
    if (language >= LANGUAGE_COUNT) return nullptr;
    return TABLE[language];
}
