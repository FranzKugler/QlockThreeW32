# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

QlockThreeW32 is ESP32 firmware for a "word clock": a letter matrix backlit so the current time reads as a sentence (in German, Swiss German, English, French, Italian, Dutch, or Spanish), updated every 5 minutes, with four corner LEDs indicating the remaining minutes. It's a long-running hobby project (originally AVR-based, later ported to ESP32) and carries some legacy cruft from that history — expect dead code paths and commented-out alternatives, though the project has been consolidated onto a single hardware target (see below).

The repo has three parts:
- **Firmware** (`src/`) — PlatformIO/Arduino C++ for the ESP32, drives the LED matrix and hosts a small config web server.
- **Web UI** (`web/`) — a Svelte 5 + Vite single-page app, built into `data/` and flashed to the ESP32's LittleFS filesystem, where the firmware's web server serves it. `data/` is generated output and is gitignored; run `npm run build` before `pio run -t uploadfs`.
- **Mock server** (`server.js`) — a Node/Express stand-in for the firmware's REST API, used to develop the UI on a desktop browser without hardware.

The target hardware is fixed: a Seeed XIAO ESP32-S3 driving a 114-pixel WS2812B strip. Support for other boards (NodeMCU-32S, ESP32-C3) and other LED drivers/peripherals from the project's history has been removed — see "Consolidation history" below.

## Commands

### Firmware (PlatformIO)

`pio` is not on PATH in this environment, but is installed via the PlatformIO VSCode extension at `%USERPROFILE%\.platformio\penv\Scripts\pio.exe`. Invoke it by full path (or add that directory to PATH):

```
pio run                          # build (uses default_envs from platformio.ini)
pio run -e seeed_xiao_esp32s3    # build a specific environment
pio run -t upload                # build and flash firmware
pio run -t uploadfs              # build and flash the data/ folder as a LittleFS image
pio run -t buildfs               # build littlefs.bin without flashing (for a browser OTA)
pio device monitor                # serial monitor (115200 baud)
```

There are no automated tests in this project.

[platformio.ini](platformio.ini) defines a single build environment, `seeed_xiao_esp32s3` (`LED_OUTPUT_PIN=4`, USB serial upload via esptool at 460800 baud).

The target is the **XIAO ESP32-S3 Plus with 16 MB flash**, but PlatformIO's board definition describes the 8 MB model and would default to `default_8MB.csv`, wasting half the flash. The environment therefore overrides `board_upload.flash_size`, `board_upload.maximum_size` and points `board_build.partitions` at [partitions.csv](partitions.csv) — stock `default_16MB.csv` layout: 6.5 MB per OTA slot (firmware uses ~1.3 MB) and a 3.5 MB LittleFS partition. Reflashing with a changed partition table moves the filesystem, so `uploadfs` has to be run again; do that over USB rather than OTA. The settings survive it, as they live in NVS.

### Web UI

```
npm install
npm run build        # web/ -> data/ (required before `pio run -t uploadfs`)
npm run dev          # Vite dev server with HMR on :5173, API proxied to :8080
npm run mock         # mock REST API on :8080, also serves a built data/
```

For UI work run `npm run mock` and `npm run dev` side by side: Vite proxies the
six API routes to the mock, so the SPA behaves as it does on the device. To
check a production build instead, run `npm run build` and open the mock server
on :8080 directly, which serves `data/` statically.

## Architecture

### Firmware request/render loop

[src/main .cpp](src/main%20.cpp) (note the literal space in the filename) is the entry point. `setup()` mounts LittleFS, loads `Settings`, connects WiFi via `WiFiManager` (falls back to a `QlockThreeW32` AP for first-time config), starts NTP, mDNS, ArduinoOTA, RemoteDebug, and the `WebServer` on port 80. `loop()` re-syncs WiFi/NTP as needed, services the web server and OTA, and once per second rebuilds `matrix[16]` (the 16-row bitmask framebuffer) according to `mode` and pushes it to the LED driver. `mode` selects between normal time display, a seconds-counter debug view, an LED test pattern, an uptime/DCF-sync debug view, etc. (see the `STD_MODE_*` / `EXT_MODE_*` defines near the top of the file).

