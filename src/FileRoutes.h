/**
 * FileRoutes
 * The clock's filesystem, opened up to the debug tab: a tree, a download, an
 * upload and a small editor.
 *
 * **This is LittleFS, not NVS.** The two are easy to confuse because both
 * survive a reboot and both are asked about in the same breath, but NVS is a
 * key-value store - `qlock`/`settings`, `qlocklight`/`curve`, one JSON string
 * apiece - with no tree, no paths and nothing that can be downloaded as a
 * file. What has files is the 3.5 MB LittleFS partition, and that is what this
 * serves: the web UI itself, zones.json, the icons, version.json.
 *
 * Which is also the warning. The partition this hands out write access to is
 * the one the page doing the asking is served from. Deleting index.html leaves
 * a clock that still answers every REST endpoint and shows nothing in a
 * browser, and the way back is `pio run -t uploadfs` over USB. That is not a
 * reason to forbid it - somebody who opens a file explorer wants to change
 * files - but it is a reason for the confirmation in the browser and for this
 * paragraph.
 *
 * Everything here is behind expert mode. That needs no argument beyond the
 * one already made for `/log`: this is strictly the more powerful of the two,
 * so anything that locks the log must lock this.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.2
 * @created  22.8.2026
 * @updated  22.8.2026
 */
#ifndef FILEROUTES_H
#define FILEROUTES_H

#include <Arduino.h>

// Longest path LittleFS is configured for, name included. A longer one is
// refused here rather than silently truncated by the driver.
#define FS_PATH_MAX 63

// Most entries one directory listing answers with. The listing is per
// directory and the browser expands lazily, so this is a guard against a
// directory somebody filled up, not a limit on the tree.
#define FS_LIST_MAX 96

// Largest file the in-browser editor will load or save. The editor route
// buffers - it takes JSON, not a stream - so this is a heap limit and has to
// stay well under what the clock has free. Anything bigger is download and
// upload only, which stream.
#define FS_EDIT_MAX 24576

namespace Files
{
    /**
     * Hangs the /fs handlers on the server. Call from setup() next to
     * Web::begin(); the order between them does not matter, the URIs are
     * distinct.
     */
    void begin();
}

#endif
