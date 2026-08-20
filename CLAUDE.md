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
API routes listed in `vite.config.js` to the mock, so the SPA behaves as it
does on the device — a new endpoint has to be added to that list. To
check a production build instead, run `npm run build` and open the mock server
on :8080 directly, which serves `data/` statically.

## Architecture

### Firmware modules

`main .cpp` had grown to 1951 lines and was split along the seams that were already there. It is 787 lines now and owns the clock itself — boot, network, time, and the render loop. Three modules sit next to it, each with a header of four or five functions and no other way in:

| File | Owns |
|---|---|
| [src/OtaUpdate.cpp](src/OtaUpdate.cpp) | `Ota::` — browser upload, release-channel polling, install, the deferred restart |
| [src/WebRoutes.cpp](src/WebRoutes.cpp) | `Web::` — every HTTP handler except `/ota/*`, `PORTAL_STYLE`, the network-switch state machine |
| [src/LightSensor.cpp](src/LightSensor.cpp) | the `LightSensor` interface, `Veml7700Sensor`, and `AmbientLight` with its sampling task |
| [src/LogBuffer.cpp](src/LogBuffer.cpp) | the in-memory log ring, the `DebugLog` tee and the ESP-IDF capture hook |
| [src/Expert.cpp](src/Expert.cpp) | the lock on `/log` and `/ota/*`: the password, the NVS flag, `Expert::guard()` |
| [src/languages/](src/languages/) | one file per language: its panel letters, its words, and how it says the time |
| [src/DisplayModes.h](src/DisplayModes.h) | the mode numbers, shared because `main .cpp` renders them and `WebRoutes.cpp` accepts them |

Two things make this a split rather than a rearrangement. **The modules take what they need as parameters instead of reading globals**: `Ota::poll(online, localHour)` gets told whether the network is up and what the local hour is, so nothing in it knows about `WiFiManager` or `Timezone`. And **`scheduleSettingsSave()` is the one named seam** both modules use to arm the deferred write, rather than each reaching into `timeToSaveToFLASH`.

What remains shared is declared `extern` in the module `.cpp`s: `server`, `settings`, `wifiConnected`. That is the honest description of the coupling — a handler needs the server it hangs on, and the settings are the state of the whole clock.

The move was verified as a pure relocation: the flash image grew by 48 bytes on the web-routes commit.

### Firmware request/render loop

[src/main .cpp](src/main%20.cpp) (note the literal space in the filename) is the entry point. `setup()` mounts LittleFS, loads `Settings`, connects WiFi via `WiFiManager` (falls back to a `QlockThreeW32` AP for first-time config), starts NTP, mDNS, ArduinoOTA, RemoteDebug, the light sensor, and the `WebServer` on port 80. `loop()` re-syncs WiFi/NTP as needed, services the web server and OTA, and once per second rebuilds `matrix[16]` (the 16-row bitmask framebuffer) according to `mode` and pushes it to the LED driver. `mode` selects between normal time display, the same with a WiFi status pixel, dark, a seconds counter and an LED test pattern (the `STD_MODE_*` / `EXT_MODE_*` defines are in [src/DisplayModes.h](src/DisplayModes.h)).

`loop()` **returns early while a restart is pending** — after an update or a rename — because a filesystem update unmounts LittleFS underneath it. The deferred settings write is the one exception and is flushed first; see "Settings persistence".

Once a minute the render path logs what is actually on the face, read back out of the framebuffer rather than out of the renderer's intentions:

```
Display 00:21 CEST (UTC 22:21) [EN] | ES IS DREIVI HALB NZWOLF | corners +1
```

That line looks broken and is not. `displayedWords()` walks the lit bits over the **German** panel letters in `PANEL[10][12]`, so an English face spells its words out of whatever letters happen to sit under them. Reading it needs the language tag — hence `[EN]`. It cost a while to establish that the renderer was right and the log was merely honest.

`mode` is stored, so it has to be read back — `isKnownMode()` guards both the boot path and `POST /display`, and anything else falls back to normal display. That matters on an update: 4 and 5 used to be an uptime counter and a DCF-sync-age display, left from the AVR and DCF77 days, and a clock updating from 2.0.1 can still have 4 in NVS. The numbers are deliberately not reused. Note also that persisting a mode is only half of it: `getMode()` was never read at first, so the web UI showed one mode selected while the face ran another.

Settings changes made through the REST API mark `needsUpdateFromRtc = true` and schedule a deferred flash write (`timeToSaveToFLASH`, `WAIT_BEFORE_SETTINGS_WRITE` seconds later) rather than writing on every request, to limit flash wear.

### REST API (firmware and mock server share the same contract)

[src/WebRoutes.cpp](src/WebRoutes.cpp) (and [src/OtaUpdate.cpp](src/OtaUpdate.cpp) for `/ota/*`) and [server.js](server.js) implement the same endpoints, consumed by [web/src/lib/api.js](web/src/lib/api.js):
- `GET /currentState` — returns the full current settings object as JSON.
- `POST /display` — display mode.
- `POST /color` — hue/saturation/luminance.
- `POST /autoluminance` — toggle automatic brightness.
- `GET /light` — `{sensor, present, available, lux, raw}` from the ambient light sensor, plus the brightness curve (`luxLow`, `brightLow`, `luxHigh`, `brightHigh`, `minRatio`) and the `brightness` it yields for the current reading. Not part of `/currentState`: the measurement is not a setting, and the colour tab polls it.
- `POST /light` — the four curve fields, `{want: 1..100}` ("at this light, this bright"), or `{reset: true}`. Answers with the same shape `GET` does, or with `calibrationTooClose` / `calibrationRange`.
- `POST /configuration` — language, corner LED direction/color.
- `POST /timezone` — NTP server + manual DST/timezone rule fields, plus `tzZone` (the picked IANA name, a label only — see "Timezone picker").
- `POST /hostname` — `{hostname}`; renames the clock, answers with the name actually stored.
- `GET /wifi` — connection status (`ssid`, `ip`, `rssi`, `mac`, `hostname`, `switching`, `error`).
- `GET /wifi/scan` — one poll of the async scan: `{scanning:true}` or `{scanning:false, networks:[…]}`.
- `GET /panel` — the face as it is right now: `rows` (the panel of the language that is running), `on` (a second grid of `#`/`.` read off the frame buffer), `corners` in reading order, plus `code`, `name`, `uiLocale`, `mode` and the sentence. Polled by the colour tab.
- `GET /log?since=<seq>` — the log ring from that sequence number on, plus `oldest`, `more`, and the state block (`uptime`, `heap`, `heapMin`, `heapBlock`, `reset`). Not part of `/currentState`: none of it is a setting, and the debug tab polls it. **Behind the lock** — see "Expert mode".
- `GET /expert` — `{enrolled, unlocked, grace, lockedOut}`. No secret in it; the shell needs it before it can decide which tabs exist.
- `POST /expert` — `{password}` (sets one on a clock with none, otherwise checks it), `{off: true}`, or `{reset: true}`. Answers with the same shape `GET` does.
- `POST /wifi` — `{ssid, password}`; answers immediately, the switch runs in `loop()`.
- `GET /ota/status` — `{firmwareVersion, fsVersion, sketchSize, freeSpace, error}`.
- `POST /ota/upload` — `multipart/form-data` with one file part; answers, then reboots.
- `GET /ota/check` — polls the channel's manifest, answers with the full status.
- `POST /ota/install` — starts the download in a task; answers immediately.
- `POST /ota/config` — `{channel, autoUpdate, checkInterval}`.

