/**
 * NvsRoutes
 * The /nvs endpoints: list, read, save, delete.
 *
 * See NvsRoutes.h for the pretence this is built on and exactly where it
 * stops. What is worth knowing down here is why the raw ESP-IDF calls are
 * used rather than Preferences, which the rest of the firmware uses: this has
 * to walk namespaces it does not know the names of, and read values whose type
 * it only learns while walking. Preferences wraps one namespace of a known
 * shape, which is the right tool everywhere else and the wrong one here.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.2
 * @created  22.8.2026
 * @updated  22.8.2026
 */
#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <nvs.h>
#include <nvs_flash.h>

#include "NvsRoutes.h"
#include "Expert.h"
#include "LogBuffer.h"

extern WebServer server;

// The default NVS partition, which is the only one this build has.
#define NVS_PARTITION "nvs"

// The one value that is not readable. It is listed - a tree that hides
// entries is a tree that lies - but a hash and its salt carried away during a
// borrowed thirty seconds of unlock are crackable offline forever, and
// probably against a password used somewhere else too. Everything else here
// stops being readable the moment the clock is locked again; this would not.
#define EXPERT_NAMESPACE "qlockexpert"

static bool isProtected(const char *ns, const char *key)
{
    return strcmp(ns, EXPERT_NAMESPACE) == 0 &&
           (strcmp(key, "hash") == 0 || strcmp(key, "salt") == 0);
}

static void sendError(int code, const char *what)
{
    String out = "{\"error\":\"";
    out += what;
    out += "\"}";
    server.send(code, "application/json", out);
}

/** The type as the browser sees it, and as this file talks about it. */
static const char *typeName(nvs_type_t type)
{
    switch (type)
    {
        case NVS_TYPE_U8:   return "u8";
        case NVS_TYPE_I8:   return "i8";
        case NVS_TYPE_U16:  return "u16";
        case NVS_TYPE_I16:  return "i16";
        case NVS_TYPE_U32:  return "u32";
        case NVS_TYPE_I32:  return "i32";
        case NVS_TYPE_U64:  return "u64";
        case NVS_TYPE_I64:  return "i64";
        case NVS_TYPE_STR:  return "str";
        case NVS_TYPE_BLOB: return "blob";
        default:            return "?";
    }
}

static bool isInteger(nvs_type_t type)
{
    switch (type)
    {
        case NVS_TYPE_U8: case NVS_TYPE_I8:
        case NVS_TYPE_U16: case NVS_TYPE_I16:
        case NVS_TYPE_U32: case NVS_TYPE_I32:
        case NVS_TYPE_U64: case NVS_TYPE_I64: return true;
        default: return false;
    }
}

/**
 * One value as text, whatever it is underneath.
 *
 * Strings come out as they are, integers as decimal, and a blob is refused -
 * it has no text form, which is what makes it a download rather than an edit.
 * `type` is filled in either way, so a caller that only wanted to know the
 * shape gets it.
 */
