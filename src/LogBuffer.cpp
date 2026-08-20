/**
 * LogBuffer
 * See LogBuffer.h for what this holds and why it exists at all.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  20.8.2026
 * @updated  20.8.2026
 */
#include <Arduino.h>
#include <esp_log.h>

#include "LogBuffer.h"

namespace
{
    struct Slot
    {
        uint32_t ms;
        uint8_t level;
        char text[LOG_LINE_MAX];
    };

    // In .bss on purpose. The first line is written from the top of setup(),
    // before LittleFS is mounted and long before there is a heap worth
    // allocating out of - a ring that has to be set up first would start one
    // step too late for the lines this exists to keep.
    Slot slots[LOG_LINES];

    // Counts up forever and never wraps in any life this clock will have:
    // at ten lines a second it takes thirteen years to reach 2^32.
    volatile uint32_t seqNext = 0;

    // Held for the length of a memcpy and nothing else. Writers are on both
    // cores - loop() and the web server on core 1, the light sampler and the
    // OTA download on core 0 - so this cannot be a plain critical section.
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

    // What ESP-IDF printed before we took its vprintf over. Still called, so
    // the serial output is exactly what it was.
    vprintf_like_t chainedVprintf = nullptr;

    /** ESP-IDF's level letter, mapped onto RemoteDebug's numbering. */
    uint8_t levelFromLetter(char letter)
    {
        switch (letter)
        {
            case 'E': return RemoteDebug::ERROR;
            case 'W': return RemoteDebug::WARNING;
            case 'I': return RemoteDebug::INFO;
            case 'D': return RemoteDebug::DEBUG;
            case 'V': return RemoteDebug::VERBOSE;
            default:  return RemoteDebug::DEBUG;
        }
    }

    /**
     * Takes one ESP-IDF log line apart: strips the colour escapes it wraps
     * every line in, and reads the level off the letter that starts it.
     *
     * The format is `I (1234) wifi: ...`, optionally inside `\033[0;32m...
     * \033[0m`. Both halves are worth removing - the escapes would arrive in
     * the browser as visible rubbish, and the level is better as a field.
     */
    void storeIdfLine(char *text)
    {
        // Cleaned in place rather than into a second array. This runs on
        // whatever task happened to log - the WiFi driver's, or the 2 KB
        // arduino_events one - so every byte of stack it does not take is
        // worth having.
        size_t out = 0;
        for (char *in = text; *in; in++)
        {
            if (*in == '\033')
            {
                while (*in && *in != 'm') in++;   // skip to the end of the escape
                if (!*in) break;
                continue;
            }
            if (*in == '\r' || *in == '\n') continue;
            text[out++] = *in;
        }
        text[out] = '\0';
        if (out == 0) return;

        uint8_t level = RemoteDebug::DEBUG;
        size_t from = 0;
        if (out > 2 && text[1] == ' ' && text[2] == '(')
        {
            level = levelFromLetter(text[0]);
            from = 2;   // drop the letter and its space, keep the (1234) tag: part
        }

        Log::line(level, text + from, out - from);
    }

    /**
     * Stands in for ESP-IDF's vprintf. Formats once into a stack buffer, hands
     * it on unchanged, and keeps a copy.
     *
     * IDF calls this per format string, not per line, and a single call can
     * carry several lines or none - so the copy is split on newlines rather
     * than stored as it arrives.
     */
    int captureVprintf(const char *format, va_list args)
    {
        // One line's worth plus a little. Anything longer is cut here rather
        // than on some task's stack - Log::line() would trim it in any case.
        char buffer[LOG_LINE_MAX + 32];

        // va_list is consumed by the first use, so the copy that goes to the
        // real vprintf has to be taken before ours runs.
        va_list forward;
        va_copy(forward, args);
        int written = vsnprintf(buffer, sizeof(buffer), format, args);
        int result = chainedVprintf ? chainedVprintf(format, forward) : written;
        va_end(forward);

        if (written > 0)
        {
            char *start = buffer;
            for (char *at = buffer; *at; at++)
            {
                if (*at != '\n') continue;
                *at = '\0';
                storeIdfLine(start);
                start = at + 1;
            }
            // A trailing fragment with no newline yet. Stored as its own line
            // rather than held back: IDF finishes its lines in one call, and a
            // half line kept for a continuation that never comes is a line lost.
            if (*start) storeIdfLine(start);
        }

        return result;
    }
}

void Log::begin()
{
    chainedVprintf = esp_log_set_vprintf(captureVprintf);
}

void Log::line(uint8_t level, const char *text, size_t length)
{
    if (length == 0) return;
    if (length > LOG_LINE_MAX - 1) length = LOG_LINE_MAX - 1;

    portENTER_CRITICAL_SAFE(&mux);

    Slot &slot = slots[seqNext % LOG_LINES];
    slot.ms = millis();
    slot.level = level;
    // Terminator first, then the body. A reader on the other core that catches
    // this slot mid-overwrite then sees a truncated line rather than one
    // running off the end of the array - the ring cannot wrap during a single
    // response, but "cannot" is a worse guarantee than "does not crash if".
    slot.text[length] = '\0';
    memcpy(slot.text, text, length);
    seqNext++;

    portEXIT_CRITICAL_SAFE(&mux);
}

uint32_t Log::nextSeq()
{
    return seqNext;
}

uint32_t Log::oldestSeq()
{
    uint32_t next = seqNext;
    return next > LOG_LINES ? next - LOG_LINES : 0;
}

size_t Log::collect(uint32_t since, Line *out, size_t max)
{
    uint32_t next = seqNext;
    uint32_t from = since;
    uint32_t oldest = Log::oldestSeq();

    // Asking for something that has already scrolled out gives what is left,
    // and the caller compares `oldest` against what it asked for to notice.
    if (from < oldest) from = oldest;

    // Asking for something that has not happened yet means the clock restarted
    // under the browser: the numbering began again at zero while the tab kept
    // counting from where the previous run left off. Without this the request
    // would match nothing for as long as the tab stayed open. Answering from
    // the beginning also lets the caller see the number go backwards, which is
    // how it knows to throw its window away rather than append a second boot
    // to the first.
    if (from > next) from = oldest;

    size_t count = 0;
    for (uint32_t seq = from; seq < next && count < max; seq++, count++)
    {
        Slot &slot = slots[seq % LOG_LINES];
        out[count].seq = seq;
        out[count].ms = slot.ms;
        out[count].level = slot.level;
        out[count].text = slot.text;
    }
    return count;
}

size_t DebugLog::write(uint8_t character)
{
    // The line is assembled here and stored whole. RemoteDebug hands its
    // output over one character at a time and inserts a \r of its own before
    // every \n for the benefit of telnet clients on Windows; neither belongs
    // in a JSON string.
    if (character == '\n')
    {
        if (assembled > 0)
        {
            Log::line(lastLevel, assembling, assembled);
            assembled = 0;
        }
    }
    else if (character != '\r' && assembled < LOG_LINE_MAX - 1)
    {
        assembling[assembled++] = (char)character;
    }

    return RemoteDebug::write(character);
}
