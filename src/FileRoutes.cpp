/**
 * FileRoutes
 * The /fs endpoints: list, read, upload, save, delete, mkdir.
 *
 * See FileRoutes.h for what this is and, more to the point, what it is not -
 * NVS has no files, and the partition being opened here is the one the page
 * asking is served from.
 *
 * Two shapes of write, deliberately, because they have different problems:
 *
 *  - **Upload streams.** POST /fs/upload takes multipart and goes to flash
 *    chunk by chunk through the second handler on the route, the same pattern
 *    the OTA upload uses, so a 200 kB bundle never sits in the heap. The cost
 *    is that the data handler cannot answer - it refuses by recording an error
 *    the done handler then sends, exactly as handleOtaUploadData() does.
 *  - **The editor buffers.** POST /fs/save takes JSON, so the whole content
 *    is in memory twice before it is written. That is fine for a config file
 *    and not fine for a bundle, hence FS_EDIT_MAX and a browser that only
 *    offers the editor below it.
 *
 * Both write to a `.part` file and rename on success. Uploading a new
 * index.html that dies half way would otherwise destroy the working one, and
 * the working one is how you reach the page to try again.
 *
 * The target path travels in the query string even for the upload, which
 * needs a word: WebServer parses the URL arguments *before* it starts reading
 * a multipart body, so server.arg() answers correctly inside the upload
 * handler, and the form fields are merged in afterwards. Checked against
 * Parsing.cpp in the core rather than assumed.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.2
 * @created  22.8.2026
 * @updated  22.8.2026
 */
#include <Arduino.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#include "FileRoutes.h"
#include "Expert.h"
#include "LogBuffer.h"

extern WebServer server;

// ------ paths ------

/**
 * Reduces what arrived to a path this clock will act on, or refuses it.
 *
 * The endpoints are reachable without the UI, so this is the only thing
 * standing between a typo and a write somewhere unintended. It is not a
 * sandbox - the whole volume is fair game by design - it is a check that what
 * was asked for is a path at all: absolute, no climbing with dot-dot, no
 * backslashes, no control characters, and short enough for LittleFS to hold
 * without truncating it into a different file.
 */
static bool safePath(const String &raw, String &out)
{
    if (raw.length() == 0 || raw[0] != '/') return false;
    if (raw.length() > FS_PATH_MAX) return false;

    out = "";
    out.reserve(raw.length());

    for (unsigned int i = 0; i < raw.length(); i++)
    {
        char c = raw[i];
        if (c == '\\' || (unsigned char)c < 0x20) return false;
        // Two slashes in a row are a typo, not a directory; collapse them
        // rather than creating a nameless one.
        if (c == '/' && out.length() > 0 && out[out.length() - 1] == '/') continue;
        out += c;
    }

    // A trailing slash would make "/assets" and "/assets/" two different
    // strings for one directory, and the tree asks about both.
    while (out.length() > 1 && out[out.length() - 1] == '/') out.remove(out.length() - 1);
    if (out.length() == 0) out = "/";

    // Segment by segment, so a name that merely starts with dots is fine.
    int at = 1;
    while (at <= (int)out.length())
    {
        int slash = out.indexOf('/', at);
        String segment = (slash < 0) ? out.substring(at) : out.substring(at, slash);
        if (segment == "." || segment == "..") return false;
        if (slash < 0) break;
        at = slash + 1;
    }

    return true;
}

static void sendError(int code, const char *what)
{
    String out = "{\"error\":\"";
    out += what;
    out += "\"}";
    server.send(code, "application/json", out);
}

/** The path of a request argument, already checked. False means answered. */
static bool argPath(const char *name, String &out)
{
    String raw = server.hasArg(name) ? server.arg(name) : String("/");
    if (safePath(raw, out)) return true;

    sendError(400, "fsPath");
    return false;
}

/** The path out of a JSON body, checked, and never the root. False: answered. */
static bool bodyPath(String &out)
{
    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok)
    {
        sendError(400, "fsBody");
        return false;
    }

    String raw = doc["path"] | String("");
    // The root is a directory that has to exist; every write here would be a
    // mistake if it were aimed at it.
    if (!safePath(raw, out) || out == "/")
    {
        sendError(400, "fsPath");
        return false;
    }
    return true;
}

/** Content type from the extension. Unknown means "download it". */
static const char *mimeFor(const String &path)
{
    if (path.endsWith(".html") || path.endsWith(".htm")) return "text/html";
    if (path.endsWith(".css"))  return "text/css";
    if (path.endsWith(".js"))   return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".svg"))  return "image/svg+xml";
    if (path.endsWith(".png"))  return "image/png";
    if (path.endsWith(".ico"))  return "image/x-icon";
    if (path.endsWith(".txt") || path.endsWith(".scad")) return "text/plain";
    if (path.endsWith(".gz"))   return "application/gzip";
    return "application/octet-stream";
}