static bool readValue(const char *ns, const char *key, String &out,
                      nvs_type_t &type, size_t &length, const char *&error)
{
    nvs_handle_t handle;
    if (nvs_open_from_partition(NVS_PARTITION, ns, NVS_READONLY, &handle) != ESP_OK)
    {
        error = "nvsNamespace";
        return false;
    }

    // The iterator knew the type; a direct read does not, so it is found by
    // asking for the length of each candidate. Cheaper than it looks - the
    // lookup is the same one the read would do.
    size_t size = 0;
    bool ok = false;

    if (nvs_get_str(handle, key, nullptr, &size) == ESP_OK)
    {
        type = NVS_TYPE_STR;
        length = size > 0 ? size - 1 : 0;   // NVS counts the terminator
        if (length > NVS_EDIT_MAX) { error = "nvsTooBig"; }
        else
        {
            char *buffer = (char *)malloc(size);
            if (buffer == nullptr) { error = "nvsMemory"; }
            else
            {
                if (nvs_get_str(handle, key, buffer, &size) == ESP_OK)
                {
                    out = buffer;
                    ok = true;
                }
                else error = "nvsRead";
                free(buffer);
            }
        }
    }
    else if (nvs_get_blob(handle, key, nullptr, &size) == ESP_OK)
    {
        type = NVS_TYPE_BLOB;
        length = size;
        error = "nvsBinary";
    }
    else
    {
        // The integer widths, in the order that costs least to get wrong: a
        // narrower read of a wider value fails, so asking narrow first and
        // widening finds the one the store actually holds.
        int64_t value = 0;
        uint64_t unsignedValue = 0;
        uint8_t u8; int8_t i8; uint16_t u16; int16_t i16;
        uint32_t u32; int32_t i32; uint64_t u64; int64_t i64;

        if      (nvs_get_u8(handle, key, &u8)   == ESP_OK) { type = NVS_TYPE_U8;  unsignedValue = u8;  ok = true; }
        else if (nvs_get_i8(handle, key, &i8)   == ESP_OK) { type = NVS_TYPE_I8;  value = i8;  ok = true; }
        else if (nvs_get_u16(handle, key, &u16) == ESP_OK) { type = NVS_TYPE_U16; unsignedValue = u16; ok = true; }
        else if (nvs_get_i16(handle, key, &i16) == ESP_OK) { type = NVS_TYPE_I16; value = i16; ok = true; }
        else if (nvs_get_u32(handle, key, &u32) == ESP_OK) { type = NVS_TYPE_U32; unsignedValue = u32; ok = true; }
        else if (nvs_get_i32(handle, key, &i32) == ESP_OK) { type = NVS_TYPE_I32; value = i32; ok = true; }
        else if (nvs_get_u64(handle, key, &u64) == ESP_OK) { type = NVS_TYPE_U64; unsignedValue = u64; ok = true; }
        else if (nvs_get_i64(handle, key, &i64) == ESP_OK) { type = NVS_TYPE_I64; value = i64; ok = true; }
        else error = "nvsNotFound";

        if (ok)
        {
            char text[24];
            if (type == NVS_TYPE_U8 || type == NVS_TYPE_U16 ||
                type == NVS_TYPE_U32 || type == NVS_TYPE_U64)
                snprintf(text, sizeof(text), "%llu", (unsigned long long)unsignedValue);
            else
                snprintf(text, sizeof(text), "%lld", (long long)value);
            out = text;
            length = out.length();
        }
    }

    nvs_close(handle);
    return ok;
}

/**
 * The suffix this module is willing to put on a value.
 *
 * An opinion, not a fact - nothing in NVS records a file name. A string that
 * begins with a brace or a bracket is offered as `.json` because that is what
 * every record this clock writes looks like; anything else is `.txt` or,
 * having no text form at all, `.bin`.
 */
