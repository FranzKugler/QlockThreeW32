# QlockThreeW32

ESP32 firmware for a **word clock** — a grid of letters, backlit so that the
current time reads as a sentence. The display changes every five minutes; four
corner LEDs fill in the minutes in between.

```
ES IST FÜNF NACH ZWEI          IT IS FIVE PAST TWO
```

Ten languages and dialects, a configuration web interface served by the clock
itself, and firmware updates through the browser. No cloud, no app, no account
— everything the clock needs it carries, and it works on a network with no
internet access at all.

---

## What it does

- **Ten languages**: German (standard, Swabian, Bavarian, Saxon), Swiss German,
  English, French, Italian, Dutch, Spanish. Each has its own irregular grammar
  — French and Italian agree the hour, the German dialects differ on
  *viertel* / *dreiviertel*.
- **Configuration in the browser**, served from the clock: display mode,
  colour, time zone, WiFi and firmware update. The interface switches its own
  language along with the clock's.
- **Colour** as hue, saturation and brightness, with a live preview of how the
  lit letters will read against the face.
- **Time from NTP**, with both daylight-saving changeover rules editable by
  hand rather than a fixed European default.
- **Firmware update from the browser** — pick a `.bin`, the clock works out
  where it belongs and reboots into it.
- A few debug displays: seconds counter, LED test pattern, uptime.

## Hardware

| | |
|---|---|
| Controller | Seeed XIAO ESP32-S3 **Plus** (16 MB flash) |
| LEDs | 114 × WS2812B on one strip, data on GPIO 4 |
| Layout | 11 × 10 letters = 110, plus 4 corner LEDs |
| Optional | BH1750 light sensor for automatic brightness (supported, not wired up) |

The strip is fed in at the top left and runs serpentine downwards; the four
corner LEDs come last, in the order bottom left, top left, top right, bottom
right. See the comment in
[`src/LedDriverWS2812FastLED.h`](src/LedDriverWS2812FastLED.h).

Earlier revisions supported other boards and LED drivers; the project has since
been consolidated onto this single target.

## Building

Two halves are built separately: the firmware, and the web interface that is
flashed into the clock's filesystem.

```sh
# web interface -> data/
npm install
npm run build

# firmware
pio run                  # build
pio run -t upload        # flash the firmware
pio run -t uploadfs      # flash data/ as the filesystem image
```

Both have to be flashed once over USB. After that, updates can go through the
browser.

> The board definition in PlatformIO describes the 8 MB XIAO ESP32-S3. This is
> the Plus with 16 MB, so [`platformio.ini`](platformio.ini) overrides the
> flash size and points at [`partitions.csv`](partitions.csv): two 6.5 MB OTA
> slots and a 3.5 MB filesystem.

### Credentials

Copy [`src/Secrets.example.h`](src/Secrets.example.h) to `src/Secrets.h` and
put in your own values — it is not in version control. The build works without
it, it just leaves network flashing via espota unprotected and says so with a
compiler warning.

## First start

With no WiFi configured the clock opens an access point called
**QlockThreeW32**. Connect to it and a captive portal asks for the network.
Afterwards the clock is reachable at **http://qlockthreew32.local** or by its
address in the network.

The configuration interface has five tabs:

| Tab | |
|---|---|
| Display | operating mode, language, corner LED direction and colour |
| Colour | wheel and sliders for hue, saturation, brightness, with a preview |
| Time zone | NTP server and both changeover rules |
| WiFi | status, scan, switching networks |
| Update | installed versions and firmware upload |

Switching networks is deliberately careful: the clock tries the new
credentials, and if they do not come up within twenty seconds it returns to the
previous network by itself, so a typo cannot lock you out.

## Updating over the air

There are two images, and they are independent: `firmware.bin` (the program)
and `littlefs.bin` (the web interface). Changing the interface and only
flashing the firmware leaves the old interface in place.

Both are produced by `pio run` and `pio run -t buildfs` in
`.pio/build/seeed_xiao_esp32s3/`. Upload either one in the **Update** tab — the
clock recognises which is which from the image itself (ESP32 program images
start with the magic byte `0xE9`) and picks the right partition.

The checksum is verified before the boot partition is switched, so an
interrupted upload does no harm: the clock simply keeps running the version it
has. There is no automatic rollback for an image that flashes correctly but
then crashes — that case needs a USB cable.

Settings live in NVS, a partition of its own, and survive both kinds of update.

**The upload endpoint has no authentication.** Anyone on the same network can
flash the clock through it. That is a deliberate choice for a device on a home
network; if it does not suit yours, a `server.authenticate()` in the two
handlers is the whole change.

## Layout of the repository

```
src/            firmware (PlatformIO / Arduino-ESP32)
  Renderer.*      hour/minute -> which words light up, no hardware involved
  Woerter_*.h     the letter grid and word positions, one file per language
  LedDriver*      WS2812B output, colour and brightness
  Settings.*      persistence in NVS
web/            configuration interface (Svelte 5 + Vite), built into data/
server.js       mock of the clock's REST API, for developing the UI without hardware
```

For UI work, run the mock and the dev server side by side — the interface then
behaves as it does on the device, on a desktop browser:

```sh
npm run mock     # REST API on :8080
npm run dev      # Vite with hot reload on :5173
```

[`CLAUDE.md`](CLAUDE.md) goes into the architecture in more detail.

## Origins

This is a long-running hobby project. It began as
**Qlockthree** by *Christian Aschoff*, written for AVR microcontrollers from
2011 onwards, and was later ported to the ESP32. Much of the rendering code is
still his, as the file headers record.

The Swiss German variant goes back to *Thomas Schuler*, the Dutch one to
*Rudolf Klimesch*.

## License

Not yet settled — see the file headers for authorship. If you want to reuse
anything here, please ask first.
