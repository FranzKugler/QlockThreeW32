/**
 * OtaUpdate
 * See OtaUpdate.h. Lifted out of main .cpp, which had grown past 1900 lines
 * with this making up nearly a third of it.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  17.8.2026
 * @updated  17.8.2026
 */
#include <Arduino.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <Update.h>               // writing an image uploaded through the web UI
#include <HTTPClient.h>           // fetching the manifest and the images
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>       // checking a download against the manifest
#include <esp_ota_ops.h>          // which slot we are running from
#include <TimeLib.h>
#include <ArduinoJson.h>

#include "OtaUpdate.h"
#include "Settings.h"
#include "Version.h"

// Debug and the debugX macros, plus the ring the web UI reads them out of.
#include "LogBuffer.h"

// The two things this module uses but does not own.
extern WebServer server;
extern Settings settings;

// Provided by main .cpp: asks for the deferred settings write, so a channel
// change here goes to NVS on the same terms as every other setting rather than
// this module knowing about the delay.
void scheduleSettingsSave();

// Whether the clock currently has a network. Owned by main .cpp, which handles
// the connection; poll() takes it as a parameter, but the manifest fetch can
// also be triggered straight from a web request.
extern bool wifiConnected;

// --- over the air update through the web UI -------------------------------
// Milliseconds to wait between answering a finished upload and restarting, so
// the HTTP response is on the wire before the socket dies with the reboot.
#define OTA_REBOOT_DELAY 1000

// Empty unless the last update failed. A code rather than a sentence, for the
// same reason as wifiLastError above.
String otaError;
// Technical detail behind the code - an HTTP status, the Update library's own
// message. Shown as-is, never translated.
String otaErrorDetail;
// Version of the filesystem image, read from /version.json at boot.
String otaFsVersion;
// millis() at which to restart, 0 when no restart is pending.
unsigned long otaRebootAt = 0;
// U_FLASH or U_SPIFFS, decided from the first byte of the uploaded image.
int otaCommand = -1;

// --- update from the release channel ---------------------------------------
// The clocks never talk to the GitHub API, they poll one of two fixed URLs.
// Change these when forking; see .github/workflows/release.yml for how they
// are produced.
#define OTA_REPO "FranzKugler/QlockThreeW32"
#define OTA_MANIFEST_STABLE "https://github.com/" OTA_REPO "/releases/latest/download/manifest.json"
#define OTA_MANIFEST_EDGE   "https://github.com/" OTA_REPO "/releases/download/edge/manifest.json"

#define OTA_CHANNEL_STABLE 0
#define OTA_CHANNEL_EDGE   1

// Auto updates only run in this window, so the clock never goes dark in the
// evening for the minute an update takes.
#define OTA_AUTO_HOUR_FROM 2
#define OTA_AUTO_HOUR_TO   5

enum OtaState {
    OTA_STATE_IDLE,
    OTA_STATE_CHECKING,
    OTA_STATE_AVAILABLE,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_FAILED,
    OTA_STATE_INSTALLED       // waiting for the restart
};

// Written by the download task on the other core, read by the web handlers.
volatile OtaState otaState = OTA_STATE_IDLE;
volatile int otaProgress = 0;

// What the manifest of the selected channel offers.
String otaAvailableVersion;
String otaAvailableNotes;
String otaFirmwareUrl, otaFirmwareSha;
String otaFilesystemUrl, otaFilesystemSha;
size_t otaFirmwareSize = 0;
size_t otaFilesystemSize = 0;
time_t otaLastCheck = 0;
time_t otaNextCheck = 0;


// Version of the web UI currently in flash. The Vite build writes version.json
// into the filesystem image (see vite.config.js), so the two halves of an
// update can be told apart: firmware and web UI are flashed separately and can
// legitimately differ.
String readFsVersion()
{
    File file = LittleFS.open("/version.json", "r");
    if (!file) return String("");

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) return String("");

    return String(doc["version"] | "");
}

// Defined further down, with the manifest client.
bool otaUpdateAvailable();
bool otaFetchManifest();
bool otaStartInstall();