Settings changes made through the REST API mark `needsUpdateFromRtc = true` and schedule a deferred flash write (`timeToSaveToFLASH`, `WAIT_BEFORE_SETTINGS_WRITE` seconds later) rather than writing on every request, to limit flash wear.

### REST API (firmware and mock server share the same contract)

Both [src/main .cpp](src/main%20.cpp) and [server.js](server.js) implement the same endpoints, consumed by [web/src/lib/api.js](web/src/lib/api.js):
- `GET /currentState` — returns the full current settings object as JSON.
- `POST /display` — display mode.
- `POST /color` — hue/saturation/luminance.
- `POST /autoluminance` — toggle LDR-based auto brightness.
- `POST /configuration` — language, corner LED direction/color.
- `POST /timezone` — NTP server + manual DST/timezone rule fields.
- `GET /wifi` — connection status (`ssid`, `ip`, `rssi`, `mac`, `hostname`, `switching`, `error`).
- `GET /wifi/scan` — one poll of the async scan: `{scanning:true}` or `{scanning:false, networks:[…]}`.
- `POST /wifi` — `{ssid, password}`; answers immediately, the switch runs in `loop()`.
- `GET /ota/status` — `{firmwareVersion, fsVersion, sketchSize, freeSpace, error}`.
- `POST /ota/upload` — `multipart/form-data` with one file part; answers, then reboots.
- `GET /ota/check` — polls the channel's manifest, answers with the full status.
- `POST /ota/install` — starts the download in a task; answers immediately.
- `POST /ota/config` — `{channel, autoUpdate, checkInterval}`.

Changing the API means touching three places: the firmware handler, `server.js`, and `web/src/lib/api.js`.

**Errors are codes, not sentences.** The firmware answers with `{"error": "otaChecksum", "errorDetail": "HTTP 404"}` — a stable code plus an untranslated technical detail — and [web/src/lib/errors.js](web/src/lib/errors.js) turns it into text in the current language from the `err_*` keys in the locale files. It used to send German sentences, which was fine while the UI was German too. An unknown code is displayed as-is rather than swallowed, so a clock running newer firmware than its web UI still says something useful.

`/currentState` used to answer with JSONP (the response wrapped in the callback named by the query string). That was a jQuery-era workaround; it now returns plain JSON, since the SPA is served from the same origin and the firmware already calls `server.enableCORS()` for cross-origin dev access.

### Web UI architecture

[web/src/App.svelte](web/src/App.svelte) loads `/currentState` once on mount into a single `$state` object and passes it to the section of the selected tab — [Display](web/src/sections/Display.svelte), [Color](web/src/sections/Color.svelte), [Timezone](web/src/sections/Timezone.svelte); [Wifi](web/src/sections/Wifi.svelte) and [Ota](web/src/sections/Ota.svelte) fetch their own state instead, since neither is part of `/currentState`. Sections mutate that object through `bind:` and POST the affected endpoint on change; there is no save button, matching the old UI. `Timezone` always posts all fourteen fields at once because the firmware rebuilds both `TimeChangeRule`s from a single request.

Points worth knowing:
- The mode/language/corner values in the UI are the firmware's own numbers (`STD_MODE_*`/`EXT_MODE_*` in `main .cpp`, `LANGUAGE_*` in `Renderer.h`). Bindings keep them as numbers rather than the strings jQuery's `.val()` used to send.
- Colour changes are throttled ([web/src/lib/throttle.js](web/src/lib/throttle.js)); dragging the wheel would otherwise fire one POST per pointer move at the ESP32's single-threaded web server.
- With "Sommerzeit" off, the changeover fields of both rules are disabled but the standard rule's abbreviation and offset stay editable — the same rule the old `setDst()` implemented.
- Everything is bundled locally. The old page pulled Bootstrap, FontAwesome, jQuery and iro.js from CDNs, so it rendered broken on a LAN without internet access. The colour wheel is still iro.js, now bundled. The built page requests exactly three files, all from the clock: the JS bundle, the CSS and the favicon — no webfonts, no `url()` in the CSS, no `@import`. Keep it that way; the clock has to work on a network with no internet at all.
- Failed writes surface as a banner via [web/src/lib/status.svelte.js](web/src/lib/status.svelte.js), instead of being dropped as they were by the old `.done()`-only handlers.
- **The UI language is not a setting of its own**: it follows the clock's language. [web/src/lib/i18n.svelte.js](web/src/lib/i18n.svelte.js) maps the `LANGUAGE_*` number onto one of six locales in [web/src/lib/locales/](web/src/lib/locales/) — the four German dialects and Swiss German all share `de.js`. `App.svelte` drives it from a single `$effect` on `clock.language`, so no section has to know about it. Components read texts with `const t = $derived(dict())`; that `$derived` is what makes the page re-render on a change. `de.js` is the reference: same keys, same order, same array lengths in every locale — nothing falls back per key, a missing key renders as `undefined`.

