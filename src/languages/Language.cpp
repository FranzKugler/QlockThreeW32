/**
 * Language
 * See Language.h for the shape of a language and why it has this one.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  20.8.2026
 * @updated  20.8.2026
 */
#include <Arduino.h>

#include "Language.h"
#include "../Renderer.h"   // LANGUAGE_COUNT, until the numbers move here too
#include "../LogBuffer.h"

namespace
{
    /**
     * The apostrophe of O'CLOCK, in the three spellings it turns up in:
     * ASCII ' , U+2019 RIGHT SINGLE QUOTATION MARK, U+2032 PRIME. See
     * PANEL_MARKS in Language.h. Returns its length in bytes, or 0.
     *
     * Reading past the end is not possible: && stops at the first byte that
     * does not match, and a terminating NUL matches nothing here.
     */
    uint8_t markLength(const char *at)
    {
        if (at[0] == '\'') return 1;
        if ((uint8_t)at[0] == 0xE2 && (uint8_t)at[1] == 0x80 &&
            ((uint8_t)at[2] == 0x99 || (uint8_t)at[2] == 0xB2))
        {
            return 3;
        }
        return 0;
    }

    /**
     * The start of the cell after the one starting at `at`.
     *
     * One lead byte, its continuation bytes, and an apostrophe if one follows
     * - that is the whole cell rule, stated here once so that counting and
     * extracting cannot disagree about it.
     */
    const char *nextCell(const char *at)
    {
        if (*at == '\0') return at;

        at++;                                       // past the lead byte
        while ((*at & 0xC0) == 0x80) at++;          // and its continuations
        at += markLength(at);

        return at;
    }
}

uint8_t Languages::cells(const char *utf8)
{
    uint8_t count = 0;
    for (const char *at = utf8; *at; at = nextCell(at)) count++;
    return count;
}

void Languages::appendCell(String &out, const char *utf8, uint8_t index)
{
    const char *at = utf8;
    for (uint8_t i = 0; i < index && *at; i++) at = nextCell(at);
    if (*at == '\0') return;

    // Byte by byte to the start of the next cell, so the continuation bytes
    // travel with their lead byte - otherwise the string stops being UTF-8 and
    // the browser shows a replacement character.
    for (const char *end = nextCell(at); at < end; at++) out += *at;
}

void Face::light(uint8_t index)
{
    if (index >= wordCount) return;

    const Word &entry = words[index];
    uint8_t length = Languages::cells(entry.text);
    if (entry.col + length > PANEL_COLS) return;

    // Column 0 is the most significant bit of the row, which is what the old
    // masks were written as: 0b1101110000000000 is columns 0, 1, 3, 4, 5. The
    // five bits below the eleven columns are the corner LEDs and stay clear.
    matrix[entry.row] |= (word)(((1u << length) - 1u) << (16 - entry.col - length));
}

int Languages::selfCheck()
{
    int problems = 0;

    for (byte number = 0; number < LANGUAGE_COUNT; number++)
    {
        const Language *language = find(number);
        if (language == nullptr) continue;   // not moved across yet

        for (uint8_t row = 0; row < PANEL_ROWS; row++)
        {
            uint8_t width = cells(language->rows[row]);
            if (width != PANEL_COLS)
            {
                debugE("Panel %s row %d has %d cells, not %d",
                       language->code, row, width, PANEL_COLS);
                problems++;
            }

            // An apostrophe is a suffix and has nothing to attach to here, so
            // the row would silently be one cell short of what it looks.
            if (markLength(language->rows[row]) != 0)
            {
                debugE("Panel %s row %d starts with an apostrophe",
                       language->code, row);
                problems++;
            }
        }

        for (uint8_t i = 0; i < language->wordCount; i++)
        {
            const Word &entry = language->words[i];
            uint8_t length = cells(entry.text);

            if (entry.row >= PANEL_ROWS || entry.col + length > PANEL_COLS)
            {
                debugE("Word %s \"%s\" runs off the panel at %d,%d",
                       language->code, entry.text, entry.row, entry.col);
                problems++;
                continue;
            }

            String under;
            for (uint8_t c = 0; c < length; c++)
            {
                appendCell(under, language->rows[entry.row], entry.col + c);
            }

            if (under != entry.text)
            {
                debugE("Word %s at %d,%d says \"%s\" but the panel reads \"%s\"",
                       language->code, entry.row, entry.col, entry.text, under.c_str());
                problems++;
            }
        }
    }

    if (problems == 0) debugA("Panels and words agree");
    return problems;
}
