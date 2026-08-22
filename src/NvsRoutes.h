/**
 * NvsRoutes
 * NVS, shown as if it were a filesystem: namespaces as folders, keys as files.
 *
 * It is not one, and the pretence is deliberate rather than sloppy. NVS is a
 * flat key-value store - a namespace, a key, a typed value - with no tree, no
 * paths and no file names. But the clock's own records are JSON strings, one
 * per key, and once that is true a namespace reads exactly like a folder of
 * `.json` files. Giving the two stores the same shape means one explorer, one
 * set of gestures, and one place to look when something has gone missing.
 *
 * Where the pretence stops is written down here so it is not discovered by
 * surprise:
 *
 *  - **The tree is two levels deep and cannot be deeper.** There are no
 *    sub-namespaces. A folder inside a folder is not refused, it is impossible.
 *  - **There is nothing to upload and no folder to create.** A namespace comes
 *    into existence when a key is written into it and vanishes with the last
 *    one, so both would be writing a key by another name. The tab drops the
 *    two buttons rather than showing them greyed out.
 *  - **The extension is a reading, not a fact.** A string that starts with `{`
 *    or `[` is offered as `.json`, another string as `.txt`, everything else -
 *    integers, blobs - as `.bin`. Nothing in NVS says so; the suffix is this
 *    module's opinion about a value it has looked at.
 *  - **Sizes are in entries, not bytes.** NVS accounts for itself in 32-byte
 *    entries and reports them through nvs_get_stats(), so that is what the
 *    fullness bar shows. Reporting invented bytes would be worse.
 *
 * One value is deliberately not readable: the expert password hash. Everything
 * else here is as open as the unlock that reached it, and stops being readable
 * the moment the clock is locked again - but a hash carried away in those
 * thirty seconds is crackable offline forever, and probably against a password
 * used elsewhere too. The key is still listed, because a tree that hides
 * entries is a tree that lies; only the read is refused.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.2
 * @created  22.8.2026
 * @updated  22.8.2026
 */
#ifndef NVSROUTES_H
#define NVSROUTES_H

#include <Arduino.h>

// Most entries one listing answers with. NVS holds a few dozen on this clock;
// this is a guard against a partition somebody filled, not a real limit.
#define NVS_LIST_MAX 128

// Largest value the browser is offered for editing, and the largest it may
// write back. Strings in NVS are capped at 4000 bytes by the store itself, so
// this only has to stay under what the heap will hold twice over.
#define NVS_EDIT_MAX 4096

// A string longer than this is listed as `.bin` without being read, so that
// building the tree never pulls a large blob into the heap just to look at
// its first character.
#define NVS_PEEK_MAX 2048

namespace Nvs
{
    /** Hangs the /nvs handlers on the server. Call from setup(). */
    void begin();
}

#endif