### WiFi configuration (two separate paths)

Which one applies depends on whether the clock is on the network at all:

- **Connected** — the "WLAN" tab ([web/src/sections/Wifi.svelte](web/src/sections/Wifi.svelte)) shows status, scans, and switches networks through the `/wifi` endpoints above.
- **Not connected** — the SPA is unreachable, because it is served from LittleFS only once WiFi is up. `WiFiManager` takes over in `setup()` with its own AP (`QlockThreeW32`) and its own web server on 192.168.4.1. A tab can never cover this case, so the portal is instead restyled to match: `PORTAL_STYLE` in `main .cpp` is injected via `setCustomHeadElement()` and mirrors the SPA's colour tokens, including the dark-mode media query. When the SPA's palette changes, change that string too.

Switching networks is deliberately not a plain `WiFi.begin()`: a wrong password would leave the clock unreachable until someone power-cycles it and uses the AP portal. `POST /wifi` therefore only records the request and answers straight away (the response would never leave the old network otherwise), and `handleWifiSwitch()` runs a small state machine in `loop()`: try the new credentials, and on timeout fall back to the previous SSID/PSK — captured via `WiFi.psk()` before the attempt — leaving an explanatory message in `wifiLastError` for the UI. The normal reconnect block in `loop()` is skipped while a switch is in flight so the two don't fight over the connection.

The mock server implements the same endpoints, including a simulated scan delay and fallback: connect with the password `wrong` to exercise the failure path.

### Firmware update

There are **two images**, and they are updated independently: `firmware.bin` (the app, ~1.3 MB, into the inactive OTA slot) and `littlefs.bin` (this web UI, a full-partition 3.5 MB image). Changing the SPA and only flashing the firmware leaves the old UI in place.

Three ways in, in order of how they arrived:

- **`espota`** — ArduinoOTA is started in `setup()` and listens on **port 8266**, not the ESP32 default 3232, so `pio run -t upload --upload-port <ip>` needs `--upload-port` *and* a matching port setting. Needs PlatformIO on the same LAN. Its password comes from `OTA_PASSWORD` in `src/Secrets.h` (see "Credentials" below).
- **Browser upload** (the "Update" tab, [web/src/sections/Ota.svelte](web/src/sections/Ota.svelte)) — `POST /ota/upload`. The synchronous `WebServer` streams the body through the second handler registered on the route, so the image goes to flash as it arrives rather than through RAM. **The target partition is not chosen by the user**: ESP32 app images start with the magic byte `0xE9`, anything else is treated as the filesystem image (`U_FLASH` vs `U_SPIFFS`). The mock and the UI repeat that same test, the UI only to label the file before uploading.
- **GitHub manifest** — the clock polls the `manifest.json` of its channel (`otaFetchManifest()`), and `otaInstallTask` downloads and installs in a FreeRTOS task pinned to core 0, so the synchronous web server on core 1 keeps answering during the ~40 s a transfer takes. Each image is streamed straight into its partition while being hashed; the boot partition is only switched once the SHA-256 matches the manifest. Only the halves whose version differs are fetched.

Two rules that are easy to get wrong here:

- **Version comparison differs per channel.** `stable` gives clean semver, where "newer" is meaningful. `edge` gives `git describe` output (`2.0.0-7-gabc123`), which has no order — there the test is "differs from what is installed". Comparing by order on edge would mean the clock never offers anything again after a reverted commit. `otaShouldReplace()` holds both rules.
- **`OTA_IDLE` and friends are taken.** `esp_ota_ops.h` defines them, hence `OTA_STATE_*` here.

Automatic installs are off by default and only run between 02:00 and 05:00 local (`OTA_AUTO_HOUR_FROM`/`_TO`), so the clock never goes dark in the evening. `/ota/status` reports the running partition (`app0`/`app1`) — without it, an update to the same version is indistinguishable from nothing having happened.

Details that are easy to get wrong:

- `Update.end(true)` verifies the image before switching the boot partition, so a truncated upload is harmless — the clock keeps booting the old one. There is **no** rollback for an image that flashes fine but then crashes: the Arduino bootloader is built without `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`, so that case needs USB.
- Before writing a filesystem image the firmware calls `LittleFS.end()`, otherwise cached writes would be flushed over the freshly written image.
- The reboot happens in `loop()` via `otaRebootAt`, not in the handler, so the HTTP response makes it onto the wire. While that is pending `loop()` returns early — the deferred settings write must not run against an unmounted or just-overwritten filesystem.
- A filesystem update overwrites the whole partition. That used to take `qlockconf.json` with it, which is why the settings now live in NVS instead — see "Settings persistence". Nothing has to be backed up or restored around an update.
- The `/ota/*` endpoints have **no authentication**, deliberately: anyone on the LAN can flash the clock through the browser. A `server.authenticate()` at the top of both handlers is the whole fix if that changes. Note that espota *is* password-protected, so the two paths do not offer the same protection.

### Credentials

`src/Secrets.h` holds the values belonging to one particular clock and is gitignored; [src/Secrets.example.h](src/Secrets.example.h) is the committed template. `main .cpp` pulls it in behind `#if __has_include(...)`, so a fresh clone builds without it — with a `#warning` and no espota password rather than a compile error. Anything device-specific and secret belongs there, not in the source.

### Release workflow

[.github/workflows/release.yml](.github/workflows/release.yml) builds both images and publishes them as GitHub releases. Two channels, decided by the trigger:

| Channel | Trigger | Release | Manifest URL |
|---|---|---|---|
| `edge` | push to `main` | rolling pre-release under the fixed tag `edge` | `…/releases/download/edge/manifest.json` |
| `stable` | push of a tag `v*` | normal release under that tag | `…/releases/latest/download/manifest.json` |

Things that are load-bearing here:

- **The edge release must stay a pre-release.** Otherwise it takes over `/releases/latest`, and clocks on the stable channel would start following every commit.
- **`fetch-depth: 0`** on the checkout. A shallow clone has no tags, `scripts/version.py` would fall back to `Version.h` on every build, and every release would claim the same version.
- **The version is computed once, not twice.** `version.py` writes what it settled on to `.pio/version.txt`; the workflow reads that file for the manifest, the release title and `QLOCK_VERSION`. Working it out a second time in shell would eventually disagree with the value compiled into the image.
- **Step order**: firmware first (produces `version.txt`), then the web build with `QLOCK_VERSION` set, then `buildfs`. Reversing the first two makes `version.json` report whatever is in `package.json`.
- `src/Secrets.h` does not exist in CI, so published images have no espota password — which is what you want for a public image.
- [scripts/manifest.py](scripts/manifest.py) writes the manifest and can be run by hand to see what a release would look like.

Version comparison differs per channel, and the firmware client will have to respect that: `stable` gives clean semver (`2.1.0`) where "newer" is meaningful, `edge` gives `git describe` output (`2.0.0-4-gabc123`) where the only sensible test is "differs from what is installed".

### Versioning

`FIRMWARE_VERSION` lives in [src/Version.h](src/Version.h) behind an `#ifndef`, and [scripts/version.py](scripts/version.py) (a PlatformIO pre-build script) overrides it from `git describe --tags` when the checkout has a usable tag — the fallback in the header applies otherwise. The web UI's version comes from `package.json`; a small Vite plugin emits it as `data/version.json`, which the firmware reads at boot into `otaFsVersion`. Keep the two version numbers in step; the update manifest will compare them.