static const char *OTA_STATE_NAMES[] = {
    "idle", "checking", "available", "downloading", "failed", "installed"
};

void sendOtaStatus()
{
    JsonDocument doc;
    doc["firmwareVersion"] = FIRMWARE_VERSION;
    doc["fsVersion"] = otaFsVersion;
    doc["sketchSize"] = ESP.getSketchSize();
    // Size of the inactive OTA slot, i.e. the largest image that would fit.
    doc["freeSpace"] = ESP.getFreeSketchSpace();
    doc["error"] = otaError;
    doc["errorDetail"] = otaErrorDetail;

    // Which slot we booted from. Without this an update to the same version is
    // indistinguishable from nothing having happened at all.
    const esp_partition_t *running = esp_ota_get_running_partition();
    doc["partition"] = running ? running->label : "?";

    doc["channel"] = settings.getOtaChannel();
    doc["autoUpdate"] = (bool)settings.getOtaAutoUpdate();
    doc["checkInterval"] = settings.getOtaCheckInterval();

    doc["state"] = OTA_STATE_NAMES[otaState];
    doc["progress"] = otaProgress;
    doc["availableVersion"] = otaAvailableVersion;
    doc["availableNotes"] = otaAvailableNotes;
    doc["updateAvailable"] = otaUpdateAvailable();
    // Seconds since the last successful check, -1 if there has not been one.
    doc["lastCheck"] = otaLastCheck ? (long)(now() - otaLastCheck) : -1;

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

/** Asks the release channel what it has, then answers with the full status. */
void handleOtaCheck()
{
    otaFetchManifest();
    sendOtaStatus();
}

/** Accepts the request and lets the task do the work; answers immediately. */
void handleOtaInstall()
{
    if (!otaUpdateAvailable())
    {
        server.send(409, "application/json", "{\"error\":\"otaNoUpdate\"}");
        return;
    }
    if (!otaStartInstall())
    {
        server.send(409, "application/json", "{\"error\":\"otaBusy\"}");
        return;
    }
    sendOtaStatus();
}

void handleOtaConfig()
{
    JsonDocument doc;
    deserializeJson(doc, server.arg("plain"));

    if (!doc["channel"].isNull())
    {
        byte channel = doc["channel"];
        if (channel != settings.getOtaChannel())
        {
            settings.setOtaChannel(channel);
            // What the other channel offers says nothing about this one.
            otaAvailableVersion = "";
            otaAvailableNotes = "";
            otaLastCheck = 0;
            otaState = OTA_STATE_IDLE;
        }
    }
    if (!doc["autoUpdate"].isNull()) settings.setOtaAutoUpdate(doc["autoUpdate"]);
    if (!doc["checkInterval"].isNull()) settings.setOtaCheckInterval(doc["checkInterval"]);

    // Next check is due relative to the new interval.
    otaNextCheck = 0;
    scheduleSettingsSave();

    sendOtaStatus();
}

// Streaming half of POST /ota/upload: the web server calls this for every chunk
// of the multipart body, so the image goes straight to flash as it arrives and
// never has to be held in RAM (it is several times larger than the heap).
void handleOtaUploadData()
{
    HTTPUpload &upload = server.upload();

    switch (upload.status)
    {
        case UPLOAD_FILE_START:
        {
            otaError = "";
            otaErrorDetail = "";
            otaCommand = -1;    // decided below, from the first byte
            debugA("OTA upload started: %s", upload.filename.c_str());
            break;
        }
        case UPLOAD_FILE_WRITE:
        {
            if (otaError.length()) break;

            if (otaCommand < 0)
            {
                // ESP32 application images start with the magic byte 0xE9;
                // anything else is taken to be the filesystem image holding
                // the web UI. That saves the user from picking the target.
                otaCommand = (upload.currentSize > 0 && upload.buf[0] == 0xE9) ? U_FLASH : U_SPIFFS;
                debugA("OTA target: %s", otaCommand == U_FLASH ? "firmware" : "filesystem");

                if (otaCommand == U_SPIFFS)
                {
                    // Unmount before writing: LittleFS caches writes, and
                    // flushing them after the image has been written would
                    // corrupt it. The settings are unaffected - they live in
                    // NVS, which is a partition of its own.
                    LittleFS.end();
                }

                if (!Update.begin(UPDATE_SIZE_UNKNOWN, otaCommand))
                {
                    otaError = "otaBegin";
                    otaErrorDetail = Update.errorString();
                    debugE("OTA begin failed: %s", Update.errorString());
                    break;
                }
            }

            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            {
                otaError = "otaWrite";
                otaErrorDetail = Update.errorString();
                debugE("OTA write failed: %s", Update.errorString());
            }
            break;
        }
        case UPLOAD_FILE_END:
        {
            if (otaError.length()) break;

            // end(true) verifies the image and only then switches the boot
            // partition, so a truncated upload leaves the old one in charge.
            if (Update.end(true))
            {
                debugA("OTA upload finished: %u bytes", upload.totalSize);
            }
            else
            {
                otaError = "otaIncomplete";
                otaErrorDetail = Update.errorString();
                debugE("OTA end failed: %s", Update.errorString());
            }
            break;
        }
        case UPLOAD_FILE_ABORTED:
        {
            Update.abort();
            otaError = "otaAborted";
            debugW("OTA upload aborted");
            break;
        }
        default:
            break;
    }
}

// Answers POST /ota/upload once the whole body has been through
// handleOtaUploadData(), and schedules the restart on success.
void handleOtaUploadDone()
{
    if (otaError.length())
    {
        JsonDocument doc;
        doc["error"] = otaError;
        doc["errorDetail"] = otaErrorDetail;
        String out;
        serializeJson(doc, out);
        server.send(500, "application/json", out);
        return;
    }

    // Nothing was written at all: the request carried no file part.
    if (otaCommand < 0)
    {
        server.send(400, "application/json", "{\"error\":\"otaNoImage\"}");
        return;
    }

    server.sendHeader("Connection", "close");
    server.send(200, "application/json", "{\"msg\":\"\",\"reboot\":true}");
    otaRebootAt = millis() + OTA_REBOOT_DELAY;
}

// --- update from the release channel ---------------------------------------

// How long a download may go without delivering anything before it counts as
// dead. Used both for the socket's own read timeout and for the stall check in
// the loop, so a slow server cannot be cut off by one while the other waits.
#define OTA_STREAM_TIMEOUT_MS 20000

/**
 * TLS without certificate verification, on purpose.
 *
 * The SHA-256 in the manifest travels over the same connection as the image,
 * so it catches a truncated or corrupted download - which is the realistic
 * failure - but not somebody who can rewrite both. Pinning a root certificate
 * would look better than it is: /ota/upload next door accepts any image from
 * anyone on the network without asking. Worth revisiting the day that endpoint
 * gets authentication, and not before.
 */
void otaPrepareClient(WiFiClientSecure &client, HTTPClient &http)
{
    client.setInsecure();
    // Milliseconds. NetworkClientSecure inherits NetworkClient, and neither
    // overrides setTimeout - only NetworkServer does, and that one takes
    // seconds. So this falls through to Stream::setTimeout, which is in
    // milliseconds. It read 20 here, and 20 ms is not enough for readBytes()
    // to wait out the next TLS record of a multi-megabyte transfer: it
    // returned 0, the download loop treated that as the end of the stream,
    // and every install failed as "otaSize".
    client.setTimeout(OTA_STREAM_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(OTA_STREAM_TIMEOUT_MS);
    http.setUserAgent("QlockThreeW32/" FIRMWARE_VERSION);
}

const char *otaManifestUrl()
{
    return settings.getOtaChannel() == OTA_CHANNEL_EDGE ? OTA_MANIFEST_EDGE : OTA_MANIFEST_STABLE;
}

/** Compares major.minor.patch, ignoring anything after a '-'. Only meaningful on stable. */
int otaCompareVersions(const String &left, const String &right)
{
    int a[3] = {0, 0, 0};
    int b[3] = {0, 0, 0};
    sscanf(left.c_str(), "%d.%d.%d", &a[0], &a[1], &a[2]);
    sscanf(right.c_str(), "%d.%d.%d", &b[0], &b[1], &b[2]);
    for (int i = 0; i < 3; i++)
    {
        if (a[i] != b[i]) return a[i] > b[i] ? 1 : -1;
    }
    return 0;
}

/**
 * Whether an offered version should replace an installed one.
 *
 * On stable that means strictly newer. On edge it means merely different:
 * `git describe` output like 2.0.0-4-gabc123 has no order, so "newer" is not
 * defined - and after a reverted commit the clock would otherwise never offer
 * anything again.
 */
bool otaShouldReplace(const String &offered, const String &installed)
{
    if (offered.length() == 0) return false;
    if (settings.getOtaChannel() == OTA_CHANNEL_EDGE) return offered != installed;
    return otaCompareVersions(offered, installed) > 0;
}

bool otaFirmwareOutdated() { return otaShouldReplace(otaAvailableVersion, String(FIRMWARE_VERSION)); }
bool otaFilesystemOutdated() { return otaShouldReplace(otaAvailableVersion, otaFsVersion); }
bool otaUpdateAvailable() { return otaFirmwareOutdated() || otaFilesystemOutdated(); }

/** Fetches and parses the manifest of the selected channel. Blocks for a second or two. */
bool otaFetchManifest()
{
    if (!wifiConnected) return false;

    otaState = OTA_STATE_CHECKING;
    otaError = "";
    otaErrorDetail = "";

    WiFiClientSecure client;
    HTTPClient http;
    otaPrepareClient(client, http);

    const char *url = otaManifestUrl();
    debugI("Checking for updates: %s", url);

    if (!http.begin(client, url))
    {
        otaError = "otaServer";
        otaState = OTA_STATE_FAILED;
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        // 404 on stable simply means no tagged release exists yet.
        otaError = "otaManifestHttp";
        otaErrorDetail = String("HTTP ") + code;
        debugW("Manifest fetch failed: HTTP %d", code);
        http.end();
        otaState = OTA_STATE_FAILED;
        return false;
    }

    JsonDocument doc;
    DeserializationError parseError = deserializeJson(doc, http.getStream());
    http.end();

    if (parseError)
    {
        otaError = "otaManifestParse";
        debugE("Manifest parse failed: %s", parseError.c_str());
        otaState = OTA_STATE_FAILED;
        return false;
    }

    otaAvailableVersion = String(doc["version"] | "");
    otaAvailableNotes = String(doc["notes"] | "");
    otaFirmwareUrl = String(doc["firmware"]["url"] | "");
    otaFirmwareSha = String(doc["firmware"]["sha256"] | "");
    otaFirmwareSize = doc["firmware"]["size"] | 0;
    otaFilesystemUrl = String(doc["filesystem"]["url"] | "");
    otaFilesystemSha = String(doc["filesystem"]["sha256"] | "");
    otaFilesystemSize = doc["filesystem"]["size"] | 0;

    otaLastCheck = now();
    otaState = otaUpdateAvailable() ? OTA_STATE_AVAILABLE : OTA_STATE_IDLE;
    debugA("Manifest says %s, %s", otaAvailableVersion.c_str(),
           otaUpdateAvailable() ? "update available" : "up to date");
    return true;
}

/** Lower-case hex of a 32 byte digest. */
String otaHex(const unsigned char *digest)
{
    static const char nibble[] = "0123456789abcdef";
    String out;
    out.reserve(64);
    for (int i = 0; i < 32; i++)
    {
        out += nibble[digest[i] >> 4];
        out += nibble[digest[i] & 0x0f];
    }
    return out;
}

/**
 * Streams one image into its partition, hashing as it goes. The boot partition
 * is only switched once the digest matches, so a corrupted download cannot
 * take the clock down.
 */
bool otaDownloadImage(const String &url, const String &expectedSha, size_t expectedSize, int command)
{
    WiFiClientSecure client;
    HTTPClient http;
    otaPrepareClient(client, http);

    debugA("Downloading %s (%u bytes)", url.c_str(), (unsigned)expectedSize);

    if (!http.begin(client, url))
    {
        otaError = "otaDownload";
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        otaError = "otaDownload";
        otaErrorDetail = String("HTTP ") + code;
        http.end();
        return false;
    }

    if (command == U_SPIFFS)
    {
        // Cached writes would otherwise be flushed over the new image.
        LittleFS.end();
    }

    if (!Update.begin(expectedSize, command))
    {
        otaError = "otaBegin";
        otaErrorDetail = Update.errorString();
        http.end();
        return false;
    }

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer[1024];
    size_t written = 0;
    unsigned long lastData = millis();

    while (written < expectedSize)
    {
        size_t available = stream->available();
        // Never ask for more than is still wanted: a response one byte longer
        // than the manifest claims would otherwise overshoot and be reported
        // as the wrong size despite having arrived intact.
        size_t wanted = min(min(available, sizeof(buffer)), expectedSize - written);

        int read = wanted ? stream->readBytes(buffer, wanted) : 0;

        if (read > 0)
        {
            mbedtls_sha256_update(&sha, buffer, read);
            if (Update.write(buffer, read) != (size_t)read)
            {
                otaError = "otaWrite";
                otaErrorDetail = Update.errorString();
                break;
            }

            written += read;
            otaProgress = (written * 100) / expectedSize;
            lastData = millis();
        }
        else
        {
            // Nothing this time round. That is not the end of the stream - a
            // TLS record can simply not have arrived yet - so only give up
            // once the connection is gone or nothing has come for a while.
            if (!http.connected() || millis() - lastData > OTA_STREAM_TIMEOUT_MS)
            {
                otaError = "otaConnectionLost";
                break;
            }
            delay(5);
        }
    }

    unsigned char digest[32];
    mbedtls_sha256_finish(&sha, digest);
    mbedtls_sha256_free(&sha);
    http.end();

    // Let go of the TLS session before the image is verified.
    //
    // http.end() closes the connection but leaves the mbedTLS context on the
    // client, which is tens of kilobytes, and the client only goes out of
    // scope when this function returns - after Update.end(). Activation runs
    // esp_image_verify(), which has to map and hash the whole partition, and
    // that has been seen to fail on one clock while succeeding on the next
    // attempt with the same images: the signature of not enough room rather
    // than a bad image.
    client.stop();

    if (otaError.length())
    {
        Update.abort();
        return false;
    }

    if (written != expectedSize)
    {
        otaError = "otaSize";
        // Says how far it got, which is the difference between "the server
        // sent the wrong thing" and "the transfer stopped part way".
        otaErrorDetail = String(written) + "/" + String(expectedSize);
        Update.abort();
        return false;
    }

    if (otaHex(digest) != expectedSha)
    {
        otaError = "otaChecksum";
        debugE("SHA-256 mismatch: got %s, expected %s", otaHex(digest).c_str(), expectedSha.c_str());
        Update.abort();
        return false;
    }

    // Logged either way: when activation fails there is nothing else to go on,
    // and the number is the whole question.
    debugA("Verifying image, %u bytes free (largest block %u)",
           (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

    if (!Update.end(true))
    {
        otaError = "otaIncomplete";
        otaErrorDetail = Update.errorString();
        debugE("Activation failed: %s, %u bytes free", Update.errorString(),
               (unsigned)ESP.getFreeHeap());
        return false;
    }

    debugA("Installed %u bytes, digest verified", (unsigned)written);
    return true;
}

/**
 * Runs the whole install off the loop, so the web server keeps answering and
 * the UI can show progress during the minute or so this takes.
 */
void otaInstallTask(void *unused)
{
    bool ok = true;

    // Firmware first, and the order matters twice over.
    //
    // Update is a singleton, and a second session in the same boot does not
    // activate: installing the filesystem first and the firmware after it got
    // both images down intact, digests and all, and then failed in
    // esp_ota_set_boot_partition with "Could Not Activate The Firmware".
    // Alone, the same firmware install goes through. This way round the
    // firmware gets the fresh session, and the filesystem - which needs no
    // boot partition switched, only bytes written - gets the second.
    //
    // It is also the safer half to fail on. Update.begin(U_SPIFFS) erases the
    // partition before the download starts, so an interrupted filesystem
    // install leaves the clock with no web UI until someone reaches it over
    // USB. Doing it last means a firmware that fails cannot cost the UI.
    if (ok && otaFirmwareOutdated() && otaFirmwareUrl.length())
    {
        ok = otaDownloadImage(otaFirmwareUrl, otaFirmwareSha, otaFirmwareSize, U_FLASH);
    }
    if (ok && otaFilesystemOutdated() && otaFilesystemUrl.length())
    {
        ok = otaDownloadImage(otaFilesystemUrl, otaFilesystemSha, otaFilesystemSize, U_SPIFFS);
    }

    if (ok)
    {
        otaState = OTA_STATE_INSTALLED;
        otaProgress = 100;
        otaRebootAt = millis() + OTA_REBOOT_DELAY;
    }
    else
    {
        otaState = OTA_STATE_FAILED;
        debugE("Update failed: %s", otaError.c_str());
    }

    vTaskDelete(NULL);
}

/** Starts the install unless one is already running. */
bool otaStartInstall()
{
    if (otaState == OTA_STATE_DOWNLOADING) return false;
    if (!otaUpdateAvailable()) return false;

    otaError = "";

    otaErrorDetail = "";
    otaProgress = 0;
    otaState = OTA_STATE_DOWNLOADING;

    // Pinned to core 0; loop() and the web server have core 1 to themselves.
    return xTaskCreatePinnedToCore(otaInstallTask, "otaInstall", 10240, NULL, 1, NULL, 0) == pdPASS;
}



// ------ the interface the rest of the program sees ------

void Ota::begin()
{
    otaFsVersion = readFsVersion();

    server.on("/ota/status", HTTP_GET, sendOtaStatus);
    server.on("/ota/upload", HTTP_POST, handleOtaUploadDone, handleOtaUploadData);
    server.on("/ota/check", HTTP_GET, handleOtaCheck);
    server.on("/ota/install", HTTP_POST, handleOtaInstall);
    server.on("/ota/config", HTTP_POST, handleOtaConfig);
}

void Ota::scheduleRestart()
{
    otaRebootAt = millis() + OTA_REBOOT_DELAY;
}

bool Ota::restartPending()
{
    return otaRebootAt != 0;
}

bool Ota::restartDue()
{
    return otaRebootAt != 0 && (long)(millis() - otaRebootAt) >= 0;
}

void Ota::poll(bool online, int localHour)
{
    // Needs the time, both for the interval and for the night window - before
    // the first NTP answer now() is meaningless, which the caller signals by
    // passing a negative hour.
    if (!online || localHour < 0) return;
    if (otaState == OTA_STATE_DOWNLOADING || otaRebootAt != 0) return;
    if (settings.getOtaCheckInterval() == 0) return;

    if (otaNextCheck == 0)
    {
        // Not immediately after boot: the clock has better things to do.
        otaNextCheck = now() + 120;
        return;
    }

    if (now() < otaNextCheck) return;

    otaNextCheck = now() + (time_t)settings.getOtaCheckInterval() * 3600;
    otaFetchManifest();

    if (!settings.getOtaAutoUpdate() || !otaUpdateAvailable()) return;

    if (localHour >= OTA_AUTO_HOUR_FROM && localHour < OTA_AUTO_HOUR_TO)
    {
        debugA("Installing %s automatically", otaAvailableVersion.c_str());
        otaStartInstall();
    }
    else
    {
        debugI("Update %s waiting for the night window", otaAvailableVersion.c_str());
    }
}
