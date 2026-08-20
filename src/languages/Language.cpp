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

uint8_t Languages::characters(const char *utf8)
{
    uint8_t count = 0;
    for (const char *at = utf8; *at; at++)
    {
        // Continuation bytes are 10xxxxxx and belong to the character before.
        if ((*at & 0xC0) != 0x80) count++;
    }
    return count;
}

void Languages::appendCharacter(String &out, const char *utf8, uint8_t index)
{
    uint8_t seen = 0;
    for (const char *at = utf8; *at; at++)
    {
        if ((*at & 0xC0) == 0x80) continue;   // still inside the previous one

        if (seen == index)
        {
            out += *at;
            // Take the continuation bytes with it, or the string stops being
            // UTF-8 and the browser shows a replacement character.
            for (const char *tail = at + 1; (*tail & 0xC0) == 0x80 && *tail; tail++)
            {
                out += *tail;
            }
            return;
        }
        seen++;
    }
}

void Face::light(uint8_t index)
{
    if (index >= wordCount) return;

    const Word &entry = words[index];
    uint8_t length = Languages::characters(entry.text);
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
            uint8_t width = characters(language->rows[row]);
            if (width != PANEL_COLS)
            {
                debugE("Panel %s row %d has %d characters, not %d",
                       language->code, row, width, PANEL_COLS);
                problems++;
            }
        }

        for (uint8_t i = 0; i < language->wordCount; i++)
        {
            const Word &entry = language->words[i];
            uint8_t length = characters(entry.text);

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
                appendCharacter(under, language->rows[entry.row], entry.col + c);
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