Changing the API means touching **four** places: the firmware handler, `server.js`, `web/src/lib/api.js`, and the route list in [vite.config.js](vite.config.js) — a new endpoint that is not in `API_ROUTES` is not proxied, so it works on the device and 404s in `npm run dev`.

**Errors are codes, not sentences.** The firmware answers with `{"error": "otaChecksum", "errorDetail": "HTTP 404"}` — a stable code plus an untranslated technical detail — and [web/src/lib/errors.js](web/src/lib/errors.js) turns it into text in the current language from the `err_*` keys in the locale files. It used to send German sentences, which was fine while the UI was German too. An unknown code is displayed as-is rather than swallowed, so a clock running newer firmware than its web UI still says something useful.

`/currentState` used to answer with JSONP (the response wrapped in the callback named by the query string). That was a jQuery-era workaround; it now returns plain JSON, since the SPA is served from the same origin and the firmware already calls `server.enableCORS()` for cross-origin dev access.

### Web UI architecture

[web/src/App.svelte](web/src/App.svelte) loads `/currentState` once on mount into a single `$state` object and passes it to the section of the selected tab — [Display](web/src/sections/Display.svelte), [Color](web/src/sections/Color.svelte), [Timezone](web/src/sections/Timezone.svelte); [Wifi](web/src/sections/Wifi.svelte), [Ota](web/src/sections/Ota.svelte) and [Debug](web/src/sections/Debug.svelte) fetch their own state instead, since none of it is part of `/currentState`. [Expert](web/src/sections/Expert.svelte) is not a tab at all — see "Expert mode". `Color` does three: the colour itself comes from `/currentState`, the sensor reading beside it from `/light`, and the face it previews from `/panel`. Sections mutate that object through `bind:` and POST the affected endpoint on change; there is no save button, matching the old UI. `Timezone` always posts all fourteen fields at once because the firmware rebuilds both `TimeChangeRule`s from a single request.

### Timezone picker

The timezone tab offers a region/place picker above the two rule editors. It is a shortcut, not a layer: it writes the same fourteen fields, which stay editable underneath, and editing one by hand clears the stored zone name — otherwise the label and the rules would disagree and the label would be the one lying.

The data is the last line of every compiled IANA zone file, the **POSIX TZ string** (`Europe/Berlin` → `CET-1CEST,M3.5.0,M10.5.0/3`), which happens to be exactly this project's storage model: two changeover rules of month, week, weekday, hour and offset. [scripts/zones.py](scripts/zones.py) extracts it into [web/public/zones.json](web/public/zones.json) and [web/src/lib/posixtz.js](web/src/lib/posixtz.js) turns it back into fields. Load-bearing details:

- **The list is generated, committed and shipped in the filesystem image — never fetched at runtime.** At 16 KB in a 3.5 MB partition there is no case where it is absent, which removes the "fetch if missing" branch and its failure modes; the clock must work on a network with no internet at all; and the FS half of the OTA update already keeps it current, tzdata releasing 2–4 times a year against a faster release cadence. The `tzdata` version travels in the file and is shown in the tab.
- Regenerate with `pip install tzdata` then `python scripts/zones.py`. The source is the PyPI `tzdata` package rather than the system database, so Windows and CI produce the same file, and rather than a third-party list such as `nayarsystems/posix_tz_db`, which was eleven months stale when this was written. The output carries no build timestamp, so regenerating without a tzdata change leaves the file byte-identical and a diff only ever shows a rule that moved.
- **The list is deliberately wider than `zone1970.tab`**, the usual choice: recent tzdata releases turned Amsterdam, Oslo, Stockholm, Copenhagen and Luxembourg into links to `Europe/Berlin` and dropped them from that table. Filtering by continent prefix instead keeps them — 490 entries rather than 312. A clock that speaks Dutch without offering Amsterdam looks broken.
- **The picked name has to be stored**, hence `TzZone` in `Settings`: 490 zones share fewer than a hundred distinct rule pairs, so Berlin, Paris and Rome are indistinguishable once stored and the selection cannot be recovered from the fields. It is a label with no effect on timekeeping; empty means the rules were set by hand.
- Ten zones cannot be expressed exactly. Cairo and Santiago change at hour "24", Jerusalem at "26", Gaza at "50", Nuuk at "-1" — all meaning midnight rolled into a neighbouring day — and Chatham changes at quarter past the hour. The parser clamps the hour to 0..23, which moves those changeovers by a few hours once a year. `scripts/zones.py` prints the list on every run; a new entry means a zone has moved outside what two rules can hold.
- The first rule in a POSIX string *starts* daylight saving and the second *ends* it, so they map onto `tzDs*` and `tz*` respectively — not in reading order. `Europe/Berlin` parses to exactly the defaults written by hand in `Settings.cpp`, which is the cheapest available check that the mapping is right.

