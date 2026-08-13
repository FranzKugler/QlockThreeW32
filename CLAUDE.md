# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

QlockThreeW32 is ESP32 firmware for a "word clock": a letter matrix backlit so the current time reads as a sentence (in German, Swiss German, English, French, Italian, Dutch, or Spanish), updated every 5 minutes, with four corner LEDs indicating the remaining minutes. It's a long-running hobby project (originally AVR-based, later ported to ESP32) and carries legacy cruft from that history — expect dead code paths, commented-out alternatives, and compile-time options for hardware that is no longer used.

The repo has three parts:
- **Firmware** (`src/`) — PlatformIO/Arduino C++ for the ESP32, drives the LED matrix and hosts a small config web server.
- **Web UI** (`data/`) — static HTML/JS/CSS flashed to the ESP32's LittleFS filesystem; this is the config page served by the firmware's web server.
- **Mock server** (`server.js`) — a Node/Express stand-in for the firmware's REST API, used to develop `data/` on a desktop browser without hardware.

Note: this repository currently has no git commits (fresh `master` branch).

## Commands

### Firmware (PlatformIO)

There is no `pio` CLI on PATH in this environment — development normally happens through the PlatformIO VSCode extension. If the CLI is available elsewhere, the equivalent commands are:

```
pio run                          # build (uses default_envs from platformio.ini)
pio run -e seeed_xiao_esp32s3    # build a specific environment
pio run -t upload                # build and flash firmware
pio run -t uploadfs              # build and flash the data/ folder as a LittleFS image
pio device monitor                # serial monitor (115200 baud)
```

There are no automated tests in this project.

Build environments are defined in [platformio.ini](platformio.ini):
- `seeed_xiao_esp32s3` — the active default (`default_envs`), `LED_OUTPUT_PIN=4`, USB serial upload.
- `nodemcu-32s` — older board, OTA upload over WiFi (`espota`, port 8266, password `admin`) instead of serial.
- `esp32-c3-display` — Seeed XIAO ESP32-C3 variant, `LED_OUTPUT_PIN=4`, USB serial upload.

Only one env is active at a time; switch by editing `default_envs` or passing `-e <env>`.

### Web UI mock server

`server.js` is a plain Express app (no `package.json` in the repo — dependencies exist under `node_modules/` but must already be installed; `npm install` will not resolve them from scratch here). It serves `data/` statically and mocks the same REST endpoints the firmware exposes, so the config page can be developed without a physical clock.

```
node server.js       # serves data/ and mock API on port 8080
npx nodemon          # auto-restart on changes to server.js and data/ (see nodemon.json)
```

## Architecture

### Firmware request/render loop

[src/main .cpp](src/main%20.cpp) (note the literal space in the filename) is the entry point. `setup()` mounts LittleFS, loads `Settings`, connects WiFi via `WiFiManager` (falls back to a `QlockThreeW32` AP for first-time config), starts NTP, mDNS, ArduinoOTA, RemoteDebug, and the `WebServer` on port 80. `loop()` re-syncs WiFi/NTP as needed, services the web server and OTA, and once per second rebuilds `matrix[16]` (the 16-row bitmask framebuffer) according to `mode` and pushes it to the LED driver. `mode` selects between normal time display, a seconds-counter debug view, an LED test pattern, an uptime/DCF-sync debug view, etc. (see the `STD_MODE_*` / `EXT_MODE_*` defines near the top of the file).

Settings changes made through the REST API mark `needsUpdateFromRtc = true` and schedule a deferred flash write (`timeToSaveToFLASH`, `WAIT_BEFORE_SETTINGS_WRITE` seconds later) rather than writing on every request, to limit flash wear.

### REST API (firmware and mock server share the same contract)

Both [src/main .cpp](src/main%20.cpp) and [server.js](server.js) implement the same endpoints, consumed by [data/main.js](data/main.js):
- `GET /currentState` — JSONP; returns the full current settings object.
- `POST /display` — display mode.
- `POST /color` — hue/saturation/luminance + auto-luminance flag.
- `POST /autoluminance` — toggle LDR-based auto brightness.
- `POST /configuration` — language, corner LED direction/color.
- `POST /timezone` — NTP server + manual DST/timezone rule fields.

When changing this API, update both implementations and `data/main.js`/`data/index.html` together.

### Rendering pipeline (hardware-independent core)

`Renderer` ([src/Renderer.cpp](src/Renderer.cpp)) is pure logic with no hardware dependency: given hour/minute/language it sets word bits in the `word matrix[16]` framebuffer. Per-language word-to-bitmask macros live in `Woerter_<LANG>.h` (`Woerter_DE.h`, `Woerter_CH.h`, `Woerter_EN.h`, `Woerter_FR.h`, `Woerter_IT.h`, `Woerter_NL.h`, `Woerter_ES.h`; `Woerter_DE_MKF.h` exists but is currently unused/commented out) — each language has its own irregular grammar handled as a switch on `minutes / 5` plus special-casing (e.g. French/Italian/Spanish hour agreement, Swabian/Bavarian/Swiss `viertel`/`dreiviertel` variants). `Renderer::setCorners` sets the four corner-LED bits for the sub-5-minute remainder, in clockwise or counter-clockwise order. `Zahlen.h`/`Staben.h` hold digit/letter bit patterns used by the debug display modes (seconds, uptime, DCF-sync-age) rather than by the word renderer.

### LED output

`LedDriver` ([src/LedDriver.h](src/LedDriver.h)) is an abstract base (legacy: designed to allow swapping in shift-register/MAX7219/etc. drivers). The only concrete implementation in active use is `LedDriverWS2812FastLED` ([src/LedDriverWS2812FastLED.cpp](src/LedDriverWS2812FastLED.cpp)), which drives a 114-pixel WS2812B strip via FastLED, wired serpentine with the corner LEDs fed separately (see the wiring diagram in the header's comment). It owns HSV color, brightness scaling, and corner-color/animation state, and converts the `matrix[16]` bitmap to physical pixel writes in `writeScreenBufferToMatrix`.

### Settings persistence

`Settings` ([src/Settings.h](src/Settings.h)/[src/Settings.cpp](src/Settings.cpp)) holds all user-configurable state (language, corner rendering, brightness, color, LDR use, mode, NTP server, and both standard/DST timezone rules) and (de)serializes it to/from LittleFS as JSON, plus exposes `getJSONSettings()` for the REST API response.

### `Configuration.h`

[src/Configuration.h](src/Configuration.h) is a large block of compile-time `#define` toggles inherited from the original AVR/DCF77/multi-driver-era project (alarm, DCF77 receiver tuning, alternate LED drivers, RTC chip selection, IR remote variants). Most of these do not apply to the current ESP32/WS2812B/NTP build — check whether a given `#define` is actually referenced (`LED_DRIVER_WS8212B` and NTP-related paths are the ones in active use) before assuming it affects behavior.

### Vendored/generated content (not project source)

- `RemoteDebugApp/` — a vendored offline copy of the third-party RemoteDebug web console (for use with the `RemoteDebug` telnet-style debug library); not maintained as part of this project.
- `.pio/`, `dist/`, `compile_commands.json`, `idedata.json` — PlatformIO build cache and IDE tooling metadata, not hand-maintained.
- `partitions.csv` / `min_spiffs.csv` — flash partition tables referenced by `platformio.ini` (`board_build.partitions`).
