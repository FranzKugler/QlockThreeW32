# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

QlockThreeW32 is ESP32 firmware for a "word clock": a letter matrix backlit so the current time reads as a sentence (in German, Swiss German, English, French, Italian, Dutch, or Spanish), updated every 5 minutes, with four corner LEDs indicating the remaining minutes. It's a long-running hobby project (originally AVR-based, later ported to ESP32) and carries some legacy cruft from that history — expect dead code paths and commented-out alternatives, though the project has been consolidated onto a single hardware target (see below).

The repo has three parts:
- **Firmware** (`src/`) — PlatformIO/Arduino C++ for the ESP32, drives the LED matrix and hosts a small config web server.
- **Web UI** (`data/`) — static HTML/JS/CSS flashed to the ESP32's LittleFS filesystem; this is the config page served by the firmware's web server.
- **Mock server** (`server.js`) — a Node/Express stand-in for the firmware's REST API, used to develop `data/` on a desktop browser without hardware.

The target hardware is fixed: a Seeed XIAO ESP32-S3 driving a 114-pixel WS2812B strip. Support for other boards (NodeMCU-32S, ESP32-C3) and other LED drivers/peripherals from the project's history has been removed — see "Consolidation history" below.

## Commands

### Firmware (PlatformIO)

`pio` is not on PATH in this environment, but is installed via the PlatformIO VSCode extension at `%USERPROFILE%\.platformio\penv\Scripts\pio.exe`. Invoke it by full path (or add that directory to PATH):

```
pio run                          # build (uses default_envs from platformio.ini)
pio run -e seeed_xiao_esp32s3    # build a specific environment
pio run -t upload                # build and flash firmware
pio run -t uploadfs              # build and flash the data/ folder as a LittleFS image
pio device monitor                # serial monitor (115200 baud)
```

There are no automated tests in this project.

[platformio.ini](platformio.ini) defines a single build environment, `seeed_xiao_esp32s3` (`LED_OUTPUT_PIN=4`, USB serial upload via esptool at 460800 baud).

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

`LedDriverWS2812FastLED` ([src/LedDriverWS2812FastLED.cpp](src/LedDriverWS2812FastLED.cpp)/[.h](src/LedDriverWS2812FastLED.h)) is the sole, concrete LED driver (an earlier `LedDriver` abstract base for swapping in other drivers was folded into this class — there is no longer an interface to implement). It drives a 114-pixel WS2812B strip via FastLED, wired serpentine with the corner LEDs fed separately (see the wiring diagram in the header's comment). It owns HSV color, brightness scaling, and corner-color/animation state, and converts the `matrix[16]` bitmap to physical pixel writes in `writeScreenBufferToMatrix`.

### Settings persistence

`Settings` ([src/Settings.h](src/Settings.h)/[src/Settings.cpp](src/Settings.cpp)) holds all user-configurable state (language, corner rendering, brightness, color, LDR use, mode, NTP server, and both standard/DST timezone rules) and (de)serializes it to/from LittleFS as JSON, plus exposes `getJSONSettings()` for the REST API response.

### Light sensor (currently unused, kept for potential future wiring)

`LDR`/`BH1750` ([src/LDR.h](src/LDR.h)/[src/LDR.cpp](src/LDR.cpp)) supports an optional BH1750 light sensor for automatic brightness. It is intentionally not instantiated: the `LDR ldr;` declaration and the brightness-adjustment block in `main .cpp`'s `loop()` are commented out. The `automaticLum`/`UseLdr` setting still exists end-to-end (UI checkbox, `/autoluminance` REST endpoint, `Settings`), but currently has no effect — toggling it doesn't do anything until the LDR is wired back in.

### Debugging

`RemoteDebug` (telnet-style remote log console, `debugI`/`debugW`/`debugE`/`debugA` macros used throughout `main .cpp`) is the only debug facility in the project. Its companion GUI library, `RemoteDebugger` (variable watch/manipulation via a web console), was already inert before consolidation — the include and its init calls were commented out in `main .cpp` — and has been removed from `lib_deps` entirely, since it no longer compiles against the current ESP32 Arduino core (`std::byte` ambiguity in its vendored source). Its vendored web client, `RemoteDebugApp/`, was removed with it. A browser-based log console (e.g. the WebSerial library) was considered as a replacement but rejected: it requires migrating the whole web server from the synchronous `WebServer` used here to `ESPAsyncWebServer`/`AsyncTCP`, and current WebSerial releases are AGPL-3.0-licensed.

### Vendored/generated content (not project source)

- `.pio/`, `dist/`, `compile_commands.json`, `idedata.json` — PlatformIO build cache and IDE tooling metadata, not hand-maintained.

### Hardware consolidation

This project originally targeted several boards and LED drivers. It has been consolidated to a single target: Seeed XIAO ESP32-S3 + WS2812B. As part of that, the following were removed as dead/unreachable code:
- The `nodemcu-32s` and `esp32-c3-display` PlatformIO environments, and the `partitions.csv`/`min_spiffs.csv` partition tables that only they referenced.
- `src/Configuration.h`, a large block of compile-time `#define` toggles for the original AVR/DCF77/multi-driver-era hardware (alarm, DCF77 receiver, alternate LED drivers, RTC chip selection, IR remote variants). It was already unreferenced by any active code path before removal.
- The `LedDriver` abstract base class, merged into `LedDriverWS2812FastLED` since it was the only implementation.
- The `RemoteDebugger` lib_dep and vendored `RemoteDebugApp/` web client (see "Debugging" above).

The BH1750 light sensor support (see above) was deliberately left in place, unlike the rest of the legacy hardware options.