Points worth knowing:
- The mode/language/corner values in the UI are the firmware's own numbers (`STD_MODE_*`/`EXT_MODE_*` in [src/DisplayModes.h](src/DisplayModes.h), `LANGUAGE_*` in `Renderer.h` — the numbers are stored in NVS and must keep their values). `MODE_VALUES` in `Display.svelte` has to agree with that header. Bindings keep them as numbers rather than the strings jQuery's `.val()` used to send.
- Colour changes are throttled ([web/src/lib/throttle.js](web/src/lib/throttle.js)); dragging the wheel would otherwise fire one POST per pointer move at the ESP32's single-threaded web server.
- With "Sommerzeit" off, the changeover fields of both rules are disabled but the standard rule's abbreviation and offset stay editable — the same rule the old `setDst()` implemented.
- Everything is bundled locally. The old page pulled Bootstrap, FontAwesome, jQuery and iro.js from CDNs, so it rendered broken on a LAN without internet access. The colour wheel is still iro.js, now bundled. The built page requests exactly three files on load, all from the clock: the JS bundle, the CSS and the favicon — no webfonts, no `url()` in the CSS, no `@import`. Opening the timezone tab adds a fourth, `/zones.json`, also from the clock. Keep it that way; the clock has to work on a network with no internet at all.
- **Never name a local `$state` in a component that takes the `state` prop.** `let { state } = $props()` makes `$state(...)` parse as a store subscription on that local binding rather than as the rune (`store_rune_conflict`). It is a warning, not an error, so the build succeeds and the variable silently never triggers a re-render. [Timezone.svelte](web/src/sections/Timezone.svelte) therefore destructures as `let { state: clock } = $props()`.
- Failed writes surface as a banner via [web/src/lib/status.svelte.js](web/src/lib/status.svelte.js), instead of being dropped as they were by the old `.done()`-only handlers.
- **The UI language is not a setting of its own**: it follows the clock's language. [web/src/lib/i18n.svelte.js](web/src/lib/i18n.svelte.js) maps the `LANGUAGE_*` number onto one of six locales in [web/src/lib/locales/](web/src/lib/locales/) — the four German dialects and Swiss German all share `de.js`. `App.svelte` drives it from a single `$effect` on `clock.language`, so no section has to know about it. Components read texts with `const t = $derived(dict())`; that `$derived` is what makes the page re-render on a change. `de.js` is the reference: same keys, same order, same array lengths in every locale — nothing falls back per key, a missing key renders as `undefined`. **Check for duplicate keys, not just for equal counts.** Adding an `err_*` code that already existed silently shadowed the original in all six files at once; a count comparison across locales saw six consistent numbers and reported nothing, because the mistake had been made six times identically. In an object literal the last entry simply wins, with no warning from Vite or Svelte.

### The clock's name

`Hostname` in `Settings` (default `QlockThreeW32`) is the one name the clock answers to, and it exists because a second clock on the same network would otherwise fight the first over the same mDNS record. It feeds six places: `<name>.local` over mDNS, the DHCP name the router lists, the setup access point, the RemoteDebug host, the espota target, and the heading of the web UI. All six used to be the same string literal.

- **`POST /hostname` restarts the clock**, and the button says so. Only mDNS could be renamed while it runs; the DHCP name, the OTA name and the AP name are read as the interface comes up, so renaming without a restart left the clock answering to two different names depending on who asked. The handler writes to NVS **immediately** rather than through the deferred write, which would still be pending when the restart happens and would lose the new name. It answers before scheduling the restart, and reports `restarting` so the UI knows to wait for the clock to come back.
- **The name is reduced to a DNS label in the firmware**, not just in the browser — `sanitizeHostname()` keeps `[A-Za-z0-9-]`, trims hyphens off both ends and caps it at 32 characters. The endpoint is reachable without the UI, and a name with a dot or a space in it would produce a record nobody can reach. The handler answers with what it stored, so the field shows the trimmed name rather than what was typed.
- It rides in **both** `/currentState` and `/wifi`: the WLAN tab is where it is edited, but the shell needs it for the heading before that tab is ever opened.
- [web/src/lib/appname.svelte.js](web/src/lib/appname.svelte.js) keeps the heading, `document.title` and the `apple-mobile-web-app-title` meta in step. That last one is what iOS puts under the home screen icon, and Safari reads it from the live DOM when someone adds the page — so setting it from JavaScript is enough, and each clock gets its own label.

### Home screen icon

[scripts/icon.py](scripts/icon.py) draws four PNGs into `web/public/`: the clock's own face reading ES IST HALB ACHT, with the lit cells taken from the firmware's bit masks so the icon shows a state the clock can actually be in. Committed, so neither the web build nor CI needs Python or the font. Everything is rendered ten times oversize and reduced — asking the rasteriser for 14 px type directly gives mush. The squares are deliberately **not** rounded: both platforms apply their own mask, and rounding twice looks wrong.

| File | For |
|---|---|
| `apple-touch-icon.png` 180 | iOS home screen, the only size current iPhones use |
| `icon-192.png` | Android home screen |
| `icon-512.png` | Android splash screen and listings |
| `icon-512-maskable.png` | declared `purpose: maskable`, drawn with a 20 % margin |

Two platform rules drive that table. iOS masks with a squircle that bites ~22 % out of each corner, which the 10.5 % margin clears. Android may crop a **maskable** icon to any shape inside the middle 80 %, which is stricter — hence the separate render. Declaring the ordinary icon as maskable would crop the letters; shipping no maskable icon at all makes some launchers put the black square on a white plate.

**Android ignores `apple-touch-icon` entirely** and reads a web app manifest instead. `/manifest.webmanifest` is **built by the firmware** (`sendManifest()`), not shipped as a file, because it carries the app name and that is per clock — see "The clock's name". Building it in the browser and handing Chrome a `blob:` URL also works, but rests on behaviour that has changed between versions; a real response from the clock does not. The icons it points at are static files from the image. `server.js` and the Vite proxy route both mirror this, so the manifest is right in development too.

### WiFi configuration (two separate paths)

Which one applies depends on whether the clock is on the network at all:

- **Connected** — the "WLAN" tab ([web/src/sections/Wifi.svelte](web/src/sections/Wifi.svelte)) shows status, scans, and switches networks through the `/wifi` endpoints above. The scan result is reduced to one entry per SSID, strongest first: a mesh or dual-band router answers once per radio, and the list is keyed by SSID — Svelte throws `each_key_duplicate` on a repeat, in production too, which took the whole list down rather than one row. `server.js` returns a duplicated SSID and a nameless one so the case shows up in development.
- **Not connected** — the SPA is unreachable, because it is served from LittleFS only once WiFi is up. `WiFiManager` takes over in `setup()` with its own AP (`QlockThreeW32`) and its own web server on 192.168.4.1. A tab can never cover this case, so the portal is instead restyled to match: `PORTAL_STYLE` in `WebRoutes.cpp` (reached through `Web::portalStyle()`, since `setup()` does the injecting) is passed to `setCustomHeadElement()` and mirrors the SPA's colour tokens, including the dark-mode media query. When the SPA's palette changes, change that string too.

Switching networks is deliberately not a plain `WiFi.begin()`: a wrong password would leave the clock unreachable until someone power-cycles it and uses the AP portal. `POST /wifi` therefore only records the request and answers straight away (the response would never leave the old network otherwise), and a small state machine driven from `loop()` by `Web::poll()` does the rest: try the new credentials, and on timeout fall back to the previous SSID/PSK — captured via `WiFi.psk()` before the attempt — leaving an explanatory message in `wifiLastError` for the UI. The normal reconnect block in `loop()` is skipped while a switch is in flight so the two don't fight over the connection.

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

