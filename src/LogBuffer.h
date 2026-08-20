/**
 * LogBuffer
 * The clock's own log, kept in memory so the web UI can show it.
 *
 * The point of this is what happens *before* anyone is watching. RemoteDebug
 * is a telnet server and the serial port needs a cable, so both only ever show
 * what is said while someone is listening - and the interesting lines are the
 * ones from the first two seconds after a restart, which nobody is ever in
 * time for. A ring in RAM holds them until the debug tab is opened, which may
 * be hours later.
 *
 * Two streams feed it, because there are two of them and they do not meet
 * anywhere lower down:
 *
 *   - `Debug` (this project's debugA/I/W/E) is a `Print`, so `DebugLog`
 *     overrides its `write()` and takes a copy on the way past.
 *   - ESP-IDF's own logging - the WiFi driver above all - does not go through
 *     `Print` at all. `esp_log_set_vprintf()` is where that one is caught.
 *
 * What it cannot hold is anything from before `Log::begin()`: the ROM
 * bootloader, the second stage and the partition table have all had their say
 * before this code runs. The log starts at the first line of setup().
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  20.8.2026
 * @updated  20.8.2026
 */
#ifndef LOGBUFFER_H
#define LOGBUFFER_H

#include <Arduino.h>
#include <RemoteDebug.h>

// How much is kept, and what it costs. These two multiply into a plain array
// in .bss - 200 x 128 = 25 KB, next to the 34 KB the rest of the firmware has
// static - so this is the one number to turn down if RAM ever gets tight.
//
// A fixed slot per line rather than a byte ring: it wastes the tail of every
// short line, and in exchange a sequence number indexes straight into the
// array. The web handler needs "everything after 412" on every poll, and that
// becomes arithmetic instead of a walk.
#define LOG_LINES 200
#define LOG_LINE_MAX 128

// How many lines one response carries at most. The whole ring as JSON is
// around 25 KB, which is a lot of String to hold on a heap that an update
// wants for itself; a batch is a tenth of that. The UI asks again straight
// away while `more` is set, so a freshly opened tab still fills in one go.
#define LOG_BATCH 100

namespace Log
{
    /** One stored line, as the web handler hands it out. */
    struct Line
    {
        uint32_t seq;
        uint32_t ms;
        uint8_t level;      // RemoteDebug's numbering: 0 PROFILER .. 5 ERROR, 6 ANY
        const char *text;
    };

    /**
     * Starts capturing. Call as the first thing in setup(), before anything
     * has a chance to log - the ring itself is in .bss and needs no setting up,
     * but the ESP-IDF hook does.
     */
    void begin();

    /**
     * Stores one line. Called from the `Debug` tee and from the ESP-IDF hook,
     * and safe to call from either core.
     */
    void line(uint8_t level, const char *text, size_t length);

    /** Sequence number the next line will get; also the count since boot. */
    uint32_t nextSeq();

    /** Sequence number of the oldest line still held. */
    uint32_t oldestSeq();

    /**
     * Copies out at most `max` lines with a sequence number of `since` or
     * above, oldest first. Returns how many were written to `out`.
     *
     * The text pointers are into the ring and stay valid only until the next
     * line arrives, so the caller has to be done with them before it lets go
     * of the CPU. Copying instead would mean a second 25 KB somewhere.
     */
    size_t collect(uint32_t since, Line *out, size_t max);
}

/**
 * RemoteDebug with a copy taken on the way past.
 *
 * `write()` is virtual and everything the debugX macros produce goes through
 * it one character at a time, so assembling those back into lines here catches
 * the lot. What does *not* come through is RemoteDebug's own prefix - the
 * `(I t:1234ms)` part is built inside its `write()` and never handed on - which
 * suits us: the level and the timestamp are kept as fields rather than as text,
 * and the browser can colour an error red without parsing anything.
 *
 * `isActive()` is not virtual, and is overridden all the same. The macros call
 * it on this type, immediately before printf, and it is the only place the
 * level is named - so this is where it can be picked up. Anything calling it
 * through a `RemoteDebug&` would miss the override and leave the level stale;
 * only the library itself does that, and only for its own bookkeeping.
 */
class DebugLog : public RemoteDebug
{
public:
    boolean isActive(uint8_t debugLevel = RemoteDebug::DEBUG)
    {
        lastLevel = debugLevel;
        return RemoteDebug::isActive(debugLevel);
    }

    size_t write(uint8_t character) override;

private:
    uint8_t lastLevel = RemoteDebug::DEBUG;
    char assembling[LOG_LINE_MAX];
    size_t assembled = 0;
};

// The name is forced: the library's macros expand to `Debug.printf(...)`.
// Declared here rather than as `extern RemoteDebug Debug` in each translation
// unit, which is what it used to be - with the definition now being a subclass,
// those declarations would each describe a different type for the same object.
extern DebugLog Debug;

#endif
