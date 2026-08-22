/**
 * Language
 * What a word clock language is, as data plus one function.
 *
 * The old arrangement had two halves that could not see each other. `Renderer`
 * held a 1300 line switch over language and minute, and `Woerter_<LANG>.h`
 * held macros like `DE_FUENF matrix[0] |= 0b0000000111100000` - which work
 * only because a variable called `matrix` happens to be in scope, and which
 * know neither that they spell "FÜNF" nor where that sits. The letters of the
 * panel existed once as a comment in the language file and once as a German
 * array in `main .cpp` that the log walked whatever the language was.
 *
 * Here a word is **a place and the text it spells**:
 *
 *     { 0, 7, "FÜNF" }      // row 0, from column 7
 *
 * and everything else follows from that. The bit mask is arithmetic, so the
 * renderer needs no macros. The text is present, so the log can name what is
 * lit in the language that is lit. The geometry is present, so the browser can
 * draw the real panel and a script can hand OpenSCAD something to cut. And
 * because both the panel letters and the word's own text are here,
 * `Languages::selfCheck()` can confirm that the letters under a word really
 * spell it - which is the error one makes when adding a panel.
 *
 * What stays imperative is the grammar, and deliberately. Swabian says
 * "viertel sechs" and counts the hour up where standard German says "viertel
 * nach fünf"; French has "moins le quart"; Italian and Spanish inflect the
 * hour. Written as a rule table that becomes a small language of its own,
 * harder to read than the switch it replaces and touched once a year. So each
 * language keeps a `render()` of its own - it just lives next to its panel now
 * instead of inside a switch with nine others.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  20.8.2026
 * @updated  20.8.2026
 */
#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <Arduino.h>

// Fixed, and the reason the rendering logic stays manageable. Every panel this
// clock knows is eleven cells by ten rows.
#define PANEL_ROWS 10
#define PANEL_COLS 11

/*
 * A cell is usually one character. It is not always.
 *
 * English wants O'CLOCK and Italian L'UNA, and on a real panel the letter and
 * its apostrophe share one milled opening - one LED behind them, lit or dark
 * together. Before this, both were faked with a lookalike single character (Ò
 * for O', Ľ for L'), which read wrongly on the wall, spelled wrongly in the
 * log, and made selfCheck complain at every boot because the word underneath
 * said O and the panel said Ò.
 *
 * So: **U+2032 PRIME attaches to the character before it, and the two are one
 * cell.** That is the entire rule. It needs no separator in the row string, so
 * a panel still reads as a panel in the source:
 *
 *     "TENSEO′CLOCK"      // twelve characters, eleven cells
 *
 * PRIME rather than an ASCII apostrophe because ' is a quote in half the files
 * that touch this and a letter in none of them, and rather than U+2019 because
 * that is typographic punctuation which a font may kern into its neighbour.
 * A prime at the start of a row has nothing to attach to; selfCheck says so.
 */
#define PANEL_PRIME "′"

/**
 * One contiguous run of lit letters.
 *
 * `text` is both the label and the length: the run is as many columns as the
 * text has cells. Words with a gap in them are two entries - the German
 * "ES IST" is ES at column 0 and IST at column 3, with a K between that stays
 * dark - which also makes the log read "ES IST" rather than "ESIST".
 */
struct Word
{
    uint8_t row;
    uint8_t col;
    const char *text;   // UTF-8
};

/**
 * What a `render()` lights. Handed in by the renderer, which owns the frame
 * buffer; the language only ever names words.
 */
class Face
{
public:
    Face(const Word *words, uint8_t wordCount, word *matrix)
        : words(words), wordCount(wordCount), matrix(matrix) {}

    /** Lights one word of this language's table. */
    void light(uint8_t index);

private:
    const Word *words;
    uint8_t wordCount;
    word *matrix;
};

/**
 * One language: its panel, its words, and how it says the time.
 *
 * `name` is in the language itself - "Deutsch", "Français" - which is how
 * language pickers are conventionally written and which saves translating ten
 * names into six locales. `uiLocale` tells the web UI which of its own locale
 * files to speak, so a language added here needs no edit on that side.
 */
struct Language
{
    const char *code;                    // "de-DE", for the API
    const char *name;                    // "Deutsch", in its own language
    const char *uiLocale;                // "de", one of the web UI's locales
    const char *rows[PANEL_ROWS];        // UTF-8, PANEL_COLS characters each
    const Word *words;
    uint8_t wordCount;

    /**
     * Lights the words for this time. `hours` arrives already clamped to
     * 0..24 by the renderer, and may be incremented inside - "halb sechs" is
     * rendered from the hour after - so it is signed and generous.
     */
    void (*render)(int8_t hours, uint8_t minutes, Face &face);
};

namespace Languages
{
    /**
     * The language for one of the LANGUAGE_* numbers, or nullptr while that
     * one has not been moved across yet - the renderer falls back to its old
     * switch for those. Once the table is complete the fallback goes.
     */
    const Language *find(byte language);

    /**
     * Checks every panel against every word: eleven characters per row, and
     * the letters under each word spelling what the word claims. Logs what it
     * finds and returns the number of problems.
     *
     * Cheap enough to run at every boot, and it catches the one mistake that
     * is otherwise found by staring at lit LEDs: a word off by a column.
     */
    int selfCheck();

    /**
     * Whether two languages are cut into the same sheet of letters.
     *
     * A clock has one panel, milled once, and no firmware setting changes it.
     * German, Swabian, Bavarian and Saxon share theirs and can be swapped on a
     * built clock; Italian and Dutch cannot, and offering the swap only gets
     * somebody a wall of letters that no longer spells anything.
     *
     * Compared by the letters rather than by a group number stored next to
     * them: a number would be a second statement of the same fact, and the two
     * would eventually disagree. nullptr is not the same panel as anything,
     * itself included - "unknown" must not read as "fits".
     */
    bool samePanel(const Language *a, const Language *b);

    /**
     * Cells - not bytes, and not quite characters - in a UTF-8 string.
     * "FÜNF" is 4 and so is "O′CLO" minus a letter; see PANEL_PRIME above.
     */
    uint8_t cells(const char *utf8);

    /**
     * Appends cell `index` of a UTF-8 string to `out`. Panels carry Ü and Ö,
     * so the letter at a column is not the byte at that offset, and they carry
     * O′, so it is not the character at that offset either.
     */
    void appendCell(String &out, const char *utf8, uint8_t index);
}

#endif