- **The firmware is installed first and the filesystem second**, because it is the safer half to fail on: `Update.begin(U_SPIFFS)` erases the partition before the download starts, so an interrupted filesystem install leaves the clock with **no web UI** until someone reaches it over USB. The REST API survives, being in the firmware, which is how that state is recognisable. (A caveat on that: `LittleFS.end()` runs before the write, so a filesystem install that fails *before its reboot* only leaves the volume unmounted — a restart brings it back. Do not reach for USB without trying that first.)
- **"Could Not Activate The Firmware" is intermittent, and not understood.** `esp_ota_set_boot_partition()` fails inside `Update.end(true)` after both images have arrived with matching digests. It was first blamed on `Update` being a singleton that will not activate in a second session — installing the filesystem first and the firmware second failed that way, while a firmware-only install went through. **That explanation is wrong**: the same failure then happened with the firmware going first, and the very next attempt, same firmware and same images, succeeded. Intermittent points at resources rather than order, so `otaDownloadImage()` now calls `client.stop()` before verifying — `http.end()` closes the connection but leaves tens of kilobytes of mbedTLS context on a client that would otherwise live until the function returns, and activation has to map and hash a whole partition. Free heap is logged either side of it. If it recurs, that number is the first thing to look at, then the 10 KB stack of `otaInstall`.
- `Update.end(true)` verifies the image before switching the boot partition, so a truncated upload is harmless — the clock keeps booting the old one. There is **no** rollback for an image that flashes fine but then crashes: the Arduino bootloader is built without `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`, so that case needs USB.
- **`uploadfs` over USB fails with "chip stopped responding".** PlatformIO sends the image compressed, and a fresh LittleFS is almost empty — 3538944 bytes become 170865. The last blocks expand to megabytes of empty space on the chip, which takes longer than esptool's reply timeout. Write it uncompressed instead: `python esptool.py --chip esp32s3 --port COMx --baud 460800 write_flash --no-compress 0xc90000 .pio/build/seeed_xiao_esp32s3/littlefs.bin`.
- Before writing a filesystem image the firmware calls `LittleFS.end()`, otherwise cached writes would be flushed over the freshly written image.
- **`Client::setTimeout()` is milliseconds, and that is not obvious.** `client.setTimeout(20)` — written in the belief it meant 20 seconds, because `NetworkServer::setTimeout()` in the same framework *does* take seconds — gave the download 20 ms of patience and failed every release-channel install with `otaSize`, a short read that looked like a truncated image. `NetworkClientSecure` derives from `NetworkClient` and neither overrides the method, so the `Stream` contract applies: milliseconds. Both timeouts are now `OTA_STREAM_TIMEOUT_MS`.
- The download loop treats **a zero-length read as "nothing yet", not as the end**. It only gives up once `http.connected()` goes false or nothing has arrived for `OTA_STREAM_TIMEOUT_MS`, and reports `otaConnectionLost` — distinguishable in the UI from a checksum failure, which means something quite different.
- The reboot happens in `loop()` via `Ota::restartDue()`, not in the handler, so the HTTP response makes it onto the wire. While that is pending `loop()` returns early — the deferred settings write must not run against an unmounted or just-overwritten filesystem. The rename in the WLAN tab uses the same `Ota::scheduleRestart()`; it is not an update, but it needs exactly the same delay.
- A filesystem update overwrites the whole partition. That used to take `qlockconf.json` with it, which is why the settings now live in NVS instead — see "Settings persistence". Nothing has to be backed up or restored around an update.
- The `/ota/*` endpoints are **behind expert mode** — see that section. They were deliberately open until the debug tab arrived and made the log readable over HTTP too; a lock in front of one and not the other made no sense. `handleOtaUploadData()` is the one that needs care: it streams the body straight into flash and cannot send a response, so it refuses by setting `otaError`, which both stops the write and lets `handleOtaUploadDone()` answer. Guarding only the done handler would have erased the filesystem partition before refusing. espota is a separate path with its own password from `src/Secrets.h`, and expert mode does not cover it.

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
- **`version.py` matches `v[0-9]*` and nothing else.** The edge release lives under a fixed tag `edge`, which the workflow deletes and recreates on the commit of every build — so a plain `git describe --tags` answers `edge`, which is not a version, and the build falls back to `Version.h`. That is self-inflicting: once one edge build has run, every later one is stuck reporting the same number. Cost half an hour to find, because the two builds either side of it looked identical.
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

`TimeChangeRule` is `{abbrev, week, dow, month, hour, offset}`, and **it must not be filled positionally**. Six call sites used to build it from the settings as `{…, week, month, dow, …}`, putting the month number into the day-of-week field: with the defaults, "last Sunday in October" was evaluated as weekday 10 of January, and the changeover date was meaningless. Both the boot path and `/timezone` were affected; only the `CET`/`CEST` constants at the top of `main .cpp` were right, because they use the library's named enums. `tzRuleFrom()` and `applyTimezoneFromSettings()` now state the order once and are the only way the rules get built.

### Rendering pipeline (hardware-independent core)

`Renderer` ([src/Renderer.cpp](src/Renderer.cpp)) is pure logic with no hardware dependency: given hour/minute/language it sets word bits in the `word matrix[16]` framebuffer. It is 140 lines and does almost nothing itself — it looks the language up in [src/languages/](src/languages/) and lets it render. `Renderer::setCorners` sets the four corner-LED bits for the sub-5-minute remainder, in clockwise or counter-clockwise order; those live in the **low five bits of rows 0..3**, below the eleven columns of letters, which is why the frame buffer is `word` and not `uint16_t` worth of panel. `Zahlen.h` holds the digit bit patterns the seconds display draws with, rather than anything the word renderer uses. `Staben.h` holds letter patterns and a heart, is referenced by nothing, and is **kept on purpose** for a future use — it is not included anywhere, so it costs nothing.

### Languages

One file per language in [src/languages/](src/languages/), plus [Language.h](src/languages/Language.h) for the shape of one and [Languages.cpp](src/languages/Languages.cpp) for the table that maps a stored language number onto it. A new language is a new file and one line in that table.

**A word is a place and the text it spells**, and everything else follows from that:

```c
{ 0, 7, "FÜNF" }      // row 0, from column 7
```

The bit mask is arithmetic (column *c* is bit 15−*c*), so no macros are needed. The text is present, so the log names what is lit in the language that is lit. The geometry is present, so the browser can draw the real panel and a script can hand OpenSCAD something to cut. And because the panel letters and the word's own text are both here, `Languages::selfCheck()` confirms at every boot that the letters under a word really spell it — which is the mistake one makes when adding a panel, and which otherwise shows up as a plausible-looking wrong face.