static const char *suffixFor(nvs_type_t type, const String &text, size_t length)
{
    if (type == NVS_TYPE_BLOB) return "bin";
    if (isInteger(type)) return "txt";
    if (length > NVS_PEEK_MAX) return "bin";   // not read, so not judged

    for (unsigned int i = 0; i < text.length(); i++)
    {
        char c = text[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        return (c == '{' || c == '[') ? "json" : "txt";
    }
    return "txt";
}

/**
 * Everything in NVS, in one answer.
 *
 * One request rather than one per namespace, unlike the LittleFS tree: there
 * are a few dozen entries on this clock and the iterator walks the whole
 * partition anyway, so splitting it would mean walking it once per folder.
 * The browser groups by namespace itself.
 *
 * The value of every string short enough to peek at is read here, because the
 * suffix depends on it and a tree that shows the wrong kind of file is worse
 * than a listing that costs a few hundred bytes of heap to build.
 */
static void sendList()
{
    if (!Expert::guard()) return;

    JsonDocument doc;
    JsonArray entries = doc["entries"].to<JsonArray>();

    nvs_iterator_t iterator = nullptr;
    esp_err_t found = nvs_entry_find(NVS_PARTITION, nullptr, NVS_TYPE_ANY, &iterator);
    uint16_t count = 0;

    while (found == ESP_OK && iterator != nullptr)
    {
        if (count++ >= NVS_LIST_MAX) { doc["truncated"] = true; break; }

        nvs_entry_info_t info;
        nvs_entry_info(iterator, &info);

        JsonObject item = entries.add<JsonObject>();
        item["ns"]   = info.namespace_name;
        item["key"]  = info.key;
        item["type"] = typeName(info.type);

        if (isProtected(info.namespace_name, info.key))
        {
            // Listed, and that is all. See the note at the top of the file.
            item["suffix"]    = "bin";
            item["protected"] = true;
            item["size"]      = 0;
        }
        else
        {
            String text;
            nvs_type_t type = info.type;
            size_t length = 0;
            const char *error = nullptr;
            bool ok = readValue(info.namespace_name, info.key, text, type, length, error);

            item["size"]   = (uint32_t)length;
            item["suffix"] = suffixFor(type, ok ? text : String(""), length);
            // Only a string short enough to hold twice can be edited in place.
            item["edit"]   = ok && type != NVS_TYPE_BLOB && length <= NVS_EDIT_MAX;
        }

        found = nvs_entry_next(&iterator);
    }
    nvs_release_iterator(iterator);

    // NVS accounts for itself in 32-byte entries, so that is what the fullness
    // bar is given. Inventing a byte count would be worse than an odd unit.
    nvs_stats_t stats;
    if (nvs_get_stats(NVS_PARTITION, &stats) == ESP_OK)
    {
        doc["used"]       = (uint32_t)stats.used_entries;
        doc["total"]      = (uint32_t)stats.total_entries;
        doc["namespaces"] = (uint32_t)stats.namespace_count;
    }
    doc["editMax"] = NVS_EDIT_MAX;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

/**
 * A blob, as the bytes it is.
 *
 * The one entry kind with no text form, so it is always a download and never
 * an edit - which is exactly what `.bin` in the tree is telling the reader.
 * Held whole in the heap rather than streamed: NVS has no partial read, so
 * there is nothing to stream from, and the cap keeps that honest.
 */
static void sendBlob(const String &ns, const String &key, size_t length)
{
    if (length > NVS_EDIT_MAX) { sendError(413, "nvsTooBig"); return; }

    nvs_handle_t handle;
    if (nvs_open_from_partition(NVS_PARTITION, ns.c_str(), NVS_READONLY, &handle) != ESP_OK)
    {
        sendError(404, "nvsNamespace");
        return;
    }

    uint8_t *buffer = (uint8_t *)malloc(length ? length : 1);
    if (buffer == nullptr) { nvs_close(handle); sendError(500, "nvsMemory"); return; }

    size_t size = length;
    esp_err_t result = nvs_get_blob(handle, key.c_str(), buffer, &size);
    nvs_close(handle);

    if (result != ESP_OK) { free(buffer); sendError(500, "nvsRead"); return; }

    server.sendHeader("Content-Disposition", "attachment; filename=\"" + key + ".bin\"");
    server.send_P(200, "application/octet-stream", (const char *)buffer, size);
    free(buffer);
}

/** One value, as text where it has one and as bytes where it does not. */
static void sendRead()
{
    if (!Expert::guard()) return;

    String ns = server.arg("ns");
    String key = server.arg("key");
    if (ns.length() == 0 || key.length() == 0) { sendError(400, "nvsPath"); return; }

    if (isProtected(ns.c_str(), key.c_str())) { sendError(403, "nvsProtected"); return; }

    String text;
    nvs_type_t type = NVS_TYPE_ANY;
    size_t length = 0;
    const char *error = nullptr;

    if (!readValue(ns.c_str(), key.c_str(), text, type, length, error))
    {
        // A blob has no text form, so it is offered as a download instead of
        // being refused outright - which is what `.bin` in the tree means.
        if (strcmp(error ? error : "", "nvsBinary") == 0) { sendBlob(ns, key, length); return; }
        sendError(404, error ? error : "nvsNotFound");
        return;
    }

    if (server.hasArg("download"))
    {
        server.sendHeader("Content-Disposition",
                          "attachment; filename=\"" + key + "." +
                          suffixFor(type, text, length) + "\"");
        server.send(200, "application/octet-stream", text);
        return;
    }

    server.send(200, strcmp(suffixFor(type, text, length), "json") == 0
                         ? "application/json" : "text/plain", text);
}

/** Writes a value back, keeping the type the store already has for the key. */
static void saveValue()
{
    if (!Expert::guard()) return;

    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok)
    {
        sendError(400, "nvsBody");
        return;
    }

    String ns = doc["ns"] | String("");
    String key = doc["key"] | String("");
    String content = doc["content"] | String("");
    if (ns.length() == 0 || key.length() == 0) { sendError(400, "nvsPath"); return; }
    if (isProtected(ns.c_str(), key.c_str())) { sendError(403, "nvsProtected"); return; }
    if (content.length() > NVS_EDIT_MAX) { sendError(413, "nvsTooBig"); return; }

    // What is there now decides what goes back. Writing a string over an
    // integer would change the type of a key the firmware then reads with
    // nvs_get_u8 and finds missing - a setting that silently reverts to its
    // default, which is the worst way for this to go wrong.
    String before;
    nvs_type_t type = NVS_TYPE_ANY;
    size_t length = 0;
    const char *error = nullptr;
    if (!readValue(ns.c_str(), key.c_str(), before, type, length, error))
    {
        // A blob was found and has no text form, which is a different answer
        // from "there is no such key" and deserves a different status.
        const char *code = error ? error : "nvsNotFound";
        sendError(strcmp(code, "nvsNotFound") == 0 ? 404 : 400, code);
        return;
    }

    nvs_handle_t handle;
    if (nvs_open_from_partition(NVS_PARTITION, ns.c_str(), NVS_READWRITE, &handle) != ESP_OK)
    {
        sendError(500, "nvsNamespace");
        return;
    }

    esp_err_t result = ESP_FAIL;
    if (type == NVS_TYPE_STR)
    {
        result = nvs_set_str(handle, key.c_str(), content.c_str());
    }
    else if (isInteger(type))
    {
        char *end = nullptr;
        long long value = strtoll(content.c_str(), &end, 10);
        while (end && (*end == ' ' || *end == '\r' || *end == '\n')) end++;
        if (end == nullptr || *end != '\0')
        {
            nvs_close(handle);
            sendError(400, "nvsNotANumber");
            return;
        }
        switch (type)
        {
            case NVS_TYPE_U8:  result = nvs_set_u8(handle, key.c_str(), (uint8_t)value); break;
            case NVS_TYPE_I8:  result = nvs_set_i8(handle, key.c_str(), (int8_t)value); break;
            case NVS_TYPE_U16: result = nvs_set_u16(handle, key.c_str(), (uint16_t)value); break;
            case NVS_TYPE_I16: result = nvs_set_i16(handle, key.c_str(), (int16_t)value); break;
            case NVS_TYPE_U32: result = nvs_set_u32(handle, key.c_str(), (uint32_t)value); break;
            case NVS_TYPE_I32: result = nvs_set_i32(handle, key.c_str(), (int32_t)value); break;
            case NVS_TYPE_U64: result = nvs_set_u64(handle, key.c_str(), (uint64_t)value); break;
            case NVS_TYPE_I64: result = nvs_set_i64(handle, key.c_str(), (int64_t)value); break;
            default: break;
        }
    }
    else
    {
        nvs_close(handle);
        sendError(400, "nvsBinary");
        return;
    }

    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);

    if (result != ESP_OK) { sendError(500, "nvsWrite"); return; }

    debugW("NVS: %s/%s rewritten, %u bytes", ns.c_str(), key.c_str(),
           (unsigned)content.length());

    JsonDocument answer;
    answer["ns"]   = ns;
    answer["key"]  = key;
    answer["size"] = (uint32_t)content.length();
    // The firmware keeps its records in RAM and writes them back on a change,
    // so an edit made here is only as durable as the next settings save. The
    // browser says so; saying it twice is cheap and the second one is the one
    // a curl user sees.
    answer["cached"] = true;

    String out;
    serializeJson(answer, out);
    server.send(200, "application/json", out);
}

