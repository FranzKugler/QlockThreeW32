# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

QlockThreeW32 is ESP32 firmware for a "word clock": a letter matrix backlit so the current time reads as a sentence (in German, Swiss German, English, French, Italian, Dutch, or Spanish), updated every 5 minutes, with four corner LEDs indicating the remaining minutes. It's a long-running hobby project (originally AVR-based, later ported to ESP32) and carries some legacy cruft from that history — expect dead code paths and commented-out alternatives, though the project has been consolidated onto a single hardware target (see below).

The repo has four parts:
- **Firmware** (`src/`) — PlatformIO/Arduino C++ for the ESP32, drives the LED matrix and hosts a small config web server.
- **Web UI** (`web/`) — a Svelte 5 + Vite single-page app, built into `data/` and flashed to the ESP32's LittleFS filesystem, where the firmware's web server serves it. `data/` is generated output and is gitignored; run `npm run build` before `pio run -t uploadfs`.
- **Hardware** (`hardware/`) — the KiCad project for the board and the OpenSCAD model of the case and the letter mask. Not built by anything here, but not independent of it either: the mask's letters are generated out of the firmware's language files, see "The panel OpenSCAD cuts".
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
| [src/Coupling.cpp](src/Coupling.cpp) | the map from the lit face to the sensor: the per-cell coefficients, the drive table, and what to subtract |
| [src/LogBuffer.cpp](src/LogBuffer.cpp) | the in-memory log ring, the `DebugLog` tee and the ESP-IDF capture hook |
| [src/Expert.cpp](src/Expert.cpp) | the lock on `/log`, `/ota/*`, `/fs/*` and `/nvs/*`: the password, the NVS flag, `Expert::guard()` |
| [src/FileRoutes.cpp](src/FileRoutes.cpp) | `Files::` — the `/fs/*` handlers: the tree, the download, the streamed upload, the editor |
| [src/NvsRoutes.cpp](src/NvsRoutes.cpp) | `Nvs::` — the `/nvs/*` handlers: NVS walked as a two-level tree |
| [src/LabRoutes.cpp](src/LabRoutes.cpp) | `Lab::` — the `/lab/*` handlers: every pixel and the sensor, for measuring the clock |
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
- `POST /color` — hue/saturation/luminance. **With the automatic on, `lum` is a lesson rather than a level** — see "Automatic brightness".
- `POST /autoluminance` — toggle automatic brightness.
- `GET /light` — `{sensor, present, available, lux, raw}` from the ambient light sensor, plus `display` (what the clock's own face contributed to that raw reading) and `coupled` (how many cells the stored map describes, 0 for none — so `raw - display` is what the averages were fed), plus the fitted line (`slope`, `offset`, `fitted`), the `brightness` it yields for the current reading, the regulated range (`minPercent`, `maxPercent`), how many points have been `taught`, and whether a nudge is being waited out (`adjusting`). Not part of `/currentState`: the measurement is not a setting, and the colour tab polls it.
- `POST /light` — `{reset: true}` throws the curve away; `{coupling: {cells, drive}}` stores the map measured by `scripts/lab.py`, and `{couplingReset: true}` removes it. The curve is not configured any more, it is taught through `POST /color`; see "Automatic brightness". **The two coupling branches are behind the lock and the reset is not**, which is not an oversight: a reset discards what the clock has been told and can be told again, while the map changes how the clock reads its own sensor for good — and it is the one thing here nobody types by hand.
- `GET /luminance` — the same line plus every calibration point and what the line makes of it. The workbench at `#luminance`; read-only, and deliberately outside expert mode.
- `POST /configuration` — language, corner LED direction/color. **Refuses a language from another panel on an enrolled clock that is locked** — `403 {"error":"languageNotOnPanel"}`, see "One clock, one panel".
- `POST /timezone` — NTP server + manual DST/timezone rule fields, plus `tzZone` (the picked IANA name, a label only — see "Timezone picker").
- `POST /hostname` — `{hostname}`; renames the clock, answers with the name actually stored.
- `GET /wifi` — connection status (`ssid`, `ip`, `rssi`, `mac`, `hostname`, `switching`, `error`).
- `GET /wifi/scan` — one poll of the async scan: `{scanning:true}` or `{scanning:false, networks:[…]}`.
- `GET /languages` — `[{value, code, name, uiLocale, panel}]`, in the order of the stored language numbers, so `value` is what `POST /configuration` wants. `panel` groups the languages cut into the same sheet of letters — the number of the first language using it, so the four German entries all report 0. Static for a firmware; the shell asks once at load. Not part of `/currentState`: it is not a setting, it is what the firmware can do.
- `GET /panel` — the face as it is right now: `rows` (the panel of the language that is running), `on` (a second grid of `#`/`.` read off the frame buffer), `corners` in reading order, plus `code`, `name`, `uiLocale`, `mode` and the sentence. `cornerColors` appears only while the coloured-corner mode is driving them — four `#rrggbb` strings or `""` for a dark one. Polled by the colour tab.
- `GET /log?since=<seq>` — the log ring from that sequence number on, plus `oldest`, `more`, and the state block (`uptime`, `heap`, `heapMin`, `heapBlock`, `reset`). Not part of `/currentState`: none of it is a setting, and the debug tab polls it. **Behind the lock** — see "Expert mode".
- `GET /fs/list?path=/` — one directory: `entries` of `{name, dir, size, edit}`, plus `total`/`used` of the volume, `editMax`, and `truncated` when the directory holds more than `FS_LIST_MAX`. **Behind the lock.**
- `GET /fs/read?path=/x` — the file, streamed. `&download=1` adds the attachment header. **Behind the lock.**
- `POST /fs/upload?path=/x` — `multipart/form-data` with one file part, streamed into flash. **Behind the lock.**
- `POST /fs/save` — `{path, content}`, for the in-browser editor; buffered, so capped at `FS_EDIT_MAX`. **Behind the lock.**
- `POST /fs/delete` — `{path}`; one file, or one *empty* directory. **Behind the lock.**
- `POST /fs/mkdir` — `{path}`; one directory, parent must exist. **Behind the lock.**
- `GET /lab/state`, `POST /lab/mode`, `GET|POST /lab/leds`, `GET /lab/sensor`, `POST /lab/sweep` — the lab interface; see its own section. Raw pixels and unsmoothed readings, addressed by strip index or by cell. **Behind the lock**, and not mirrored in `server.js`: nothing in the web UI talks to it.
- `POST /restart` — restarts the clock and nothing else, through the same `Ota::scheduleRestart()` a rename uses. It exists for the storage tab, where it is the answer to that tab's own warning about records held in RAM. **Behind the lock**: `/hostname` restarts without a guard and predates it, but there is no reason for a new reboot button to be open to the network.
- `GET /nvs/list` — every entry in the partition as `{ns, key, type, size, suffix, edit|protected}`, plus `used`/`total` in entries. One answer, not one per namespace: the iterator walks the whole partition anyway. **Behind the lock.**
- `GET /nvs/read?ns=&key=` — one value as text, or as bytes where it has no text form. `&download=1` names the file. **Behind the lock.**
- `POST /nvs/save` — `{ns, key, content}`, in the type the key already has. **Behind the lock.**
- `POST /nvs/delete` — `{ns, key}`. **Behind the lock.**
- `GET /expert` — `{enrolled, unlocked, grace, lockedOut}`. No secret in it; the shell needs it before it can decide which tabs exist.
- `POST /expert` — `{password}` (sets one on a clock with none, otherwise checks it), `{off: true}`, or `{reset: true}`. Answers with the same shape `GET` does.
- `POST /wifi` — `{ssid, password}`; answers immediately, the switch runs in `loop()`.
- `GET /ota/status` — `{firmwareVersion, fsVersion, sketchSize, freeSpace, error}`.
- `POST /ota/upload` — `multipart/form-data` with one file part; answers, then reboots.
- `GET /ota/check` — polls the channel's manifest, answers with the full status.
- `POST /ota/install` — starts the download in a task; answers immediately.
- `POST /ota/config` — `{channel, autoUpdate, checkInterval}`.

Changing the API means touching **four** places: the firmware handler, `server.js`, `web/src/lib/api.js`, and the route list in [vite.config.js](vite.config.js) — a new endpoint that is not in `API_ROUTES` is not proxied, so it works on the device and 404s in `npm run dev`.

**Errors are codes, not sentences.** The firmware answers with `{"error": "otaChecksum", "errorDetail": "HTTP 404"}` — a stable code plus an untranslated technical detail — and [web/src/lib/errors.js](web/src/lib/errors.js) turns it into text in the current language from the `err_*` keys in the locale files. It used to send German sentences, which was fine while the UI was German too. An unknown code is displayed as-is rather than swallowed, so a clock running newer firmware than its web UI still says something useful. The generic `post()` in `api.js` reads the code out of a failed write too, so a refusal reaches the banner as a sentence rather than as `HTTP 403`.

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
- The mode/language/corner values in the UI are the firmware's own numbers (`STD_MODE_*`/`EXT_MODE_*` in [src/DisplayModes.h](src/DisplayModes.h), `LANGUAGE_*` in `Renderer.h` — the numbers are stored in NVS and must keep their values). `MODE_VALUES` in `Display.svelte` has to agree with that header. **The language numbers no longer do** — they arrive from `/languages` with their names, so the picker cannot fall out of step with the firmware. Bindings keep them as numbers rather than the strings jQuery's `.val()` used to send.
- Colour changes are throttled ([web/src/lib/throttle.js](web/src/lib/throttle.js)); dragging the wheel would otherwise fire one POST per pointer move at the ESP32's single-threaded web server.
- With "Sommerzeit" off, the changeover fields of both rules are disabled but the standard rule's abbreviation and offset stay editable — the same rule the old `setDst()` implemented.
- Everything is bundled locally. The old page pulled Bootstrap, FontAwesome, jQuery and iro.js from CDNs, so it rendered broken on a LAN without internet access. The colour wheel is still iro.js, now bundled. The built page requests exactly three files on load, all from the clock: the JS bundle, the CSS and the favicon — no webfonts, no `url()` in the CSS, no `@import`. Opening the timezone tab adds a fourth, `/zones.json`, also from the clock. Keep it that way; the clock has to work on a network with no internet at all.
- **Never name a local `$state` in a component that takes the `state` prop.** `let { state } = $props()` makes `$state(...)` parse as a store subscription on that local binding rather than as the rune (`store_rune_conflict`). It is a warning, not an error, so the build succeeds and the variable silently never triggers a re-render. [Timezone.svelte](web/src/sections/Timezone.svelte) therefore destructures as `let { state: clock } = $props()`.
- Failed writes surface as a banner via [web/src/lib/status.svelte.js](web/src/lib/status.svelte.js), instead of being dropped as they were by the old `.done()`-only handlers.
- **The language names come from the clock, and are not translated.** Each locale used to carry an array of ten — `'Englisch'`, `'English'`, `'Anglais'` — in an order that silently had to match the `LANGUAGE_*` defines, so a new language cost one file in `src/languages/` and six in `locales/`. They are endonyms now, as the firmware spells them: someone looking for their own language on a page they cannot read finds *Nederlands*, not *Dutch*. `BUILT_IN` in `i18n.svelte.js` is the fallback for a clock too old to answer `/languages`, and is **deliberately frozen** — such a clock's list is exactly that list, so adding to it would be describing a firmware that does not exist.
- **The UI language is not a setting of its own**: it follows the clock's language. The clock says which languages exist and which locale each one wants (`GET /languages`); [web/src/lib/i18n.svelte.js](web/src/lib/i18n.svelte.js) takes that list and picks one of six locales in [web/src/lib/locales/](web/src/lib/locales/) — the four German dialects and Swiss German all share `de.js`. `App.svelte` drives it from a single `$effect` on `clock.language`, so no section has to know about it. Components read texts with `const t = $derived(dict())`; that `$derived` is what makes the page re-render on a change. `de.js` is the reference: same keys, same order, same array lengths in every locale — nothing falls back per key, a missing key renders as `undefined`. **Check for duplicate keys, not just for equal counts.** Adding an `err_*` code that already existed silently shadowed the original in all six files at once; a count comparison across locales saw six consistent numbers and reported nothing, because the mistake had been made six times identically. In an object literal the last entry simply wins, with no warning from Vite or Svelte.

#### One clock, one panel

The letters are milled once. No setting moves them, so a language whose words are cut into a different sheet turns the face into a wall of letters that spells nothing — and whoever changed it by accident has no way of telling what went wrong.

Once the clock is set up, the language may therefore only move **within the panel it already has**. German, Swabian, Bavarian and Saxon share theirs and can be swapped freely; every other language is alone on its own, so on an Italian clock the picker has one entry and the field is not drawn at all. Expert mode is where a clock is set up; normal mode is where it can no longer be set up wrongly.

- **The refusal is in `POST /configuration`, not only in the browser** — the endpoint is reachable without the UI, the same reason the expert tabs are guarded server-side. `Display.svelte` filters the list so the UI never offers what the firmware would refuse.
- **Panels are compared by their letters** (`Languages::samePanel`), not by a group number stored beside them. A stored number would be the same fact written twice, and the two would drift; comparing ten short strings costs nothing at the rate this is asked. `nullptr` is not the same panel as anything, itself included — "unknown" must not read as "fits".
- **The check is against the *stored* language, never against what the request claims.** Otherwise a request could carry its own permission.
- **A stored language this firmware does not know grounds no refusal.** It says nothing about which panel is on the wall, and refusing on it would lock the language out entirely on a clock whose NVS holds a number from a newer build.
- **`/languages` does not fold the lock state in.** It is static for a firmware, which is what lets the shell ask once and never poll; the browser filters it against `expert.unlocked`, which it already tracks. Making the answer depend on the lock would mean refetching on every unlock.
- **A clock with no expert password is left alone**, and keeps the full list. That looks inconsistent beside `Expert::guard()`, which closes `/log`, `/ota/*`, `/fs/*` and `/nvs/*` on such a clock too, and it is not: those two keep a stranger on the network out, while this one keeps the owner from breaking their own face. Someone who has not yet chosen a password is still setting the clock up, and the language is the one setting that has to match the hardware — locking them out of it at that moment would be the worst possible timing. Enrolling is what says "this clock is set up".
- **It is a one-way street once enrolled.** A clock left locked on Italian cannot be moved back to German without the password. That is the point, but it is not a state anyone can click their way out of.

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

The bit mask is arithmetic (column *c* is bit 15−*c*), so no macros are needed. The text is present, so the log names what is lit in the language that is lit. The geometry is present, so the browser can draw the real panel and `scripts/panels.py` can hand OpenSCAD something to cut. And because the panel letters and the word's own text are both here, `Languages::selfCheck()` confirms at every boot that the letters under a word really spell it — which is the mistake one makes when adding a panel, and which otherwise shows up as a plausible-looking wrong face.

- **The grammar stays imperative, deliberately.** Swabian says "viertel sechs" and counts the hour up where standard German says "viertel nach fünf"; French has "moins le quart"; Italian and Spanish inflect the hour ("è l'una" against "sono le due"). Written as a rule table that becomes a small language of its own, harder to read than the switch it replaces and touched once a year. Each language keeps a `render()`; it just lives next to its panel now.
- **German is four entries over one panel** — standard, Swabian, Bavarian, Saxon — differing in four of the twelve five-minute steps. `Language_DE.cpp` states the differences as a table in its header comment.
- **A cell is not always one character.** English wants O'CLOCK and Italian L'UNA, and on a real panel the letter and its apostrophe share one milled opening and one LED. The rule is that **an apostrophe attaches to the character before it**, so `"TENSEO'CLOCK"` is twelve characters and eleven cells, and no separator is needed in the row string. `Languages::cells()` and `appendCell()` are the only two places that know it, and everything counting columns goes through them. Write the plain ASCII apostrophe — a rule people cannot type is a rule they will not follow, and the first thing typed into a panel after this was built was a plain `'`. U+2019 and U+2032 are accepted as well, so a panel pasted in from elsewhere fails on the word it disagrees with rather than on a mystifying "12 cells, expected 11". Nothing is normalised: the mark is kept as written, so the panel, the log, `/panel` and the SCAD array all spell it the same way.
- **The two panels that need one solve it differently, on purpose.** English uses the rule above, `"TENSEO'CLOCK"`, because there is no single character for O'. Italian uses `Ľ` (U+013D, L with caron) for L' — one character that carries its own mark, so it needs no rule and is cut as one glyph. Do not make them match: the choice follows what Unicode happens to offer, not a preference.
- **Both used to be fakes, and both were caught the same way.** English wrote `Ò` for O' and Italian `Ľ` with the word underneath still saying `LUNA`. `selfCheck()` had been reporting three mismatches at every boot (`en at 9,5 says "OCLOCK" but the panel reads "ÒCLOCK"`, plus `È`/`E` and `ĽUNA`/`LUNA`) since the day the check was written, and nobody read the boot log — which is a fair description of what the debug tab exists for. `Ľ` survived because it is a real answer; `Ò` did not, because it is not.
- **Words with a gap are two entries.** German "ES IST" is ES at column 0 and IST at column 3 with a dark K between, which is also why the log reads "ES IST" and not "ESIST".
- `name` is in the language itself ("Deutsch", "Français") and `uiLocale` names which web locale to speak. Both are served by `GET /languages`, which is what makes the claim below true in practice: **a language added here needs no edit on the web side at all** — not the picker, not the locale files, not the i18n map.
- **Registration is a table line, not a self-registering static object.** The order in which static constructors run across translation units is not defined in C++, and when that goes wrong it goes wrong before there is any way to see it.
- `extern const` on the definitions, not only on the declarations: a `const` object at namespace scope has internal linkage in C++ unless it is spelled out, and the table will not find it. That is one link error, and it is the first one this refactor produced.

**The English panel used to spell FIFE**, and now spells FIVE. Row 2 read `TWENTYFIFEX` while the mask for "five past" lit columns 6..9, so the letters under the word were F, I, F, E. It could not be told from the source which half was wrong, so the file stated what the panel said rather than what it ought to say, and the question was left for a physical panel to answer. It has been: the real panel says FIVE, so the drawing was the wrong half and the mask had been right all along. Worth keeping as the shape of the answer — when a panel and a word disagree, only the wall settles it.

#### How the switch was retired safely

1300 lines of mechanical rewriting is the kind that breaks something quietly and is noticed in October when Swabian says the wrong thing at a quarter past. There is **no host C++ compiler on the development machine** (checked: no gcc, clang, MSVC or MinGW; the cpptools extension ships only clang-format and clang-tidy), so the offline golden-master run that would be the obvious first step was not available.

The on-device comparison that replaced it turned out better: both implementations sat in the same image, on the same chip, so nothing was lost in a shim. `RenderCheck.cpp`, built only with `-DRENDER_CHECK`, rendered every language at every minute of the day through both paths and reported where they disagreed — 14,400 frames, and the answer was **0 differing frames and 0 panel problems** before the old switch, `Woerter_*.h` and the harness itself were deleted. If a language is ever rewritten again, that file is worth resurrecting from git rather than reinventing.

`cleanWordsForAlarmSettingMode()` went with it: it was per-language, and nothing had called it since the alarm was removed.

### LED output

`LedDriverWS2812FastLED` ([src/LedDriverWS2812FastLED.cpp](src/LedDriverWS2812FastLED.cpp)/[.h](src/LedDriverWS2812FastLED.h)) is the sole, concrete LED driver (an earlier `LedDriver` abstract base for swapping in other drivers was folded into this class — there is no longer an interface to implement). It drives a 114-pixel WS2812B strip via FastLED, wired serpentine with the corner LEDs fed separately. **The header's wiring comment is not a source of truth for the corners** — it named an order that is not the one the clock lights, which is why the mapping lives at `Renderer::setCorners` and the four `CORNER_*` defines in the driver restate it in physical pixel numbers. It owns HSV color, brightness scaling, and corner-color/animation state, and converts the `matrix[16]` bitmap to physical pixel writes in `writeScreenBufferToMatrix`.

**Brightness goes through gamma 2.2** (`_gammaScale()`, computed once in `setBrightness()` into `_brightnessScaled`, not per pixel). Perception follows roughly a power law, so driving the LEDs proportionally to the slider does not feel proportional — everything interesting used to happen in the bottom third, and half way up looked far brighter than half. The curve now gives 25 %→12, 50 %→55, 75 %→135. Two details are load-bearing: **the floor of 1**, because 1–3 % otherwise rounds to zero and the clock goes dark while the UI says it is on, and **the corner LEDs use `_brightnessScaled` too** — they were on the raw percentage and drifted visibly brighter than the letters at low settings.

**The coloured corners are the one place that writes corner pixels directly.** With `RenderColorCorner` on, the corners are drawn from `_minute`/`_second` instead of from the frame buffer, because each carries a different hue and a frame buffer row is one bit — the newest corner cycles through the hues once a minute, the ones before it sit at `CORNER_SETTLED_HUE`. **What each corner shows is `cornerHue()`'s to say, and it has two callers**: the strip, and `GET /panel` for the web UI's preview. `CORNER_SETTLED_HUE` is 180, which on FastLED's *rainbow* wheel is violet rather than the cyan a geometric HSV would give — and the moving corner ends its minute at 3×59 = 177, just short of it, so the newest and the settled corners nearly match in the last seconds of a minute. Long-standing behaviour, left alone. It has to count the same minutes `Renderer::setCorners` does, and for a long time it did not: a remainder of 0 lit one corner, so every corner came on a minute early and the fourth never went out, and `_cw` was read in exactly one branch whose `else` was empty, so counter-clockwise ran clockwise. Both came from it being a `switch` with the four states written out by hand; it is a table and a loop now. The mode is off by default, which is why nobody had seen it.

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

[src/LightSensor.h](src/LightSensor.h)/[.cpp](src/LightSensor.cpp) replaced the old `LDR`/BH1750 pair, which had been commented out rather than used for a long time. Four parts:

- **`LightSensor`** — an interface of one meaningful method, `readLux()`. An OPT3001 would be another class beside the two below and one more entry in the candidate list.
- **Two chips, and the choice is made at run time**, not in a build flag: `AmbientLight::begin()` tries each candidate in turn and keeps the first that answers. One firmware serves every build of the clock and they do not all carry the same sensor, and a chip swapped on the bench then needs no rebuild. **The TSL2591 is asked first on purpose** — the two do not share an address (0x29 against 0x10), so a clock with both wired up answers twice, and the more sensitive one is the one to keep.
- **`Veml7700Sensor`** — Vishay VEML7700 on I²C at the fixed address 0x10, read through the library's `VEML_LUX_AUTO` mode. **The BH1750 was dropped because it resolves 1 lx** and behind a dark front panel a lit living room arrives as a handful of lux — the interesting range is fractions of one.
- **`Tsl2591Sensor`** — ams TSL2591 at 0x29, and what the board in [hardware/](hardware/) actually carries (`TSL25911FN`). Resolves to roughly 188 µlx — twenty times finer than the VEML7700 and six hundred times finer than the BH1750. That only matters at the dark end, and **the dark end is the whole point**: behind a front panel an evening living room arrives as a fraction of a lux, and a sensor reporting a flat 0 there cannot tell dusk from night, so the automatic sits at its floor all evening. Its library has **no auto-ranging**, so the class walks a sensitivity ladder itself — a reading taken while changing rung is dropped rather than reported, which is why `readLux()` may answer -1.
- Both hold their Adafruit object as a `void *`, so neither header drags its library into every translation unit.
- **`AmbientLight`** — owns the sensor and samples it **in a FreeRTOS task pinned to core 0**, every 2 s, smoothed with an EMA over 30 s (`dt/(tau+dt)`, seeded from the first reading so it does not crawl up from zero).

**The task is not optional.** Auto-ranging walks gain and integration time and waits for a fresh measurement at each step, which can block well over a second, and the web server here is synchronous — a blocked `loop()` is a clock that stops answering. `smoothed`/`lastRaw`/`sampleCount` are `volatile` 32-bit values written on core 0 and read on core 1; a torn read is not possible for those, so they carry no lock.

`I2C_SDA_PIN` / `I2C_SCL_PIN` default to 5/6 (D4/D5 on the XIAO) and are overridable, because one firmware serves every build of the clock and the sensor is not in the same place in all of them.

`present()` distinguishes "no sensor on this clock" from "sensor found, no reading yet" (`available()`), and both the UI and the boot path depend on that distinction:

- The colour tab **hides the whole "Automatik" section** when `present` is false, rather than showing a switch that does nothing.
- `setup()` **clears `UseLdr` when no sensor answers**, writing straight to NVS rather than through the deferred write (which is armed later in `setup()` and would drop it). Without that, a clock whose sensor is removed keeps a stored "on" for a switch nobody can see, and therefore nobody can turn off.

### Automatic brightness

`Luminance` ([src/Luminance.h](src/Luminance.h)/[.cpp](src/Luminance.cpp)) owns the curve from a reading to a display brightness, and how it is learned. It is a straight line in log light:

```
brightness = slope * log10(lux) + offset      clamped to 20..100
```

- **Log, not lux.** Perception is roughly logarithmic and the range to cover spans decades — a dark bedroom and a sunlit room differ by a factor of thousands, which no straight line in plain lux survives.
- **20 % is the floor, and it is a floor on the *regulation*.** Below a fifth the face is not really readable, and zero is the display switching itself off — a mode chosen in the display tab, never something the light sensor decides. The slider in the colour tab is limited to the same range while the automatic is on, taken from `minPercent`/`maxPercent` in `/light` rather than assumed, so the UI cannot ask for a level the clock will then refuse to use.

#### There is no "remember this" button, and that is the design

Nudging the brightness is the **only** signal a user ever gives about whether the automatic got it right. So the nudge *is* the calibration:

1. The slider moves while the automatic is on. `POST /color` sees `UseLdr` set and hands the value to `Luminance::nudged()` instead of storing it as the manual brightness.
2. The automatic steps aside — `brightnessToApply()` returns the nudge outright, with no easing, because someone holding a slider does not want the clock arguing back.
3. `LUM_SETTLE_MS` (10 s) after the **last** move, `Luminance::poll()` keeps the pair (light now, brightness asked for) and fits the line again through everything kept.

The switch keeps saying "automatic" throughout, because it still is: it is being taught, not turned off.

- **The timer is in the firmware, not the browser.** A tab closed mid-adjustment must not lose the point.
- **The manual brightness is never overwritten** while the automatic is on, so switching it off gives back the value chosen by hand rather than the last thing the learning was told.
- **The colour tab must not follow `brightness` while `adjusting` is true.** During those ten seconds `brightness` is still what the *old* line says; following it would snap the slider back to a value the clock is not showing and is about to revise. The tab has a 2 s local guard as well, for fingers — the two are different problems and both are needed.

#### What keeps the fit from going somewhere silly

- **Plain least squares, both halves, every point weighted the same.** Averaging out the person is what the feature is for: somebody setting the brightness by eye is guessing, and guessing differently each time, so ten statements about a room are worth more than the last one. Age is deliberately not a weight — a point is not less true for being older — and the only ageing is the ring: an eleventh point pushes the first one out. **The cost is that a correction does not land exactly on what was asked**: nudge to 55 % where the fit says 47, and the clock settles between the two, and repeating it changes nothing because the replaced point is the same point. That was once treated as the defect and fixed by anchoring the offset on the newest point, which converged perfectly and threw away the averaging. Both ends have now been tried on a real clock; this is the one that was chosen, and the trade is stated in `Luminance.cpp` so it is not rediscovered as a bug.
- **The points are held oldest-first, not in a ring with a write cursor.** A replaced point is taken out and re-appended rather than overwritten in place, so the stored order really is the order things happened — which is what decides who leaves when an eleventh arrives, and what the store comment had been claiming while the ring quietly broke it.
- **`applied` in `/light` and `/luminance` is what actually reached the driver**, and it is not the same number as `brightness`: during a nudge the clock shows the nudge, and afterwards it eases towards the curve by an eighth a second. Without it the workbench could say what the curve wants and what the sensor sees but not what the clock is doing, which is exactly the number missing when the automatic first felt wrong.
- **`POST /color` used to drop `"lum": "55"` in silence.** `doc["lum"].is<int>()` refused a string while the manual branch beside it converted one happily, so the automatic looked broken in a way it had not earned. It is `isNull()` now.
- **Too little spread, and only the offset moves.** Ten corrections made in one evening say nothing about steepness; least squares through them is noise multiplied by a large number. Below `LUM_FIT_MIN_DECADES` (0.6, a factor of four) the slope stands and only the level is re-fitted. That is exactly what the old "shift the whole curve" did — it was never wrong, it was just done *always* instead of only when it is all one can honestly do. `fitted` in `/light` and `/luminance` says which happened.
- **A slope of zero or less is refused** the same way. Darker room, brighter clock is not a thing anybody wants, and one careless nudge in daylight produces it.
- **A new point replaces a near neighbour** (within `LUM_SAME_LIGHT_RATIO`, 1.3) instead of joining the queue. Without that, ten evening corrections push the one daylight point out of the ring and the line collapses onto a single lighting condition — which is the failure this whole scheme has to survive, because people adjust their clock while sitting in front of it, usually in the same room at the same time of day.
- The ring holds `LUM_POINTS` (10). Enough to describe a home, small enough that a bad point is forgotten within a week of ordinary use.

#### Storage, and what replaced what

- **Its own NVS namespace `qlocklight`**, one JSON string under one key — the same shape the settings use, and next to them rather than in them: a settings record is rewritten whole on every change, and this is written from a timer on a completely different schedule.
- **The points are the record; the line is derived.** The coefficients are stored too, for the read-out and for a future tool, but `begin()` re-fits from the points rather than trusting them. If the two ever disagree, the points win.
- The four `AutoLux*`/`AutoBright*` fields are **gone from `Settings`**, along with `brightnessForLux()`, `luxPosition()` and `CALIBRATION_MIN_RATIO` in `LightSensor`. No `SETTINGS_SCHEMA` bump: an old record simply carries four keys nobody reads, which is not the same as misreading one.
- **`POST /light` now only takes `{reset: true}`.** The two calibration points and the `{want}` shift are gone. The defaults it restores (`LUM_DEFAULT_*`, 0.3 lx → 20 %, 9 lx → 100 %) are cautious rather than good: they assume a sensor in the open, and behind a front panel both readings shrink by the same factor — which in log space only shifts the line sideways, so an uncalibrated clock still dims in the right direction, just not by the right amount.

#### The line went to the newest point and back, and both ends were tried

For a while the slope came from least squares and the offset did not: the line was moved to pass exactly through the newest point. On a real curve that reads as a broken fit — the line through one point, the others below it — and the numbers are worth keeping, because both arrangements have now been seen on the same three points:

```
lux       gewollt   anchored   least squares
0.0202         40      40.00          33.01
0.0507         30      46.55          39.56
0.6225         60      64.43          57.44
                        slope 16.4178 either way
```

**The slope was never the difference.** Both fit it identically; only the level moved. Anchored, a correction lands exactly where it was asked for and older points are overridden. Averaged, the line runs between the points and no single correction is ever fully honoured — the residuals sum to zero by construction, which is the definition of the fit and also the reason the last nudge is not obeyed.

The clock now averages, because averaging out the person is what the feature is for: the brightness is set by eye, the guess is different every time, and ten statements about a room are worth more than the last one. **What that costs is a correction that does not converge**, and it is documented at the fit rather than left to be rediscovered.

**The reader's real problem in that table was the data, not the fit.** `0.0202 lx → 40 %` against `0.0507 lx → 30 %` is two and a half times the light asked to be *dimmer*; the near-neighbour rule merges points within a factor of 1.3 and left these two standing. No line can satisfy both, which is why the averaged one misses one by 9.6 % and the anchored one by 16.5 %.

**The mock had been right the whole time.** `server.js` never stopped computing `offset = meanY - slope * meanX`, so the two implementations disagreed from the day the anchoring landed until the day it was taken out again — in a project whose own rule is that a change to the API means touching four places. A behaviour is as much part of the contract as a field name.

#### Scales, a grid, and the two clamps

The chart had neither axis. A reader could see that the points were scattered but not by how much, and the difference between 10 % out and 40 % out is the difference between a curve worth keeping and one worth throwing away.

**Nine ticks to the decade, not one.** Whole decades were the first attempt and read as a linear axis with odd numbers on it — the uneven spacing of 1, 2, 3 … 9 is what makes a log axis legible as one at a glance. Labels are thinned by distance rather than by rule, in **two passes**: the decades claim their labels first, then the minor ticks fill what is left, each measured against every label already placed. Done left to right in one pass, a decade shoulders its way in 19 px after a minor label and the two overlap — which the first version did, at the place a reader looks first. The effect is that a sparse range gets `0.01 0.02 0.03` and a crowded one drops to `0.1 0.3 1 3`, with nobody having to decide in advance which numbers those are.


Brightness is ruled every twenty per cent.

**Both ends of the regulated range are drawn as dotted lines, and both are settable.** `LUM_MIN_PERCENT` was 20 because that suited one clock; how dim a face is still readable depends on the panel in front of it, how far away it is read from, and whose eyes are reading it. They are stored with the curve in `qlocklight` (a record without them reads as 20/100, which is what such a clock was regulating to) and written through `POST /luminance {minPercent, maxPercent}`.

- **Zero stays unreachable.** `LUM_RANGE_FLOOR` is 1: the display switching itself off is a mode, chosen in the display tab, and never something the light sensor gets to decide.
- **`LUM_RANGE_GAP` keeps the ends apart**, or the curve becomes a constant and the whole screen a lie.
- **Points outside the new range are kept as they are.** They are what somebody said; a range moved back would want them again. Clamping happens where they are used, not where they are stored.
- **Both ends are posted together even when one moved**, because the firmware validates them against each other.

#### The table is in the chart's order

- **The newest point wears a dot in the table**, because a table sorted by light carries no other trace of age and that is the point the ring will still hold when the others are pushed out. It briefly wore a ring in the chart as well, back when the line was pinned to it; with a plain least-squares fit it is not special to the line at all, and marking it there would only suggest it was.
- **The table is sorted by light**, because that is the order the points appear in on the chart above and therefore the only order in which the two can be read together. **The row keeps its real position** all the same: forgetting a point addresses it by index into the clock's own oldest-first array, and sorting the display must not renumber it.
- **Sorting by light is what made the contradiction visible.** `0.0202 → 40 %` sitting directly above `0.0507 → 30 %` states the problem; the same two rows in the order they were made say nothing at all.

#### The clock measuring itself

[src/Calibration.cpp](src/Calibration.cpp) is `scripts/lab.py calibrate` moved onto the clock: the same three passes, started from a button on the brightness screen, with no laptop on the network. It exists because the coefficients belong to one clock — "cell (7,5) puts 69.1 lx into the sensor in red" is true only while the sensor sits behind that letter — and a calibration that needs Python is one most clocks will never get.

- **Two things the script was handed and this works out for itself.** The rung, and whether the room is dark enough. `COARSE_RUNG = 4` is written into the script because its author knew one clock; and the script's docstring says "cover the clock", which is advice, not a check. The firmware measures the ambient first and **refuses** above `CAL_MAX_AMBIENT_LUX`, because a map measured through daylight looks exactly like a good one afterwards.
- **A task on core 0**, like the OTA download and for the same reason: ninety seconds of blocked `loop()` on a synchronous web server means no progress to show and a clock that answers nothing.
- **One owner of the strip.** `Lab` refuses to take it while a calibration runs and the other way round, and the render loop asks `Lab::active() || Calibration::running()`. A script stepping in halfway would not fail visibly — it would produce a map that looks like every other map.
- **Progress rides in `/luminance`**, which the screen already polls once a second, while the action is `POST /light {calibrate: true}` beside the upload that produces the same thing. A polling endpoint of its own would only be a second answer to disagree with.

**It found its own bug on the first run, which is the argument for building it at all.** The rung search scanned rows with auto-ranging — and `readLux()` returns **−1 while the ladder is moving**, deliberately, because that reading belongs to two gains. −1 loses a contest for the *strongest* cell. So the brightest cells looked like the weakest, a rung was chosen to suit a middling one, and in the pass that followed the real peaks saturated, came back as zero and dropped out entirely: the clock reported twenty cells and not one of the three that carry the coupling. It now pins the **blindest** rung for the whole search, compares counts rather than lux, and climbs the ladder only as far as the strongest cell allows. This is the same mistake the script made once in the other direction, and it is worth writing down twice: **a scan whose rung can move under it lies confidently.**

Second run, against the script's own numbers measured a day earlier at a different rung:

| cell | clock | script | |
|---|---|---|---|
| 7,5 | 67.5 / 78.2 / 48.2 | 69.1 / 79.6 / 48.9 | −1.8 % |
| 7,6 | 22.4 / 21.4 / 12.7 | 21.8 / 20.6 / 12.1 | +3.7 % |
| 8,5 | 14.6 / 13.1 / 7.3 | 14.1 / 12.5 / 6.9 | +4.7 % |

Same ten cells, same order, drive table identical to three decimals (0.477 against 0.480 at half drive). The weak cells drift up to +32 %, which is noise on half a per mille of the peak.

- **`base_colour()` in the script taught the clock three junk points**, and the mechanism is worth remembering: with the automatic on, `POST /color` is not a setting, it is a lesson. The function turned the brightness to 100 and back to read the colour off the strip, and the clock stored "0 lx deserves 50 %" three times — the lab had blanked the strip — collapsing the fitted slope from 27.5 to 3.6 %/decade. It switches the automatic off first now. Reading the colour without writing at all was the other option and is worse: at a drive of eighteen the colour comes back `[36, 255, 0]` where it is really `[32, 245, 11]`.
- **The `feedback` table divided by the dark reading**, which is zero in the dark room the run needs, and printed a serene `0.0 %` for every row — the one number it exists to produce, printed without having been computed. It measures against the model where there is no room to measure against.

#### The brightness screen — one screen, two ways in

[Luminance.svelte](web/src/sections/Luminance.svelte) holds everything about the automatic: the line, every point with what the line makes of *its* light, both averages, and how much of the sensor's reading is the clock's own face. The chart is in log light — plotted against plain lux it would be a curve, hiding the one thing worth seeing, which is whether the points sit on a line at all.

**The lock decides what it offers, not whether it exists.** Unlocked it is the eighth tab; locked it is still reachable at `#luminance` and shows the same numbers read-only. The firmware draws exactly the same line: `GET /luminance` is open, `POST /luminance` is not, and in `POST /light` the two coupling branches are guarded while the `{reset}` beside them is not.

That split is the point. Looking at the curve is what somebody does when the automatic feels wrong, and a password in front of a diagnosis helps nobody — while editing a curve, or throwing away a coupling measurement that took twenty minutes, is a different act. It replaced a flat "deliberately not behind expert mode", which was right while the screen was read-only and stopped being right when it grew writes.

- **One component, a prop for the lock.** Two components would have been two things to keep in step, and they would have drifted the first time a field was added to one.
- **The brightness screen survives locking**, unlike every other tab behind the lock: `applyExpert()` excludes it, because it stays readable rather than turning into a screen whose every request is refused.
- **`POST /luminance {forget: n}`** removes one point. A point can be wrong rather than merely old — a correction made ten seconds after the room went dark was stored at 0.1184 lx when the room was at 0.0008 — and until this existed the only remedy was throwing the whole calibration away. Addressed by position, so the screen refuses a second click while one is in flight; two deletes racing would remove the wrong point.
- **A 307 redirect is a trap here.** The mock's writes first answered by redirecting to `/luminance`, which re-sends the POST to the handler that issued it. Both writes share `luminanceState()` instead.
- Polled once a second, faster than the colour tab, because this is the screen somebody watches *while* dragging the slider.

#### Subtracting the clock's own face

[src/Coupling.h](src/Coupling.h)/[.cpp](src/Coupling.cpp) is the measurement of the section below, turned into a correction. Every background sample has the display's own contribution taken out of it **before the averages see the number** — before, because the loop it closes acts within one sample, so a contribution left in is one the regulator has already acted on.

- **Nothing stored means nothing subtracted**, and the clock behaves exactly as it did before this existed. A guessed map would be worse than none: the coefficients depend on where the sensor was fitted and what sits between it and the LEDs, which is a property of one clock in the same way its panel letters are. Hence NVS, never compiled in — and `scripts/lab.py <clock> upload` after a `calibrate`.
- **The sensor does not know there are LEDs.** `AmbientLight::compensateWith()` takes a function pointer and `main()` hands it `Coupling::contribution`; `LightSensor.cpp` samples a sensor and what else happens to be shining into it is somebody else's subject. Without that, the light sensor would have to include the LED driver.
- **`readNow()` stays raw.** The lab measures the coupling, and an instrument that measures through its own correction measures itself.
- **Read off the driver's pixels, not off the frame buffer.** The brightness and the colour are already in them, so nothing here has to know about either — which is also what makes it right at a drive of seven, where the display gamma puts 20 % brightness.
- **No lock, on purpose.** The sum runs on core 0 while the pixels are written on core 1; the worst a torn read produces is one pixel from the previous frame, a fraction of a per cent of one sample, and a lock would be held across a hundred pixel reads on the path that must never delay the strip.
- **Clamped at zero.** A model that overshoots by a per cent must not turn a dark room into negative light, which `log10` has no answer for.
- `GET /light` reports `display` and `coupled` beside `raw` and `lux`, for the same reason `applied` exists: without the middle number a correction looks like a sensor fault.

#### The sensor must not see the display

Measured on the clock, automatic off, room unchanged:

```
display  20 %  ->  raw  0.42 lx
display 100 %  ->  raw 16.79 lx
```

A factor of forty, all of it the clock's own light. That closes a positive feedback loop — brighter face, more measured light, the curve asks for brighter still — and it runs to whichever end it is nearer. It also poisons what is learned: the lux kept ten seconds after a nudge is mostly the display's contribution at the brightness just chosen, so the point describes the clock rather than the room.

Measured properly afterwards through the lab interface, the coupling turned out to be **local and steeply peaked** rather than diffuse: the sensor is behind row 7 column 5, its own cell reads 240 lx over dark, and the face falls away by roughly a factor of six per cell of distance. That makes a per-cell compensation map meaningful, which the paragraph below was written before knowing.

**No amount of fitting survives this.** Solving the two readings above for ambient plus a display term proportional to the gamma-corrected drive leaves an ambient of roughly zero, so a compensation term would be subtracting two nearly equal numbers and keeping the noise. The fix is optical: the sensor has to be shielded from the LEDs or moved out of their light. Check this first on any clock where the automatic behaves oddly — the numbers above take four minutes to reproduce with `/light` and the automatic switched off.

`brightnessToApply()` in `main .cpp` still decides what reaches the driver each tick: the manual setting immediately with the automatic off, the nudge outright while one is being waited out, and otherwise the computed value approached by an eighth of the remaining distance per second — about twenty seconds for a full swing. The reading is already smoothed over 30 s, so that easing is not about noise: it is about the step when a lamp is switched on.

### Debugging

`RemoteDebug` (telnet-style remote log console, `debugI`/`debugW`/`debugE`/`debugA` macros used throughout the firmware) is the transport; the log itself is also kept in RAM and served to the browser — see "The log ring and the debug tab" below. The instance in `main .cpp` is a `DebugLog`, the subclass declared in [src/LogBuffer.h](src/LogBuffer.h), and every translation unit that logs includes that header rather than declaring `extern RemoteDebug Debug;` for itself as it used to. **That declaration cannot come back**: with the definition being a subclass, each such line would describe a different type for the same object, and the compiler has no way to notice.

Do not confuse it with [src/Debug.h](src/Debug.h), which despite the name has nothing to do with RemoteDebug: it is a leftover set of `DEBUG_PRINT*` macros around `Serial.print`, compiled to nothing unless `DEBUG` is defined, and included only by `Renderer.cpp`. Its companion GUI library, `RemoteDebugger` (variable watch/manipulation via a web console), was already inert before consolidation — the include and its init calls were commented out in `main .cpp` — and has been removed from `lib_deps` entirely, since it no longer compiles against the current ESP32 Arduino core (`std::byte` ambiguity in its vendored source). Its vendored web client, `RemoteDebugApp/`, was removed with it. A browser-based log console (e.g. the WebSerial library) was considered as a replacement but rejected: it requires migrating the whole web server from the synchronous `WebServer` used here to `ESPAsyncWebServer`/`AsyncTCP`, and current WebSerial releases are AGPL-3.0-licensed.

### The storage tab

The seventh tab, behind expert mode, holds an explorer over the clock's two persistent stores: **LittleFS**, the 3.5 MB filesystem this page is served from, and **NVS**, the key-value store the settings live in. [Storage.svelte](web/src/sections/Storage.svelte) is the switch between them, [Explorer.svelte](web/src/sections/Explorer.svelte) draws either, and [web/src/lib/explorers.js](web/src/lib/explorers.js) is the interface that makes one component enough. [src/FileRoutes.cpp](src/FileRoutes.cpp) serves `/fs/*` and [src/NvsRoutes.cpp](src/NvsRoutes.cpp) serves `/nvs/*`.

It began inside the debug tab and moved out: a file tree next to a log window is two unrelated jobs sharing a screen, and the log is the one that needs the room.

- **The switch between them is a pair of underlined tabs**, the same idiom as the tab row at the top of the page. A segmented control was tried first and read badly on a real screen: two greys on a third grey, with the selected one told apart only by a faint shadow.
- **Three different quantities get a number beside them, and conflating them shows.** A value is always in bytes; the volume is bytes on the filesystem and 32-byte entries in NVS; a folder says nothing on the filesystem and its key count in NVS. One shared formatter labelled an 80-byte value as "80 entries" until a screenshot caught it.
- **Svelte's style scoping does not stop a global rule matching.** It adds a class, so a component's `.switch` compiles to `.switch.svelte-xyz` — and `.switch` in [app.css](web/src/app.css) *also* still matches, because it is the less specific selector on the same element, not a different one. app.css gives `.switch` a fixed `width: 2.6rem; height: 1.5rem` for the on/off toggle, so the storage tab's row inherited a 24 px height it never set, its labels overflowed, and the selected tab's underline came out through the middle of the word. It is `.store-tabs` now. **Before naming a class in a component, check it is not already in app.css** — the properties a component happens not to set are the ones that leak in, and they leak in silently.
- **The colour tokens are `--surface` and `--text`.** `--card` and `--fg` do not exist, and a `var()` naming neither falls back to nothing — which is how the context menu shipped transparent and unreadable, and how the `#luminance` chart lost its background at the same time.
- **They are named by what they are.** "LittleFS" and "NVS" are the words in every ESP32 document and in this project's own logs, so somebody searching for where their settings went finds the panel holding them. A friendlier metaphor would have to be un-learned the first time anything goes wrong.
- **The difference between them is the whole reason both exist**, and each panel says so in a line: a filesystem update overwrites LittleFS wholesale, and leaves NVS untouched. That is why the settings, the expert password and the brightness curve are in NVS — see "Settings persistence".

#### NVS as a tree, and where the pretence stops

A namespace is drawn as a folder and a key as a file. It reads convincingly because every record this clock writes is a JSON string under one key, so `qlock` really does look like a folder holding `conf.json`. The four places it is not a filesystem are stated in [src/NvsRoutes.h](src/NvsRoutes.h) rather than left to be discovered:

- **The tree is two levels deep and cannot be deeper.** There are no sub-namespaces; a folder inside a folder is not refused, it is impossible.
- **Nothing to upload, no folder to create.** A namespace exists because keys are in it and vanishes with the last one, so both buttons would be writing a key by another name. The panel drops them rather than greying them out — `canUpload`/`canMkdir` on the adapter, so the component has no branch of its own.
- **The extension is a reading, not a fact.** A string starting with `{` or `[` is offered as `.json`, another string as `.txt`, everything else as `.bin`. Nothing in NVS records a name. `NVS_PEEK_MAX` stops the listing pulling a large blob into the heap just to look at its first character.
- **Sizes are entries, not bytes**, because `nvs_get_stats()` counts in 32-byte entries and inventing a byte figure would be worse than an odd unit. Hence `unit` on the adapter.

Two more, from building it:

- **Raw `nvs_*` calls, not `Preferences`.** This has to walk namespaces whose names it does not know and read values whose type it learns while walking; `Preferences` wraps one namespace of a known shape, which is right everywhere else in this firmware and wrong here.
- **The stored type decides what a write may be.** Putting a string over a `u8` would leave the firmware's `nvs_get_u8` finding nothing and the setting silently reverting to its default — the worst way for this to fail. A number that will not parse is refused with `nvsNotANumber` instead.
- **One value is deliberately unreadable: the expert password hash, and its salt.** Everything else here is exactly as open as the unlock that reached it and closes again when the clock is locked; a hash carried off during a borrowed thirty seconds is crackable offline forever, and probably against a password used elsewhere too. The key is still *listed* — a tree that hides entries is a tree that lies — only the read and the write are refused.
- **An edit to a cached namespace is only as durable as the next settings save.** The firmware holds `Settings` in RAM and writes the whole record back on any change, so editing `qlock/conf` and then touching a slider loses the edit. The panel warns, and `/nvs/save` answers `cached: true` so a curl user sees it too. **The warning carries a restart button**, because saying "only an immediate restart makes this stick" and then leaving the reader to find a power socket is telling half the story.

#### One explorer, two stores

`lib/explorers.js` is a narrow interface — `list`, `readText`, `urlOf`, `save`, `remove`, and `upload`/`mkdir` only where they mean something — and the differences it admits are the ones that would be lies to hide.

- **The one structural difference is how a listing arrives.** LittleFS answers per directory and the tree expands lazily, which keeps a full directory from making every response slow. NVS has no such thing: `nvs_entry_find` walks the whole partition regardless, so it is fetched once and sliced in the adapter. Hiding that behind `list(path)` means the component never has to know.
- Switching panels destroys the component and builds a new one — the two sit in different `{#if}` branches — so there is no reset code and no effect watching the prop.

#### The context menu

Right-click, or press and hold for `LONG_PRESS_MS` (500 ms) on a touch screen. That replaced a download link and two buttons on every row, which was most of what the tree looked like.

- **Long press is the only honest touch equivalent.** A swipe collides with the page scroller, and an always-visible "⋯" per row is the clutter this removed. The gesture is named in a line under the tree, because a context menu nobody finds is worse than the buttons were.
- **A long press ends in a click**, which the window's close-on-outside handler would read as "clicked elsewhere" and shut the menu the instant it appeared. Hence the 300 ms guard on `openedAt`.
- Rows are focusable and answer Enter, Space and the menu key, so the tree works without a mouse. `onKey` checks `event.target === event.currentTarget` first: a folder row contains a button, and Enter on that means "open the folder" — letting it bubble would open the folder and the menu at once.
- `user-select: none` and `-webkit-touch-callout: none` on the row, or the long press selects the text and raises iOS's own callout instead.

#### Pretty-printing, and the way back out

JSON arrives from both stores as a single line — that is how the clock writes its records and how the build writes `version.json` — and one line is not something anyone can correct. So it is laid out on the way in.

**What matters is the way back.** A record shown pretty and saved pretty is three times its size on a partition with a job to do, so a value that arrived compact goes back compact. The switch is visible rather than magic: hiding it would mean the editor silently deciding what to write. If the box stops being valid JSON it is saved exactly as shown, because refusing to save half-typed text is worse than not minifying it.

### The lab interface

`/lab/*` ([src/LabRoutes.cpp](src/LabRoutes.cpp)) gives a script direct control of every pixel and of the light sensor, and [scripts/lab.py](scripts/lab.py) is the client and the experiments run through it. It exists because of the feedback loop under "The sensor must not see the display": how strongly, from which cells, and whether the infrared channel escapes it are all questions with numbers for answers, and none of the numbers can be reasoned out of the source.

**It is an instrument, not a feature**, and everything about its shape follows from that:

- **Raw means raw.** No gamma, no brightness scaling, no colour setting — a pixel goes to the strip as the number it was given, because an instrument that applies two correction curves measures itself.
- **The sensor is read synchronously and unsmoothed**, through `AmbientLight::readNow()`, with the sensitivity rung pinnable. The regulator's thirty-second average answers a different question, and a scan comparing frames cannot have the gain moving underneath it.
- **A sweep is one request.** Frames, a settle, a reading, all in `POST /lab/sweep`. Three round trips per frame would put network jitter inside the measurement; the price is that the clock answers nothing else while a sweep runs, which is stated rather than worked around.
- **Nothing here is a setting.** `EXT_MODE_LAB` (7) is never written to NVS, `isKnownMode()` says no to it so `POST /display` and the boot path cannot reach it, and it is left by a request or by a restart. It stays in the firmware because the coupling between a face and its sensor is a property of one clock's geometry, the same as its panel letters — every clock needs this measured, not just the one it was written on.

Load-bearing details:

- **Both addressings, on purpose.** `{"i": 0..113}` is the strip, `{"cell": [row, col]}` is the face. Having both is not convenience: lighting cell (9,10) and lighting pixel 0 must be the same lamp, so the mapping can be *checked* rather than believed — and this project has had it wrong before. `LedDriverWS2812FastLED::physicalFor()` is now the one place that computes it.
- **The lab refuses an over-budget frame instead of dimming it.** FastLED's power cap works by scaling the *global* brightness down, so a frame over budget is not the frame that was asked for and every number taken from it is quietly wrong. `LAB_MAX_DRAW_MW` is 7.5 W, about 25 pixels of white. The number is not theoretical: the first whole-face white frame drew an estimated 23.6 W, browned the clock out and reset it. `SUPPLY_MILLIAMPS` came down from 4000 to 2500 at the same time — the normal face draws about 170 mW-equivalent, so the cap never engages in use.
- **The sampling task is held off the bus while the lab has it**, and there is a mutex either way: two tasks on one I²C bus, and a half-finished transaction is not a small problem. Held means the sampler *skips its turn* rather than blocking, or a scan holding the bus for a minute would be followed by a stale reading pushed straight into the regulator's average.
- **A restart ends the session.** That was designed in and then confirmed by accident: the brownout above left the clock showing the time again, not dark.
- **No mock and no Vite proxy entry.** Nothing in the web UI talks to `/lab`; its only client is a Python script talking to a real clock over a real strip, and a mock of a light sensor measuring a mock of an LED would test nothing.

#### What it found

Within an hour of existing, on the clock it was written for:

- **The wiring in three files was wrong** and the code was right. Confirmed by lighting cells and reading back which pixels they are.
- **The coupling is local, not diffuse.** Row 0 gave nothing and row 7 gave 240 lx over dark — a ratio of thousands, which kills the "light piped through the front sheet" hypothesis and means a per-cell map is worth building.
- **The sensor sits behind row 7, column 5** — the `N` of `SECHSNLACHT`, a filler letter no word uses, third row from the bottom and centre. Its neighbour at (7,6) is the `L`, also unused. The strongest cell that a word *does* light is (8,5), the `N` of `SIEBEN`, at 17 % of the peak.
- **The whole 40x feedback was one letter.** With the map in hand the original measurement made sense: it was taken around seven o'clock, and the `N` of `SIEBEN` at (8,5) sits directly below the sensor at 161/1000 of the peak. One word in white, room dark: SIEBEN 32.4 lx, SECHS 4.8, ZWOELF 4.7, ACHT 0.26, NEUN 0.14, ES IST 0.002. The clock regulates cleanly for eleven hours in twelve and loses its mind in the hour around seven. That is why it felt wrong rather than broken.
- **The corner LEDs contribute nothing** - measured, 0 counts each.
- **`lux` was `null` in a dark room**, which is how a real bug surfaced: `calculateLux()` divides by CH0, and `readLux()` guarded only `lux < 0`, which is false for a NaN. One NaN reaching the exponential average would have stayed there for ever. Now both channels at zero answer 0 lx - darkness is a reading, not a failure - and everything else goes through `sane()`. The first version of that fix sat *below* the pinned-rung early return, so every lab measurement still got a NaN; the guard has to come first.

#### What the compensation rests on

Three things were measured before the model was written rather than assumed, and all three hold:

| | |
|---|---|
| superposition across channels | white came out 1.3 % from red + green + blue |
| superposition across cells | the word SIEBEN came out 0.9 % from the sum of its six cells |
| the far field really is zero | a whole row outside the map: 0.002 lx |

- **Colour matters, and not slightly.** Normalised to the sensor's own cell, `(7,4)` couples at 36.6/1000 in red and 18.9/1000 in blue - almost a factor of two, systematic with wavelength. One white coefficient would be about 9 % out on this clock's green face. Hence **three coefficients per cell**, one per channel, for the ten cells above 1/1000.
- **The drive response is neither linear nor a gamma.** Half drive gives 0.48 of full, a quarter 0.216, an eighth 0.082, and 16 gives 0.024 where a proportional lamp would give 0.063. An offset of twelve counts fits from 255 down to 24 and then breaks completely. It is stored as a **table**, because nothing fits it - and it is one table for all channels and cells, which was checked: white, red, green and blue gave the same curve to a few parts in a thousand, so it belongs to the LED and its driver.
- **That table is not a nicety.** 20 % brightness through the clock's own gamma curve comes out as a drive of about seven, so the dim hours live entirely in the part of the curve where proportionality is 22 % wrong and worse.
- **Verified against the strip** over a 150:1 range, including drive 7: 2-6.6 % throughout. The systematic part of that is the sensor, not the model - the same light reads 16.03 lx on rung 2 and 15.42 lx on rung 6, a 3.8 % spread across the ladder, and the calibration and the check ran on different rungs.
- `coupling.json` is **per clock and gitignored**, the same as any other measurement of one particular piece of hardware.

- **A scan without a pinned rung lies confidently.** The first run put the sensor two cells away: a bright row saturated, the ladder dropped a rung, and the dark reading taken beside the next frame was on a different scale — which came out as `-2.24 lx` for the row the sensor is actually in. `warn_saturated()` in the script now says when a reading is against the stop, and `find` pins the rung.

#### The loop, measured shut

`lab.py <clock> feedback` is the check every other one was leading up to: the worst face the clock can show — SIEBEN, the word under the sensor — swept over the whole regulated brightness range, in the clock's own colour at the drive values its own gamma produces, with a dark reading taken beside every frame so a passing cloud cannot be mistaken for a result.

```
Anz.     Wert    dunkel      hell    Modell      Rest   Fehler
   20%      7     8.318     8.290     0.097     8.193    -1.5%
   50%     55     8.318    10.383     2.154     8.229    -1.1%
  100%    255     8.290    22.801    14.891     7.911    -4.6%

Ohne Kompensation wandert der Messwert um +175.0 % ueber den Regelbereich.
Mit Kompensation bleibt er auf 4.6 % genau.
```

**+175 % in an unchanged room** is the loop: 0.44 decades of apparent light produced by nothing but the clock's own slider, which on the default line asks for 24 % more brightness, which produces more apparent light. Compensated, the same sweep moves 0.02 decades — under one per cent of brightness. That is the difference between a regulator that runs away and one that does not.

- **The residual drifts systematically negative with drive** (−1.5 % at 20 %, −4.6 % at 100 %), so the model over-predicts slightly at the top. It is not worth chasing: 4.6 % of lux is half a per cent of brightness, well under the one-per-cent step the setting is quantised to, and the sensor's own ladder spreads 3.8 % across its rungs.
- **A dark reading beside every frame, not one at the start.** The sweep takes about a minute and daylight is not constant over a minute; without it the last rows would have been measuring the weather.
- `base_colour()` reads what the driver actually writes at full brightness off the strip instead of computing it from hue and saturation. FastLED's rainbow wheel is not the HSV anyone would write down — this clock's fully saturated "yellow" comes out as `[171, 114, 0]`.

#### The clock will update the instrument away

The clock ran the published `edge` 2.1.1 the next morning, and `/lab/state` answered 404. **`autoUpdate` is on by the owner's choice and the edge rule is "differs", not "newer"** — so a locally flashed build carrying unpushed commits is replaced by the release at the next 02:00–05:00 window, every night, and the difference in version string guarantees it rather than preventing it.

Switch `autoUpdate` off for the duration of an experiment, or push the commits so the channel carries them. Switching it off is the honest choice while the work is unfinished; the alternative publishes a half-built lab interface to every clock on the channel.


#### What the infrared channel can and cannot do

The TSL2591 reports two channels, and CH1/CH0 is a property of the *source*: WS2812B put out almost no infrared, daylight puts out plenty. `lab.py <clock> ir [rung]` measures both ratios in one sweep — the room from a dark frame, the display from the increments over it, so what the room contributes cancels. Measured in moderate daylight:

| | CH1/CH0 | separation |
|---|---|---|
| the room (daylight through the panel) | 0.420 | — |
| display, white | 0.174 | 2.4x |
| display, red | 0.273 | 1.5x |
| display, green | 0.103 | 4.1x |
| display, blue | 0.055 | 7.7x |

Two channels and two known ratios do solve for the room without any per-cell map: `A0 = (rd*CH0 - CH1) / (rd - ra)`. **It is not usable, and the reason is which of the two ratios is unknown.** `rd` is ours and follows from the frame buffer, but `ra` is the room's — daylight 0.42, white LED lighting about 0.15, incandescent higher still — so the constant the method needs is exactly the thing that changes when the lighting changes. Measuring it means blanking the display, which is what the map exists to avoid. The channel is worth keeping as a **check** on the map, not as a replacement for it.

- **Half drive gave the same ratio as full** (0.174 both), which is the linearity this rests on, and it is also how a bad first run was caught: at `DEFAULT_RUNG` the white frame ran to 103 % of full scale, and clipping CH0 harder than CH1 reported 0.219. Hence the rung argument, and `warn_saturated()` in the run.

#### The clock outshines the daylight

Measured at midday, moderate daylight, through the panel:

```
daylight, display off                6.86 lx
SIEBEN at the clock's own setting   +3.80 lx   (lum 60, hue 60 -> pixel 56,37,0)
SIEBEN at full white               +32.98 lx
```

So the one word under the sensor adds **half the daylight again** at an ordinary evening setting, and nearly five times the daylight at full. The feedback loop is not a night-time problem that daylight drowns out. The same frame measured 32.4 lx yesterday on a different rung against a different room, so the map reproduces to under 2 % across days.

The compensation was checked against this: with the face showing ES IST ZWANZIG NACH ZWÖLF, the model predicted 0.6117 lx from the display and blanking it measured 0.6034 — **0.1 % apart**, at a colour (yellow, fully saturated) that was never calibrated as such. Superposition across the three channels is what makes that work.


#### The regulator, run from here

[scripts/regulate.py](scripts/regulate.py) is the whole automatic brightness on this side of the network: the compensation, the curve, the settle timer and the easing, driving a clock whose own automatic is switched **off**. It exists so the compensation can be tried against a real room before any of it is compiled in, and so a change to the algorithm costs a rerun rather than a flash cycle.

- **With the automatic off, the slider is the immediate response.** The firmware applies `Brightness` the moment `POST /color` lands, with no easing at all, so a hand on the slider reaches the LEDs without passing through the script. That is not a compromise for the simulation — it is the same feel the on-clock version has to produce through `Luminance::adjusting()`.
- **A nudge is noticed by remembering the last value written.** Anything else in `lum` is a hand on the slider. The log was the other candidate and is worse: with the automatic off the firmware logs nothing when the brightness changes, and text would have to be parsed to recover a number `/currentState` already hands over as a number. The exposure is one tick wide, and stays small because a settled regulator writes nothing at all.
- **`--zero` blinks the face dark for 600 ms and compares.** With no display there is no contribution, so this is the ground truth the compensation is checked against — better than the checkerboard pattern the idea started as, which would still have lit cells two away from the sensor and needed compensating in its turn. `--zero-every N` repeats it, which is the only way to validate the model during the hour when it matters.
- **The curve is a transcription of `Luminance.cpp`, not a fresh design** — same points, same 1.3 near-neighbour replacement, same 0.6 decades before a slope is fitted. It still anchors the offset on the newest point, which the firmware no longer does; a session run through it will converge where the clock averages. Worth fixing before the next long run, and worth knowing about in the meantime.
- `curve.json` beside `coupling.json`, per clock and gitignored for the same reason.
- **Six handlers answer `{msg: ''}` under `application/json`**, which is not JSON — an unquoted key and single quotes, left from the jQuery UI that never read the body. Parsing it is what broke the first live run, at the moment it first tried to correct anything.

First run on the clock, in a 1.2 lx room: eased 30 → 53 % on the default line, saw a slider move to 45 %, held it for ten seconds, learned `1.203 lx → 45 %` and settled at exactly 45. **No drift back** — the convergence failure the centroid intercept used to produce is visible here as its absence.

#### The guided run

`regulate.py <clock> lernen` walks a person through a calibration instead of leaving them to nudge and hope. It exists because the thing the curve is short of is invisible while making it: **spread**. Four corrections in one evening look like diligence and carry no slope at all — `lauf.csv` is exactly that, three points inside a factor of ten, all after 22:00.

So the run says what to do, point by point: how much the light has to change before the next point is worth taking, in which direction, and when there is enough. Written here first because it is the same script the on-clock calibration will have to speak later, and here it costs nothing to reword.

- **It asks for a factor, not a lux value.** "At least a factor of 4 below 9.3 lx" is something a person can act on — blinds, a lamp, a time of day. `LUM_FIT_MIN_DECADES` is the same requirement in the units the fit uses, and nobody can act on 0.6 decades.
- **A running readout while waiting for ENTER**, with the factor against the nearest existing point, so the decision is made against a number rather than against a feeling about how dark the room now looks. That needs the key press not to block, hence the `Prompt` thread — `input()` would freeze the display on the last number printed, which is the one being watched.
- **The light must be still before a point is taken.** A hand on the light switch is not a lighting condition; six seconds within 15 % is.
- **A point at 20 % or 100 % is flagged.** It says "at least this much", not "exactly this much", so it drags the slope flat. On this clock the first daylight point landed at 9.33 lx wanting 100 % — the whole bright end of the room is at the ceiling, which is a fact about a dim panel and not a fault, but the slope cannot be learned from it.
- **It teaches from the short average** (`TEACH_SECONDS`, 3 s), never the regulating one. That is the defect `lauf.csv` exposed and the reason the run exists in this shape: at 22:14 the room went to 0.0009 lx and the pair kept was `0.1184 lx -> 30 %`, a light level 148 times too high, straight into the slope. `lauf2.csv` the next day shows the fix working — the 30 s average read 7.64 while the point was taught from 9.33.

### Expert mode

The update, debug and storage tabs are locked behind a password. One flag in NVS says whether the clock is unlocked; while it is 0, `/ota/*`, `/log`, `/fs/*` and `/nvs/*` answer `403 {"error":"expertLocked"}` and the web UI does not offer the tabs. [src/Expert.cpp](src/Expert.cpp) owns all of it, and `Expert::guard()` is the single line at the top of every covered handler — fifteen of them, listed in the header so the list cannot quietly grow. The two streaming uploads (`/ota/upload`, `/fs/upload`) ask `Expert::unlocked()` themselves instead: they write into flash from a handler that cannot send a response, so guarding only the done handler would let a stranger overwrite something and be refused afterwards, which is not a refusal.

**How to get in: `http://<clock>/#expert`** — the address bar, since the screen has no chip in the tab row. That is the whole entrance; there is no other. What it offers depends on the state the clock reports through `GET /expert`:

| State | The screen shows | Effect |
|---|---|---|
| no password yet | one password field, minimum six characters | enrols it **and** unlocks — first come, first served |
| enrolled, locked | the same field | unlocks; the update and debug tabs appear |
| unlocked | a "lock again" button | locks, no password needed |
| five wrong answers | the field, disabled for five minutes | nothing gets through, right answer included |
| enrolled, within 5 min of a **power-on** | a second card, "forgotten", with a countdown | clears the enrolment — see the recovery bullet |

`#expert` is left behind on the way out, so reloading does not land back on it. Being unlocked survives a reboot; it is a flag in NVS, not a session.

- **It is a mode, not a login.** Setting the flag needs the password; clearing it does not, since someone locking the clock out of spite has gained nothing. HTTP Basic authentication was the first idea and is worse here for a concrete reason: the debug tab polls `/log` every two seconds, so Basic would put the password on the wire some 1800 times an hour with the tab open. One unlock puts it there once. The price is that while unlocked, nothing is protected — hence the visible "lock again" button.
- **The hash is made on the clock, never at build time.** A hash compiled into the image would be published with every release *and* would come back with every OTA update to overwrite the owner's own. This was the design's first version and its own author found the hole. NVS is the one store an update does not touch — the same reason the settings live there.
- **Its own NVS namespace (`qlockexpert`), not a field in the settings record.** Two reasons that both bite: that record is rewritten whole from `fillDocument()` on every settings change, so a field forgotten there is a field lost; and `getJSONSettings()` publishes exactly that shape through `/currentState`, where a password hash has no business being.
- **A fresh clock has no password and is locked.** The first password offered is the one that is kept. That race — whoever reaches it first owns the clock — is bounded and not a regression: an un-enrolled clock is exactly as open as every clock was before this existed, so enrolling can only improve matters.
- **The way back from a forgotten password is the plug.** `POST /expert {reset: true}` clears the enrolment, but only within `EXPERT_GRACE_MS` of a **power-on** reset. `esp_reset_reason()` tells that apart from the software restart the update tab triggers, so rebooting the clock from its own web UI does not open the window — and neither does a USB flash, which reports `ESP_RST_USB`. That this hands the clock to anyone who can pull the plug is not a weakness: without flash encryption, the same person reads the NVS out over USB anyway. Physical access already wins, and a recovery path that admits it beats one that pretends otherwise.
- Five wrong answers stop the endpoint taking any for five minutes, or a short password over HTTP is guessed in seconds. The comparison is constant-time. The password itself is never logged — the ring is what it guards.
- **What it is worth**: it keeps out someone who joins the network and goes looking. It does not keep out someone watching the traffic, since enrolment and unlock cross the wire in the clear, and there is no TLS on the clock.
- The tab row is built from `ALL_TABS` in `App.svelte` with the last two sliced off while locked, so `t.tabs` keeps all six entries in every locale either way. The expert screen deliberately has **no chip in the row** — there is nothing there for someone who has not gone looking, and a visible one would only invite guessing. Hence `#expert` above. `App.svelte` treats a clock that cannot answer `/expert` as locked, which is what an older firmware under a newer web UI looks like.
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

The tab also carries the state block the update history keeps pointing at: uptime, reset reason, and free / lowest-ever / largest-block heap. The brightness curves that were once meant to land here went to `#luminance` instead, outside the lock: the debug tab is behind expert mode and a brightness curve has no business needing a password.

#### The face in the colour tab

The preview beside the colour wheel is the **real face**: eleven letters by ten rows in the panel of the language that is running, lit exactly where the clock's own frame buffer says, with the four corner LEDs. It used to be three hardcoded lines per locale (`preview: ['ES IST', 'FÜNF NACH', 'ZWEI']`), which have been deleted.

- **The browser is not a second opinion.** `on` is read off `matrix[16]` itself, so what is on screen is what is on the wall — a wrong render included. Rebuilding the grammar in JavaScript would have been a second implementation to keep in step, and it would have hidden exactly the faults worth seeing.
- **`on` is `#` and `.`, not a bit mask.** `curl http://<clock>/panel` then shows two aligned grids, the letters and what is lit, which is worth more than the eighty bytes it costs on something whose whole purpose is visual inspection.
- **A coloured corner is not in the display colour, and the preview asks rather than guesses.** With the mode on, the newest corner walks the hue wheel with the seconds — red at :00 to blue at :59 — which is the whole point of it, and the preview drew all four in the plain display colour because `/panel` gave it no way to know better. `cornerColors` now carries what each one is actually showing, computed by the driver through `cornerHue()`. The colours arrive at full intensity and the browser blends them down exactly as it does the letters: how bright the clock is running is the preview's model, not the firmware's. The field is **absent**, not empty, when the mode is off — so an older web UI behaves as it did, and a newer one can tell "no colours" from "four dark corners" without a second flag. At a 5 s poll the moving corner steps rather than sweeps; the real one is on the wall.
- **The corners are reported separately and in reading order** — top left, top right, bottom right, bottom left. Which frame buffer row is which corner of the face is `matrix[1]` top left, `matrix[0]` top right, `matrix[3]` bottom right, `matrix[2]` bottom left, and it is written down once at `Renderer::setCorners`. **It cannot be derived from the code.** The driver's wiring comment says something else, and the path from a row to a pixel goes through two remappings that partly cancel — `writeScreenBufferToMatrix` sends `matrix[1]` to `_setPixel(110)`, and `_setPixel` swaps 110 with 112. Following that through gave a mapping that was wrong on the face; the order the corners actually light in, watched on the clock, gave the right one. Check that against a clock, not against the source.
- **Unlit letters are drawn faintly rather than hidden**, because on a real panel they are still there. A grid with no letters at all then means "no panel data" — a different thing from a clock that is switched off, and the two must not look alike.
- Polled every 5 s, not at the sensor's 2 s: the letters change every five minutes and the corners once a minute, and the clock answers one request at a time.
- The mock lit the corners in the order `3, 0, 1, 2` for a while — written before the mapping had been established against a real clock, and not revisited when the firmware was corrected. It is reading order now, and it honours `cornerDirection`.
- The mock renders standard German from the wall clock time. It is not a second renderer and does not try to be — it exists so the layout and the moving corners can be worked on without a clock.

#### The panel OpenSCAD cuts

[scripts/panels.py](scripts/panels.py) reads the ten rows out of every `Language_*.cpp` and writes [hardware/Qlock250mm/3dprint/panels.scad](hardware/Qlock250mm/3dprint/panels.scad), which [body.scad](hardware/Qlock250mm/3dprint/body.scad) includes to cut the letter mask.

- **The letters were written out twice, and only one copy was checked.** `body.scad` carried its own `display[y][x]` array; the firmware verifies its panels at every boot (`Languages::selfCheck()`) and a SCAD array gets none of that. The two happened to still agree when this was written — that is luck, not a system, and the failure mode is a physical panel milled with a letter in the wrong place.
- **Generated, committed, and never fetched or built on demand** — the same rule as `zones.json` and the icons. OpenSCAD cannot run Python, and a 3D print should not depend on a toolchain that happens to be installed. Regenerate with `python scripts/panels.py` after changing a panel; the output carries no timestamp, so a run with nothing changed leaves the file byte-identical.
- **Seven panels, ten languages.** The four German dialects share one, so the file emits one variable per *distinct* panel and a table mapping each language code onto it — writing the German letters out four times would invite exactly the drift this removes. `panel("de-DE")` looks one up; an unknown code gives `undef` rather than a wrong panel.
- The script **parses rather than compiles**, which is only safe because the shape is narrow: three string literals then a braced block of either ten strings or ten references into a shared array. Anything else raises. It also cross-checks its findings against `TABLE` in `Languages.cpp` and stops if a language is in one and not the other.
- **It checks two things the firmware cannot.** The **drawing in the header comment** of each language file, against the rows it claims to draw — it is a comment, so nothing has ever forced it to be true, and it had quietly disagreed three times. It is also the first thing anybody reads when adding a word, which is what makes a wrong one expensive. And the **enum against the WORDS array**: the names index into it by position, so if the two get out of step every `face.light()` in that language points at the wrong word, and nothing says so — the code compiles and the clock lights letters, just the wrong ones.
- **It runs `selfCheck` too, and refuses to write a panel that fails it.** The same comparison the firmware makes at boot — every word against the letters underneath it — done here because there is no host C++ compiler in this project and because the output of this script is what a milling machine cuts. A wrong panel found on the wall costs a sheet of aluminium; found here it costs nothing. This is what turned up the `Ò`/`Ľ` fakes described under "Languages".
- One element per **cell**, so an `O` and its prime are one entry in the SCAD array and OpenSCAD cuts them as one opening.
- Verified as a pure relocation by rendering `body.scad` before and after: identical vertex, edge and facet counts, identical sorted vertex list. **Note that the STL is not byte-reproducible** — two renders of the same file differ, because the facet order out of CGAL is not stable. Compare sorted vertices, not checksums.
- Two faults fell out of doing this. `body.scad` looped `x` over `[0:11]` for an eleven-column panel, asking for a twelfth letter that does not exist and drawing nothing one column outside the mask; and its `echo` printed `display[x][y]` where the line above it cut `display[y][x]`, so the debug output described a transposed panel. Both are gone.

### Vendored/generated content (not project source)

- `.pio/`, `dist/`, `compile_commands.json`, `idedata.json` — PlatformIO build cache and IDE tooling metadata, not hand-maintained.
- `web/public/zones.json`, `web/public/*.png`, `hardware/Qlock250mm/3dprint/panels.scad` — generated by the scripts in [scripts/](scripts/) and committed on purpose, so that neither a build nor a print needs Python. Edit the generator, not the output.

### Hardware consolidation

This project originally targeted several boards and LED drivers. It has been consolidated to a single target: Seeed XIAO ESP32-S3 + WS2812B. As part of that, the following were removed as dead/unreachable code:
- The `nodemcu-32s` and `esp32-c3-display` PlatformIO environments, and the 4 MB `partitions.csv`/`min_spiffs.csv` tables that only they referenced. (The current `partitions.csv` is unrelated — a new 16 MB table for the XIAO ESP32-S3 Plus, see above. The S3 environment never referenced a partition file before and relied on the board default.)
- `src/Configuration.h`, a large block of compile-time `#define` toggles for the original AVR/DCF77/multi-driver-era hardware (alarm, DCF77 receiver, alternate LED drivers, RTC chip selection, IR remote variants). It was already unreferenced by any active code path before removal.
- The `LedDriver` abstract base class, merged into `LedDriverWS2812FastLED` since it was the only implementation.
- The `RemoteDebugger` lib_dep and vendored `RemoteDebugApp/` web client (see "Debugging" above).
- `src/LDR.h`/`.cpp` and the `BH1750` lib_dep, replaced by the VEML7700 behind an interface (see "Light sensor"). This one was kept through the first consolidation and only went when something took its place.