### Time synchronisation

The core's own SNTP client keeps the **system clock in UTC** (`configTime(0, 0, server)` in `startNtp()`); `setSyncProvider(syncFromSystemClock)` feeds TimeLib from it, and the `Timezone` rules convert to local time only where it is displayed. The provider returns 0 until the system clock passes `TIME_LOOKS_VALID`, so `timeStatus()` stays `timeNotSet` rather than the clock confidently showing 1970. `sntp_set_time_sync_notification_cb` sets `ntpLastSync`/`ntpSyncPending` from the SNTP task — hence `volatile` — and `loop()` acts on it.

This replaced **NtpClientLib**, which had to go: it declares a dependency on Time ~1.5, which the pinned 1.6.1 does not satisfy, so PlatformIO installed a *second* copy of the Time library. That old copy ships a deprecated `Time.h`, and every installed library folder lands on the include path of every source — so on a case-insensitive filesystem `#include <time.h>` inside the framework's `HTTPClient.cpp` resolved to `Time.h` and the build died on `strptime`/`mktime`/`struct tm`. Neither deleting the header (the duplicate needs it to compile itself) nor `lib_ignore` (keeps it out of the LDF but not off the include path) fixed it. Dropping the library removed the cause, and `HTTPClient` — needed for the update manifest — compiles.

Fixed along the way: `loop()` used to call `NTP.begin("pool.ntp.org", …)` with the server hardcoded, ignoring the one in `Settings`.

### Rendering pipeline (hardware-independent core)

`Renderer` ([src/Renderer.cpp](src/Renderer.cpp)) is pure logic with no hardware dependency: given hour/minute/language it sets word bits in the `word matrix[16]` framebuffer. Per-language word-to-bitmask macros live in `Woerter_<LANG>.h` (`Woerter_DE.h`, `Woerter_CH.h`, `Woerter_EN.h`, `Woerter_FR.h`, `Woerter_IT.h`, `Woerter_NL.h`, `Woerter_ES.h`; `Woerter_DE_MKF.h` exists but is currently unused/commented out) — each language has its own irregular grammar handled as a switch on `minutes / 5` plus special-casing (e.g. French/Italian/Spanish hour agreement, Swabian/Bavarian/Swiss `viertel`/`dreiviertel` variants). `Renderer::setCorners` sets the four corner-LED bits for the sub-5-minute remainder, in clockwise or counter-clockwise order. `Zahlen.h`/`Staben.h` hold digit/letter bit patterns used by the debug display modes (seconds, uptime, DCF-sync-age) rather than by the word renderer.

### LED output