- **The grammar stays imperative, deliberately.** Swabian says "viertel sechs" and counts the hour up where standard German says "viertel nach fünf"; French has "moins le quart"; Italian and Spanish inflect the hour ("è l'una" against "sono le due"). Written as a rule table that becomes a small language of its own, harder to read than the switch it replaces and touched once a year. Each language keeps a `render()`; it just lives next to its panel now.
- **German is four entries over one panel** — standard, Swabian, Bavarian, Saxon — differing in four of the twelve five-minute steps. `Language_DE.cpp` states the differences as a table in its header comment.
- **Words with a gap are two entries.** German "ES IST" is ES at column 0 and IST at column 3 with a dark K between, which is also why the log reads "ES IST" and not "ESIST".
- `name` is in the language itself ("Deutsch", "Français"), which is how language pickers are conventionally written and which saves translating ten names into six locales. `uiLocale` names which web locale to speak, so a language added here needs no edit on the web side.
- **Registration is a table line, not a self-registering static object.** The order in which static constructors run across translation units is not defined in C++, and when that goes wrong it goes wrong before there is any way to see it.
- `extern const` on the definitions, not only on the declarations: a `const` object at namespace scope has internal linkage in C++ unless it is spelled out, and the table will not find it. That is one link error, and it is the first one this refactor produced.

**The English panel spells FIFE.** Row 2 reads `TWENTYFIFEX`, and the mask for "five past" has always lit columns 6..9 — F, I, F, E. Either the panel drawing is wrong or the mask is one column out; it cannot be told from the source, and `Language_EN.cpp` states what the panel says rather than what it ought to say. Nobody noticed because the letters had never been machine-readable before. **Check a physical English panel before changing it**: if the real panel says FIVE, the drawing is what needs fixing, and the mask is already right.

#### How the switch was retired safely

1300 lines of mechanical rewriting is the kind that breaks something quietly and is noticed in October when Swabian says the wrong thing at a quarter past. There is **no host C++ compiler on the development machine** (checked: no gcc, clang, MSVC or MinGW; the cpptools extension ships only clang-format and clang-tidy), so the offline golden-master run that would be the obvious first step was not available.

The on-device comparison that replaced it turned out better: both implementations sat in the same image, on the same chip, so nothing was lost in a shim. `RenderCheck.cpp`, built only with `-DRENDER_CHECK`, rendered every language at every minute of the day through both paths and reported where they disagreed — 14,400 frames, and the answer was **0 differing frames and 0 panel problems** before the old switch, `Woerter_*.h` and the harness itself were deleted. If a language is ever rewritten again, that file is worth resurrecting from git rather than reinventing.

`cleanWordsForAlarmSettingMode()` went with it: it was per-language, and nothing had called it since the alarm was removed.

### LED output

`LedDriverWS2812FastLED` ([src/LedDriverWS2812FastLED.cpp](src/LedDriverWS2812FastLED.cpp)/[.h](src/LedDriverWS2812FastLED.h)) is the sole, concrete LED driver (an earlier `LedDriver` abstract base for swapping in other drivers was folded into this class — there is no longer an interface to implement). It drives a 114-pixel WS2812B strip via FastLED, wired serpentine with the corner LEDs fed separately. **The header's wiring comment is not a source of truth for the corners** — it named an order that is not the one the clock lights, which is why the mapping lives at `Renderer::setCorners` and the four `CORNER_*` defines in the driver restate it in physical pixel numbers. It owns HSV color, brightness scaling, and corner-color/animation state, and converts the `matrix[16]` bitmap to physical pixel writes in `writeScreenBufferToMatrix`.

**Brightness goes through gamma 2.2** (`_gammaScale()`, computed once in `setBrightness()` into `_brightnessScaled`, not per pixel). Perception follows roughly a power law, so driving the LEDs proportionally to the slider does not feel proportional — everything interesting used to happen in the bottom third, and half way up looked far brighter than half. The curve now gives 25 %→12, 50 %→55, 75 %→135. Two details are load-bearing: **the floor of 1**, because 1–3 % otherwise rounds to zero and the clock goes dark while the UI says it is on, and **the corner LEDs use `_brightnessScaled` too** — they were on the raw percentage and drifted visibly brighter than the letters at low settings.

**The coloured corners are the one place that writes corner pixels directly.** With `RenderColorCorner` on, the corners are drawn from `_minute`/`_second` instead of from the frame buffer, because each carries a different hue and a frame buffer row is one bit — the newest corner cycles through the hues once a minute, the ones before it sit at `CORNER_SETTLED_HUE`. It has to count the same minutes `Renderer::setCorners` does, and for a long time it did not: a remainder of 0 lit one corner, so every corner came on a minute early and the fourth never went out, and `_cw` was read in exactly one branch whose `else` was empty, so counter-clockwise ran clockwise. Both came from it being a `switch` with the four states written out by hand; it is a table and a loop now. The mode is off by default, which is why nobody had seen it.

### Settings persistence

`Settings` ([src/Settings.h](src/Settings.h)/[src/Settings.cpp](src/Settings.cpp)) holds all user-configurable state (language, corner rendering, brightness, color, LDR use, mode, NTP server, and both standard/DST timezone rules) and (de)serializes it as JSON, plus exposes `getJSONSettings()` for the REST API response — a different shape, keyed the way the web UI wants it.

Hue and saturation are held in the **units the web UI uses** (0..359 and 0..100), not scaled to a byte. They used to be squeezed into 0..255 on the way in and expanded again on the way out, both with truncating integer division, so 195/90 came back as 194/89 — 358 of 360 hues and 95 of 101 saturations failed to survive a round trip. The conversion to the 8 bits FastLED wants now happens once, where the colour is handed to the driver, and is rounded. The LEDs are unchanged: they only ever had 256 hue steps.

**Everything the web UI shows is persisted, and that is worth keeping true.** `fillDocument()` and `loadSettings()` must name the same fields, and both must cover every member the UI can see through `getJSONSettings()`. `Mode` was in `getJSONSettings()` and in the `/display` handler but in neither of the other two, so the display mode — including "off (dark)" — was the one thing on screen that no restart survived. When adding a member, add it in all three places or it is decoration.

The deferred write has one more hole worth knowing: `loop()` returns early while a restart is pending, so anything changed in the twenty seconds before an update or a rename used to be dropped. It is flushed before `ESP.restart()` now. That was the right call back when the settings lived in `qlockconf.json` and writing could have landed in a just-flashed image; in NVS it is safe.