/** Erases one key. The namespace goes with the last key in it. */
static void removeKey()
{
    if (!Expert::guard()) return;

    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok)
    {
        sendError(400, "nvsBody");
        return;
    }

    String ns = doc["ns"] | String("");
    String key = doc["key"] | String("");
    if (ns.length() == 0 || key.length() == 0) { sendError(400, "nvsPath"); return; }

    nvs_handle_t handle;
    if (nvs_open_from_partition(NVS_PARTITION, ns.c_str(), NVS_READWRITE, &handle) != ESP_OK)
    {
        sendError(404, "nvsNamespace");
        return;
    }

    esp_err_t result = nvs_erase_key(handle, key.c_str());
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);

    if (result != ESP_OK) { sendError(500, "nvsDelete"); return; }

    debugW("NVS: %s/%s erased", ns.c_str(), key.c_str());

    JsonDocument answer;
    answer["ns"]  = ns;
    answer["key"] = key;

    String out;
    serializeJson(answer, out);
    server.send(200, "application/json", out);
}

void Nvs::begin()
{
    server.on("/nvs/list",   HTTP_GET,  sendList);
    server.on("/nvs/read",   HTTP_GET,  sendRead);
    server.on("/nvs/save",   HTTP_POST, saveValue);
    server.on("/nvs/delete", HTTP_POST, removeKey);
}