/**
 * Whether the browser should offer to edit this file.
 *
 * A guess, and it only decides which buttons are drawn - the editor route
 * checks the size for itself. By extension rather than by content: sniffing a
 * file means reading it, and this answer is wanted for every entry in a
 * listing.
 */
static bool editable(const String &name, size_t size)
{
    if (size > FS_EDIT_MAX) return false;
    return name.endsWith(".json") || name.endsWith(".txt") || name.endsWith(".css") ||
           name.endsWith(".html") || name.endsWith(".htm") || name.endsWith(".js") ||
           name.endsWith(".csv")  || name.endsWith(".scad") || name.endsWith(".md") ||
           name.indexOf('.') < 0;   // no extension at all is usually a note
}

// ------ reading ------

/**
 * One directory, and how full the volume is.
 *
 * Per directory rather than the whole tree in one answer: the browser expands
 * a branch when it is opened, so a directory somebody filled with a thousand
 * files costs one slow response instead of making every response slow. The
 * volume figures ride along because the tab shows them above the tree, and
 * asking twice for something this cheap would be silly.
 */
static void sendList()
{
    if (!Expert::guard()) return;

    String path;
    if (!argPath("path", path)) return;

    File dir = LittleFS.open(path);
    if (!dir)               { sendError(404, "fsNotFound"); return; }
    if (!dir.isDirectory()) { dir.close(); sendError(400, "fsNotDir"); return; }

    JsonDocument doc;
    doc["path"]    = path;
    doc["total"]   = LittleFS.totalBytes();
    doc["used"]    = LittleFS.usedBytes();
    doc["editMax"] = FS_EDIT_MAX;
    JsonArray entries = doc["entries"].to<JsonArray>();

    uint16_t count = 0;
    File entry = dir.openNextFile();
    while (entry)
    {
        if (count++ >= FS_LIST_MAX) { doc["truncated"] = true; entry.close(); break; }

        // openNextFile() answers with the full path on this core, and the tree
        // wants the name; the browser rebuilds the path from where it sits.
        String full = entry.path();
        int slash = full.lastIndexOf('/');
        String name = (slash < 0) ? full : full.substring(slash + 1);

        JsonObject item = entries.add<JsonObject>();
        item["name"] = name;
        item["dir"]  = entry.isDirectory();
        item["size"] = entry.isDirectory() ? 0 : (uint32_t)entry.size();
        if (!entry.isDirectory()) item["edit"] = editable(name, entry.size());

        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

/**
 * A file, as it is.
 *
 * Streamed, so downloading the whole web UI does not need the heap to hold it.
 * `download=1` asks for the attachment header, which is the difference between
 * the browser saving index.html and the browser rendering it in place - and
 * rendering the clock's own index.html from here would look like the app had
 * gone wrong.
 */
static void sendRead()
{
    if (!Expert::guard()) return;

    String path;
    if (!argPath("path", path)) return;

    File file = LittleFS.open(path, "r");
    if (!file)              { sendError(404, "fsNotFound"); return; }
    if (file.isDirectory()) { file.close(); sendError(400, "fsIsDir"); return; }

    bool download = server.hasArg("download");
    if (download)
    {
        int slash = path.lastIndexOf('/');
        String name = path.substring(slash + 1);
        server.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
    }

    server.streamFile(file, download ? "application/octet-stream" : mimeFor(path));
    file.close();
}

// ------ writing ------

// The upload in flight. The data handler cannot answer - the response belongs
// to the done handler - so a refusal is recorded here and sent there, the same
// arrangement handleOtaUploadData() has.
static File uploadFile;
static String uploadTarget;
static String uploadPart;
static const char *uploadError = nullptr;
static size_t uploadWritten = 0;

/** Streams the body straight into the part file. Cannot send a response. */
static void handleUploadData()
{
    HTTPUpload &upload = server.upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        uploadError = nullptr;
        uploadWritten = 0;

        // Guarding only the done handler would let a stranger overwrite a file
        // and be refused afterwards, which is not a refusal.
        if (!Expert::unlocked()) { uploadError = "expertLocked"; return; }

        String raw = server.hasArg("path") ? server.arg("path") : String("");
        if (!safePath(raw, uploadTarget) || uploadTarget == "/")
        {
            uploadError = "fsPath";
            return;
        }

        uploadPart = uploadTarget + ".part";
        if (uploadPart.length() > FS_PATH_MAX) { uploadError = "fsPath"; return; }

        uploadFile = LittleFS.open(uploadPart, "w");
        if (!uploadFile) { uploadError = "fsOpen"; return; }

        debugI("FS: receiving %s", uploadTarget.c_str());
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (uploadError || !uploadFile) return;
        if (uploadFile.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            // Out of space, almost always. Stop rather than finish a file with
            // a hole in it and rename it over a good one.
            uploadError = "fsWrite";
            uploadFile.close();
            LittleFS.remove(uploadPart);
            return;
        }
        uploadWritten += upload.currentSize;
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (uploadFile) uploadFile.close();
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        if (uploadFile) uploadFile.close();
        LittleFS.remove(uploadPart);
        if (!uploadError) uploadError = "fsAborted";
    }
}

/** Renames the part file into place, and is the only one that can answer. */
static void handleUploadDone()
{
    if (uploadError)
    {
        sendError(strcmp(uploadError, "expertLocked") == 0 ? 403 : 400, uploadError);
        uploadError = nullptr;
        return;
    }

    // The rename is the moment the new file exists. Until here the old one is
    // untouched, which is what makes replacing index.html survivable.
    LittleFS.remove(uploadTarget);
    if (!LittleFS.rename(uploadPart, uploadTarget))
    {
        LittleFS.remove(uploadPart);
        sendError(500, "fsRename");
        return;
    }

    debugI("FS: wrote %s, %u bytes", uploadTarget.c_str(), (unsigned)uploadWritten);

    JsonDocument doc;
    doc["path"]  = uploadTarget;
    doc["size"]  = (uint32_t)uploadWritten;
    doc["total"] = LittleFS.totalBytes();
    doc["used"]  = LittleFS.usedBytes();

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

/**
 * What the in-browser editor saves: {path, content}.
 *
 * Buffered rather than streamed, which is the whole reason FS_EDIT_MAX exists
 * - the body is in the request buffer, the parsed string is a second copy, and
 * both are on the heap an OTA update wants. The same part-file dance as the
 * upload, for the same reason.
 */
static void saveText()
{
    if (!Expert::guard()) return;

    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok)
    {
        sendError(400, "fsBody");
        return;
    }

    String path;
    if (!safePath(doc["path"] | String(""), path) || path == "/")
    {
        sendError(400, "fsPath");
        return;
    }

    String content = doc["content"] | String("");
    if (content.length() > FS_EDIT_MAX) { sendError(413, "fsTooBig"); return; }

    String part = path + ".part";
    if (part.length() > FS_PATH_MAX) { sendError(400, "fsPath"); return; }

    File file = LittleFS.open(part, "w");
    if (!file) { sendError(500, "fsOpen"); return; }

    size_t written = file.print(content);
    file.close();

    if (written != content.length())
    {
        LittleFS.remove(part);
        sendError(507, "fsWrite");
        return;
    }

    LittleFS.remove(path);
    if (!LittleFS.rename(part, path))
    {
        LittleFS.remove(part);
        sendError(500, "fsRename");
        return;
    }

    debugI("FS: saved %s, %u bytes", path.c_str(), (unsigned)written);

    JsonDocument answer;
    answer["path"]  = path;
    answer["size"]  = (uint32_t)written;
    answer["total"] = LittleFS.totalBytes();
    answer["used"]  = LittleFS.usedBytes();

    String out;
    serializeJson(answer, out);
    server.send(200, "application/json", out);
}

/**
 * Deletes one file, or one empty directory.
 *
 * Not recursive, on purpose. A file explorer that empties a directory tree on
 * one click is how the web UI gets deleted by somebody who meant to tidy up,
 * and LittleFS gives no way back. Emptying a directory by hand is tedious
 * exactly in proportion to how much is being thrown away.
 */
static void removeEntry()
{
    if (!Expert::guard()) return;

    String path;
    if (!bodyPath(path)) return;

    File entry = LittleFS.open(path);
    if (!entry) { sendError(404, "fsNotFound"); return; }
    bool isDir = entry.isDirectory();

    if (isDir)
    {
        File child = entry.openNextFile();
        if (child) { child.close(); entry.close(); sendError(409, "fsNotEmpty"); return; }
    }
    entry.close();

    bool gone = isDir ? LittleFS.rmdir(path) : LittleFS.remove(path);
    if (!gone) { sendError(500, "fsDelete"); return; }

    debugW("FS: deleted %s", path.c_str());

    JsonDocument answer;
    answer["path"]  = path;
    answer["total"] = LittleFS.totalBytes();
    answer["used"]  = LittleFS.usedBytes();

    String out;
    serializeJson(answer, out);
    server.send(200, "application/json", out);
}

/** Creates one directory. Its parent has to exist; LittleFS does not do -p. */
static void makeDir()
{
    if (!Expert::guard()) return;

    String path;
    if (!bodyPath(path)) return;

    if (LittleFS.exists(path)) { sendError(409, "fsExists"); return; }
    if (!LittleFS.mkdir(path)) { sendError(500, "fsMkdir"); return; }

    JsonDocument answer;
    answer["path"] = path;

    String out;
    serializeJson(answer, out);
    server.send(200, "application/json", out);
}

// ------ the interface the rest of the program sees ------

void Files::begin()
{
    server.on("/fs/list",   HTTP_GET,  sendList);
    server.on("/fs/read",   HTTP_GET,  sendRead);
    server.on("/fs/upload", HTTP_POST, handleUploadDone, handleUploadData);
    server.on("/fs/save",   HTTP_POST, saveText);
    server.on("/fs/delete", HTTP_POST, removeEntry);
    server.on("/fs/mkdir",  HTTP_POST, makeDir);
}