`SETTINGS_SCHEMA` in `Settings.cpp` guards that. A stored record without a `Schema` field is schema 1 and gets converted on load; bump the constant and add a branch whenever a stored field changes meaning, rather than silently misreading old records. Note the "changes meaning" — *adding* a field does not qualify. `TzZone` arrived without a bump, because an older record simply lacks it and reads as empty, which is the honest answer: the rules in such a record may have been set by hand, and guessing a city that does not match them would be worse than naming none.

It is stored in **NVS**, not in the filesystem, because the filesystem partition is overwritten wholesale by a web UI update. Details:

- The whole record goes in under one key as a JSON string, not as 21 individual keys. That shares `fillDocument()` with the file format, and NVS keys are capped at 15 characters — `RenderColorCorner` would not fit.
- `storeSettings()` compares against the stored string first and returns if nothing changed, so a no-op save costs no flash write. Together with the deferred write in `main .cpp` (`WAIT_BEFORE_SETTINGS_WRITE`) that keeps writes down to roughly one per settings change.
- `migrateLegacyFile()` takes over a `qlockconf.json` from a pre-2.1 firmware on the first boot and deletes it afterwards. Harmless to keep; it costs one `LittleFS.exists()` per boot.
- Clearing NVS therefore resets the clock to defaults — reflashing the filesystem no longer does.

### Light sensor

[src/LightSensor.h](src/LightSensor.h)/[.cpp](src/LightSensor.cpp) replaced the old `LDR`/BH1750 pair, which had been commented out rather than used for a long time. Three parts:

- **`LightSensor`** — an interface of one meaningful method, `readLux()`. A TSL2591 or OPT3001 would be another class beside `Veml7700Sensor` and one changed line in `AmbientLight::begin()`.
- **`Veml7700Sensor`** — Vishay VEML7700 on I²C at the fixed address 0x10, read through the library's `VEML_LUX_AUTO` mode. **The BH1750 was dropped because it resolves 1 lx** and behind a dark front panel a lit living room arrives as a handful of lux — the interesting range is fractions of one. The `Adafruit_VEML7700` object is held as a `void *` so the header does not drag the library into every translation unit.
- **`AmbientLight`** — owns the sensor and samples it **in a FreeRTOS task pinned to core 0**, every 2 s, smoothed with an EMA over 30 s (`dt/(tau+dt)`, seeded from the first reading so it does not crawl up from zero).

**The task is not optional.** Auto-ranging walks gain and integration time and waits for a fresh measurement at each step, which can block well over a second, and the web server here is synchronous — a blocked `loop()` is a clock that stops answering. `smoothed`/`lastRaw`/`sampleCount` are `volatile` 32-bit values written on core 0 and read on core 1; a torn read is not possible for those, so they carry no lock.

`I2C_SDA_PIN` / `I2C_SCL_PIN` default to 5/6 (D4/D5 on the XIAO) and are overridable, because one firmware serves every build of the clock and the sensor is not in the same place in all of them.

`present()` distinguishes "no sensor on this clock" from "sensor found, no reading yet" (`available()`), and both the UI and the boot path depend on that distinction:

- The colour tab **hides the whole "Automatik" section** when `present` is false, rather than showing a switch that does nothing.
- `setup()` **clears `UseLdr` when no sensor answers**, writing straight to NVS rather than through the deferred write (which is armed later in `setup()` and would drop it). Without that, a clock whose sensor is removed keeps a stored "on" for a switch nobody can see, and therefore nobody can turn off.

### Automatic brightness

`brightnessForLux()` in `LightSensor.cpp` maps a reading onto a display brightness, from two calibration points held in `Settings` (`AutoLuxLow`/`AutoBrightLow`, `AutoLuxHigh`/`AutoBrightHigh`). It is a free function with no state, so the curve can be reasoned about — and compared against a reference implementation — without a sensor present.

- **Brightness is linear in log(lux), not in lux.** Perception is roughly logarithmic and the range to cover spans several decades: a dark bedroom and a sunlit room differ by a factor of thousands, which no linear mapping survives.
- **The result is clamped to the two calibrated ends, never extrapolated**, so a torch or a sunbeam on the sensor cannot drive the display past what the user asked for.
- **The floor is 1 %, never 0.** Zero is the display switching itself off, which is a mode chosen in the display tab, not something the light sensor gets to decide.
- Two guards keep bad input out of a division: `LUX_FLOOR` (log(0) has no answer, and the VEML7700 reports a plain 0 in a closed room) and `CALIBRATION_MIN_RATIO`, the factor by which the two points must differ. Points too close together describe no slope and would swing the brightness across its whole range on sensor noise. `POST /light` rejects such a pair with `calibrationTooClose`; `brightnessForLux()` clamps as well, because the endpoint is reachable without the UI and an old record could hold anything.
- The defaults (1 lx → 20 %, 200 lx → 100 %) are deliberately cautious rather than good: they assume a sensor in the open. **Behind a front panel both readings shrink by the same factor, which in log space only shifts the line sideways** — so an uncalibrated clock still dims in the right direction, just not by the right amount.

`brightnessToApply()` in `main .cpp` decides what actually reaches the driver each tick:

- With the automatic **off**, the setting is applied immediately — someone dragging the slider wants to see the effect while dragging, so no easing there.
- With it **on**, the computed value is approached by an eighth of the remaining distance per second, about twenty seconds for a full swing. The reading is already smoothed over 30 s, so this is not about noise: it is about the step when a lamp is switched on, which is a genuine jump the eye would otherwise catch.
- **The manual setting is never overwritten.** Switching the automatic off has to give back the brightness the user chose, and the calibration needs it as the "how bright I want it here" half of a point.

**The brightness slider has two meanings, and that is the point.** With the automatic off it is the brightness. With it on it is *"at this light, I want this much"* — `POST /light {want}` shifts the whole curve to satisfy it. Disabling the slider instead was the first attempt and it was wrong: nudging the brightness is the **only** signal a user ever gives about whether the automatic got it right, so locking the slider locks out the one input step 4 has to learn from. The endpoint is deliberately shaped as that learning primitive — it takes a wanted level at the current light, and a learning version keeps the samples and fits a line through them instead of applying each one immediately.

How the shift works, and why it is not just an addition:

- Normally both ends move by the same amount, which keeps the slope. The two calibration points say how hard the clock reacts to a change in light; this says at what level. Two questions, two controls.
- **A pure translation cannot always express the request.** The default curve already reaches 100 % at its bright end, so "brighter here" has nowhere to go — the first version clamped the shift to zero and the slider silently did nothing, which is the exact fault this control exists to fix. Now the overflowing end is pinned and the other is **solved** so the curve passes through the requested point exactly. The slope gives way, and only at the extremes.
- Which end gets solved is decided by `luxPosition()`, split at the middle so the divisor is never near zero: the far end is always at least half a span away.
- Asking for 100 % (or 1 %) at a middling light level flattens the curve completely, because no line through that point stays in range. That is forced by the arithmetic, not a bug — `{reset: true}` is the way back.
- A drag converges rather than compounding: each request is measured against the curve as it stands, so the last value sent is the one that ends up applied.

The calibration buttons are **entirely in the browser**: the UI reads the current lux from `/light`, takes the brightness from whichever slider is on screen, and posts all four numbers. The firmware only ever stores and validates a curve — there is no "capture" concept in it. The curve is written against the **smoothed** reading, not the raw one, because that is what the curve is fed at runtime; calibrating against anything else builds in an offset.

`POST /light` also accepts `{reset: true}`, which restores the curve from a freshly constructed `Settings` rather than repeating those four numbers in a second place.

**The colour tab polls `/light` every 2 s while it is open** — matching the sampling interval, so the number moves as fast as it can and no faster. It is the only tab that polls; the clock's web server is single-threaded.

The roadmap agreed for this: (1) interface and sensor, (2) measure and display, (3) the curve and two-point calibration — all done — and (4) passive learning with a "geek" tab showing the curves and allowing backup/restore. Step 4 is where the slider could re-anchor the nearer point live, which would remove the "switch the automatic off to calibrate" step.

### Debugging

`RemoteDebug` (telnet-style remote log console, `debugI`/`debugW`/`debugE`/`debugA` macros used throughout the firmware) is the transport; the log itself is also kept in RAM and served to the browser — see "The log ring and the debug tab" below. The instance in `main .cpp` is a `DebugLog`, the subclass declared in [src/LogBuffer.h](src/LogBuffer.h), and every translation unit that logs includes that header rather than declaring `extern RemoteDebug Debug;` for itself as it used to. **That declaration cannot come back**: with the definition being a subclass, each such line would describe a different type for the same object, and the compiler has no way to notice.

Do not confuse it with [src/Debug.h](src/Debug.h), which despite the name has nothing to do with RemoteDebug: it is a leftover set of `DEBUG_PRINT*` macros around `Serial.print`, compiled to nothing unless `DEBUG` is defined, and included only by `Renderer.cpp`. Its companion GUI library, `RemoteDebugger` (variable watch/manipulation via a web console), was already inert before consolidation — the include and its init calls were commented out in `main .cpp` — and has been removed from `lib_deps` entirely, since it no longer compiles against the current ESP32 Arduino core (`std::byte` ambiguity in its vendored source). Its vendored web client, `RemoteDebugApp/`, was removed with it. A browser-based log console (e.g. the WebSerial library) was considered as a replacement but rejected: it requires migrating the whole web server from the synchronous `WebServer` used here to `ESPAsyncWebServer`/`AsyncTCP`, and current WebSerial releases are AGPL-3.0-licensed.

### Expert mode

The update and debug tabs are locked behind a password. One flag in NVS says whether the clock is unlocked; while it is 0, `/ota/*` and `/log` answer `403 {"error":"expertLocked"}` and the web UI does not offer the tabs. [src/Expert.cpp](src/Expert.cpp) owns all of it, and `Expert::guard()` is the single line at the top of every covered handler — six of them, listed in the header so the list cannot quietly grow.

- **It is a mode, not a login.** Setting the flag needs the password; clearing it does not, since someone locking the clock out of spite has gained nothing. HTTP Basic authentication was the first idea and is worse here for a concrete reason: the debug tab polls `/log` every two seconds, so Basic would put the password on the wire some 1800 times an hour with the tab open. One unlock puts it there once. The price is that while unlocked, nothing is protected — hence the visible "lock again" button.
- **The hash is made on the clock, never at build time.** A hash compiled into the image would be published with every release *and* would come back with every OTA update to overwrite the owner's own. This was the design's first version and its own author found the hole. NVS is the one store an update does not touch — the same reason the settings live there.
- **Its own NVS namespace (`qlockexpert`), not a field in the settings record.** Two reasons that both bite: that record is rewritten whole from `fillDocument()` on every settings change, so a field forgotten there is a field lost; and `getJSONSettings()` publishes exactly that shape through `/currentState`, where a password hash has no business being.
- **A fresh clock has no password and is locked.** The first password offered is the one that is kept. That race — whoever reaches it first owns the clock — is bounded and not a regression: an un-enrolled clock is exactly as open as every clock was before this existed, so enrolling can only improve matters.
- **The way back from a forgotten password is the plug.** `POST /expert {reset: true}` clears the enrolment, but only within `EXPERT_GRACE_MS` of a **power-on** reset. `esp_reset_reason()` tells that apart from the software restart the update tab triggers, so rebooting the clock from its own web UI does not open the window — and neither does a USB flash, which reports `ESP_RST_USB`. That this hands the clock to anyone who can pull the plug is not a weakness: without flash encryption, the same person reads the NVS out over USB anyway. Physical access already wins, and a recovery path that admits it beats one that pretends otherwise.
- Five wrong answers stop the endpoint taking any for five minutes, or a short password over HTTP is guessed in seconds. The comparison is constant-time. The password itself is never logged — the ring is what it guards.
- **What it is worth**: it keeps out someone who joins the network and goes looking. It does not keep out someone watching the traffic, since enrolment and unlock cross the wire in the clear, and there is no TLS on the clock.
- The tab row is built from `ALL_TABS` in `App.svelte` with the last two sliced off while locked, so `t.tabs` keeps all six entries in every locale either way. The expert screen has **no chip in the row** and is reached through `#expert` in the address; there is nothing there for someone who has not gone looking. `App.svelte` treats a clock that cannot answer `/expert` as locked, which is what an older firmware under a newer web UI looks like.
- The mock registers its guard as Express middleware **before** the routes it covers. Added at the end it would let every `/ota` and `/log` request through and the dev UI would behave nothing like the device.

### The log ring and the debug tab

The clock keeps its last 200 log lines in RAM and serves them through `GET /log`, which is what the debug tab shows. The reason is timing rather than convenience: the serial port needs a cable and RemoteDebug's telnet server needs someone already connected, so both only ever show what is said **while somebody is listening** — and the lines worth having are the ones from the two seconds after a restart that nobody is ever in time for. The ring holds them until the tab is opened, which may be hours later.

