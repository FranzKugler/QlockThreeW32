/**
 * Expert
 * See Expert.h for the model and what it is and is not worth.
 *
 * @mc       ESP32S3
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  20.8.2026
 * @updated  20.8.2026
 */
#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_random.h>
#include <mbedtls/sha256.h>
#include <WebServer.h>

#include "Expert.h"
#include "LogBuffer.h"

extern WebServer server;

// Its own namespace, next to the settings' "qlock" rather than inside it.
#define EXPERT_NAMESPACE "qlockexpert"
#define KEY_SALT "salt"
#define KEY_HASH "hash"
#define KEY_ON   "on"

#define SALT_BYTES 16
#define HASH_BYTES 32

// A password shorter than this is not worth the flash write it costs.
#define MIN_PASSWORD 6

namespace
{
    bool haveHash = false;
    bool isUnlocked = false;

    // Taken once in begin(): whether this run started at the plug. Read from
    // esp_reset_reason() rather than tracked, so a software restart cannot
    // fake it.
    bool poweredOn = false;
    uint32_t startedAt = 0;

    uint8_t failures = 0;
    uint32_t lockoutUntil = 0;

    /** SHA-256 over salt then password. */
    void digest(const uint8_t *salt, const char *password, uint8_t out[HASH_BYTES])
    {
        mbedtls_sha256_context ctx;
        mbedtls_sha256_init(&ctx);
        mbedtls_sha256_starts(&ctx, 0);
        mbedtls_sha256_update(&ctx, salt, SALT_BYTES);
        mbedtls_sha256_update(&ctx, (const uint8_t *)password, strlen(password));
        mbedtls_sha256_finish(&ctx, out);
        mbedtls_sha256_free(&ctx);
    }

    /**
     * Compares without giving the answer away in the time it takes. Overkill
     * against someone on a home network, but it is three lines and the
     * alternative is memcmp, which returns as soon as two bytes differ.
     */
    bool sameDigest(const uint8_t *a, const uint8_t *b)
    {
        uint8_t difference = 0;
        for (size_t i = 0; i < HASH_BYTES; i++) difference |= a[i] ^ b[i];
        return difference == 0;
    }

    /** Writes the flag through, so an unlock survives the next restart. */
    void storeFlag(bool value)
    {
        Preferences preferences;
        if (!preferences.begin(EXPERT_NAMESPACE, false))
        {
            debugE("Cannot open NVS to store the expert flag");
            return;
        }
        preferences.putUChar(KEY_ON, value ? 1 : 0);
        preferences.end();
    }
}

void Expert::begin()
{
    startedAt = millis();
    poweredOn = esp_reset_reason() == ESP_RST_POWERON;

    Preferences preferences;
    if (!preferences.begin(EXPERT_NAMESPACE, true))
    {
        // No namespace yet is the normal state of a clock that has never been
        // enrolled - not an error, and not a reason to fall open.
        debugA("Expert mode: not set up on this clock");
        return;
    }

    haveHash = preferences.getBytesLength(KEY_HASH) == HASH_BYTES &&
               preferences.getBytesLength(KEY_SALT) == SALT_BYTES;
    isUnlocked = haveHash && preferences.getUChar(KEY_ON, 0) == 1;
    preferences.end();

    debugA("Expert mode: %s, %s%s",
           haveHash ? "password set" : "no password yet",
           isUnlocked ? "unlocked" : "locked",
           poweredOn ? ", reset window open" : "");
}

bool Expert::enrolled()
{
    return haveHash;
}

bool Expert::unlocked()
{
    return isUnlocked;
}

bool Expert::inGraceWindow()
{
    return poweredOn && (millis() - startedAt) < EXPERT_GRACE_MS;
}

uint32_t Expert::graceRemaining()
{
    if (!inGraceWindow()) return 0;
    return (EXPERT_GRACE_MS - (millis() - startedAt)) / 1000;
}

bool Expert::lockedOut()
{
    if (lockoutUntil == 0) return false;
    if ((int32_t)(millis() - lockoutUntil) >= 0)
    {
        lockoutUntil = 0;
        failures = 0;
        return false;
    }
    return true;
}

bool Expert::enroll(const char *password)
{
    if (haveHash || password == nullptr || strlen(password) < MIN_PASSWORD) return false;

    uint8_t salt[SALT_BYTES];
    esp_fill_random(salt, sizeof(salt));

    uint8_t hash[HASH_BYTES];
    digest(salt, password, hash);

    Preferences preferences;
    if (!preferences.begin(EXPERT_NAMESPACE, false))
    {
        debugE("Cannot open NVS to set the expert password");
        return false;
    }
    preferences.putBytes(KEY_SALT, salt, sizeof(salt));
    preferences.putBytes(KEY_HASH, hash, sizeof(hash));
    preferences.putUChar(KEY_ON, 1);
    preferences.end();

    haveHash = true;
    isUnlocked = true;
    // Never the password itself, here or anywhere else: this line ends up in
    // the ring, which is exactly what the password is guarding.
    debugA("Expert mode: password set, unlocked");
    return true;
}

bool Expert::unlock(const char *password)
{
    if (!haveHash || lockedOut() || password == nullptr) return false;

    Preferences preferences;
    if (!preferences.begin(EXPERT_NAMESPACE, true)) return false;

    uint8_t salt[SALT_BYTES];
    uint8_t stored[HASH_BYTES];
    bool read = preferences.getBytes(KEY_SALT, salt, sizeof(salt)) == sizeof(salt) &&
                preferences.getBytes(KEY_HASH, stored, sizeof(stored)) == sizeof(stored);
    preferences.end();
    if (!read) return false;

    uint8_t offered[HASH_BYTES];
    digest(salt, password, offered);

    if (!sameDigest(offered, stored))
    {
        if (++failures >= EXPERT_MAX_FAILURES)
        {
            lockoutUntil = millis() + EXPERT_LOCKOUT_MS;
            // millis() rolls over to 0 after 49 days and lockedOut() reads the
            // difference as signed, which handles the wrap - except for the one
            // value that means "no lockout".
            if (lockoutUntil == 0) lockoutUntil = 1;
            debugW("Expert mode: %d wrong passwords, no more attempts for %lu min",
                   failures, EXPERT_LOCKOUT_MS / 60000UL);
        }
        else
        {
            debugW("Expert mode: wrong password (%d of %d)", failures, EXPERT_MAX_FAILURES);
        }
        return false;
    }

    failures = 0;
    lockoutUntil = 0;
    isUnlocked = true;
    storeFlag(true);
    debugA("Expert mode: unlocked");
    return true;
}

void Expert::lock()
{
    if (!isUnlocked) return;
    isUnlocked = false;
    storeFlag(false);
    debugA("Expert mode: locked");
}

bool Expert::reset()
{
    if (!inGraceWindow()) return false;

    Preferences preferences;
    if (!preferences.begin(EXPERT_NAMESPACE, false)) return false;
    preferences.clear();
    preferences.end();

    haveHash = false;
    isUnlocked = false;
    failures = 0;
    lockoutUntil = 0;
    debugA("Expert mode: password cleared, clock is back to unconfigured");
    return true;
}

bool Expert::guard()
{
    if (isUnlocked) return true;
    server.send(403, "application/json", "{\"error\":\"expertLocked\"}");
    return false;
}