`LedDriverWS2812FastLED` ([src/LedDriverWS2812FastLED.cpp](src/LedDriverWS2812FastLED.cpp)/[.h](src/LedDriverWS2812FastLED.h)) is the sole, concrete LED driver (an earlier `LedDriver` abstract base for swapping in other drivers was folded into this class — there is no longer an interface to implement). It drives a 114-pixel WS2812B strip via FastLED, wired serpentine with the corner LEDs fed separately (see the wiring diagram in the header's comment). It owns HSV color, brightness scaling, and corner-color/animation state, and converts the `matrix[16]` bitmap to physical pixel writes in `writeScreenBufferToMatrix`.

### Settings persistence

`Settings` ([src/Settings.h](src/Settings.h)/[src/Settings.cpp](src/Settings.cpp)) holds all user-configurable state (language, corner rendering, brightness, color, LDR use, mode, NTP server, and both standard/DST timezone rules) and (de)serializes it as JSON, plus exposes `getJSONSettings()` for the REST API response — a different shape, keyed the way the web UI wants it.

Hue and saturation are held in the **units the web UI uses** (0..359 and 0..100), not scaled to a byte. They used to be squeezed into 0..255 on the way in and expanded again on the way out, both with truncating integer division, so 195/90 came back as 194/89 — 358 of 360 hues and 95 of 101 saturations failed to survive a round trip. The conversion to the 8 bits FastLED wants now happens once, where the colour is handed to the driver, and is rounded. The LEDs are unchanged: they only ever had 256 hue steps.

`SETTINGS_SCHEMA` in `Settings.cpp` guards that. A stored record without a `Schema` field is schema 1 and gets converted on load; bump the constant and add a branch whenever a stored field changes meaning, rather than silently misreading old records.

It is stored in **NVS**, not in the filesystem, because the filesystem partition is overwritten wholesale by a web UI update. Details:

- The whole record goes in under one key as a JSON string, not as 21 individual keys. That shares `fillDocument()` with the file format, and NVS keys are capped at 15 characters — `RenderColorCorner` would not fit.
- `storeSettings()` compares against the stored string first and returns if nothing changed, so a no-op save costs no flash write. Together with the deferred write in `main .cpp` (`WAIT_BEFORE_SETTINGS_WRITE`) that keeps writes down to roughly one per settings change.
- `migrateLegacyFile()` takes over a `qlockconf.json` from a pre-2.1 firmware on the first boot and deletes it afterwards. Harmless to keep; it costs one `LittleFS.exists()` per boot.
- Clearing NVS therefore resets the clock to defaults — reflashing the filesystem no longer does.

### Light sensor (currently unused, kept for potential future wiring)

`LDR`/`BH1750` ([src/LDR.h](src/LDR.h)/[src/LDR.cpp](src/LDR.cpp)) supports an optional BH1750 light sensor for automatic brightness. It is intentionally not instantiated: the `LDR ldr;` declaration and the brightness-adjustment block in `main .cpp`'s `loop()` are commented out. The `automaticLum`/`UseLdr` setting still exists end-to-end (UI checkbox, `/autoluminance` REST endpoint, `Settings`), but currently has no effect — toggling it doesn't do anything until the LDR is wired back in.

### Debugging

`RemoteDebug` (telnet-style remote log console, `debugI`/`debugW`/`debugE`/`debugA` macros used throughout `main .cpp`) is the only debug facility in the project. Its companion GUI library, `RemoteDebugger` (variable watch/manipulation via a web console), was already inert before consolidation — the include and its init calls were commented out in `main .cpp` — and has been removed from `lib_deps` entirely, since it no longer compiles against the current ESP32 Arduino core (`std::byte` ambiguity in its vendored source). Its vendored web client, `RemoteDebugApp/`, was removed with it. A browser-based log console (e.g. the WebSerial library) was considered as a replacement but rejected: it requires migrating the whole web server from the synchronous `WebServer` used here to `ESPAsyncWebServer`/`AsyncTCP`, and current WebSerial releases are AGPL-3.0-licensed.

### Vendored/generated content (not project source)

- `.pio/`, `dist/`, `compile_commands.json`, `idedata.json` — PlatformIO build cache and IDE tooling metadata, not hand-maintained.

### Hardware consolidation

This project originally targeted several boards and LED drivers. It has been consolidated to a single target: Seeed XIAO ESP32-S3 + WS2812B. As part of that, the following were removed as dead/unreachable code:
- The `nodemcu-32s` and `esp32-c3-display` PlatformIO environments, and the 4 MB `partitions.csv`/`min_spiffs.csv` tables that only they referenced. (The current `partitions.csv` is unrelated — a new 16 MB table for the XIAO ESP32-S3 Plus, see above. The S3 environment never referenced a partition file before and relied on the board default.)
- `src/Configuration.h`, a large block of compile-time `#define` toggles for the original AVR/DCF77/multi-driver-era hardware (alarm, DCF77 receiver, alternate LED drivers, RTC chip selection, IR remote variants). It was already unreferenced by any active code path before removal.
- The `LedDriver` abstract base class, merged into `LedDriverWS2812FastLED` since it was the only implementation.
- The `RemoteDebugger` lib_dep and vendored `RemoteDebugApp/` web client (see "Debugging" above).

The BH1750 light sensor support (see above) was deliberately left in place, unlike the rest of the legacy hardware options.