- **The tab asks for `since=0` and gets the beginning**, not a subscription to the present. `more` in the response says whether another batch is waiting, so a freshly opened tab fills in one go rather than one screen every two seconds.
- **Two streams feed the ring**, because they do not meet anywhere lower down. `Debug` is a `Print`, so `DebugLog::write()` reassembles its characters into lines on the way past. ESP-IDF's own logging — the WiFi driver above all — never touches `Print`, and is caught with `esp_log_set_vprintf()` instead. The handful of raw `Serial.println` calls in the WiFi event handler were converted to `debugI` rather than given a third hook.
- **`isActive()` is overridden although it is not virtual.** The macros call it on this type immediately before `printf`, and it is the only place the level is named — so it is the only place the level can be picked up. Everything else about the line (the `(I t:1234ms)` prefix) is built inside RemoteDebug's own `write()` and never handed on, which suits the tab: level and timestamp travel as fields, and the browser colours an error red without parsing anything.
- **`Debug.setSerialEnabled(true)` moved to the top of `setup()`.** `isActive()` answers false until either the serial echo is on or a telnet client has connected, so every `debugX` before the old call site was silently discarded — mounting the filesystem, loading the settings, and WiFiManager choosing between the stored network and its own portal. None of it reached the cable either. That was not a symptom of anything; it was simply never noticed, because nobody watches a boot they cannot see.
- **What it cannot hold is anything before `Log::begin()`**: the ROM bootloader, the second stage and the partition table have all had their say first. A clock that hangs before the firmware runs shows nothing here, which is exactly the case the USB cable is still for.
- **A restart is detected by the sequence number going backwards.** The clock's numbering starts at zero again while the open tab is still asking for 412; the firmware clamps such a request back into the ring and answers with a smaller number than was asked for, and the browser throws its window away rather than gluing a second boot onto the first. Without that clamp the request would match nothing for as long as the tab stayed open.
- **`oldest` is not decoration.** It says which line the ring still starts at, so the browser can tell "nothing new" from "the ring wrapped and you missed 300 lines" and name the gap. Dropping that silently is what makes a log window untrustworthy.
- Storage is a fixed slot per line (`LOG_LINES` × `LOG_LINE_MAX` = 25 KB in `.bss`) rather than a byte ring: it wastes the tail of every short line, and in exchange a sequence number indexes straight into the array, which is what the "everything after 412" query wants. **PSRAM was considered and rejected** — it is switched off for a reason (see `platformio.ini`), the size a browser can usefully render fits in internal RAM many times over, and it survives a crash no better than the heap does. The idea worth keeping from that discussion is a small `RTC_NOINIT_ATTR` ring, which *does* survive a panic reboot; that is not built yet.
- The ring is written from both cores — `loop()` and the web server on core 1, the light sampler and the OTA download on core 0 — so `Log::line()` takes a spinlock for the length of a `memcpy`. The terminator is written **before** the body, so a reader catching a slot mid-overwrite sees a truncated line rather than one running off the end of the array. RemoteDebug itself is not thread-safe and never was; that is untouched here.
- The response is built into one `String` with the room reserved up front rather than through ArduinoJson, which would put every line into a document and serialise that into a second buffer — on the same heap an update wants. `LOG_BATCH` caps one response at about 11 KB.

The tab also carries the state block the update history keeps pointing at: uptime, reset reason, and free / lowest-ever / largest-block heap. **This is the tab the light sensor's roadmap calls the "geek" tab** — the curves and backup/restore of step 4 belong here too, rather than in a second tab beside it.

#### The face in the colour tab

The preview beside the colour wheel is the **real face**: eleven letters by ten rows in the panel of the language that is running, lit exactly where the clock's own frame buffer says, with the four corner LEDs. It used to be three hardcoded lines per locale (`preview: ['ES IST', 'FÜNF NACH', 'ZWEI']`), which have been deleted.

- **The browser is not a second opinion.** `on` is read off `matrix[16]` itself, so what is on screen is what is on the wall — a wrong render included. Rebuilding the grammar in JavaScript would have been a second implementation to keep in step, and it would have hidden exactly the faults worth seeing.
- **`on` is `#` and `.`, not a bit mask.** `curl http://<clock>/panel` then shows two aligned grids, the letters and what is lit, which is worth more than the eighty bytes it costs on something whose whole purpose is visual inspection.
- **The corners are reported separately and in reading order** — top left, top right, bottom right, bottom left. Which frame buffer row is which corner of the face is `matrix[1]` top left, `matrix[0]` top right, `matrix[3]` bottom right, `matrix[2]` bottom left, and it is written down once at `Renderer::setCorners`. **It cannot be derived from the code.** The driver's wiring comment says something else, and the path from a row to a pixel goes through two remappings that partly cancel — `writeScreenBufferToMatrix` sends `matrix[1]` to `_setPixel(110)`, and `_setPixel` swaps 110 with 112. Following that through gave a mapping that was wrong on the face; the order the corners actually light in, watched on the clock, gave the right one. Check that against a clock, not against the source.
- **Unlit letters are drawn faintly rather than hidden**, because on a real panel they are still there. A grid with no letters at all then means "no panel data" — a different thing from a clock that is switched off, and the two must not look alike.
- Polled every 5 s, not at the sensor's 2 s: the letters change every five minutes and the corners once a minute, and the clock answers one request at a time.
- The mock renders standard German from the wall clock time. It is not a second renderer and does not try to be — it exists so the layout and the moving corners can be worked on without a clock.

### Vendored/generated content (not project source)

- `.pio/`, `dist/`, `compile_commands.json`, `idedata.json` — PlatformIO build cache and IDE tooling metadata, not hand-maintained.

### Hardware consolidation

This project originally targeted several boards and LED drivers. It has been consolidated to a single target: Seeed XIAO ESP32-S3 + WS2812B. As part of that, the following were removed as dead/unreachable code:
- The `nodemcu-32s` and `esp32-c3-display` PlatformIO environments, and the 4 MB `partitions.csv`/`min_spiffs.csv` tables that only they referenced. (The current `partitions.csv` is unrelated — a new 16 MB table for the XIAO ESP32-S3 Plus, see above. The S3 environment never referenced a partition file before and relied on the board default.)
- `src/Configuration.h`, a large block of compile-time `#define` toggles for the original AVR/DCF77/multi-driver-era hardware (alarm, DCF77 receiver, alternate LED drivers, RTC chip selection, IR remote variants). It was already unreferenced by any active code path before removal.
- The `LedDriver` abstract base class, merged into `LedDriverWS2812FastLED` since it was the only implementation.
- The `RemoteDebugger` lib_dep and vendored `RemoteDebugApp/` web client (see "Debugging" above).
- `src/LDR.h`/`.cpp` and the `BH1750` lib_dep, replaced by the VEML7700 behind an interface (see "Light sensor"). This one was kept through the first consolidation and only went when something took its place.
