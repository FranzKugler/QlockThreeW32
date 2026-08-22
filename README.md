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
- **Configuration in the browser**, served from the clock. The interface
  switches its own language along with the clock's.
- **Colour** as hue, saturation and brightness, beside a live preview of the
  real face — the lit letters are read back out of the clock's own frame
  buffer, not guessed at in the browser.
- **Automatic brightness** from an ambient light sensor, on a curve the clock
  learns from the way you use the slider. There is no "remember this" button.
- **Time from NTP**, with a region/place picker over both daylight-saving
  changeover rules, which stay editable by hand underneath.
- **Firmware and interface updates** from the browser, or automatically from a
  release channel.
- **Expert mode** — a password in front of the update, debug and storage tabs.
- A few debug displays: seconds counter, LED test pattern.

## Hardware

| | |
|---|---|
| Controller | Seeed XIAO ESP32-S3 **Plus** (16 MB flash) |
| LEDs | 114 × WS2812B on one strip, data on GPIO 4 |
| Layout | 11 × 10 letters = 110, plus 4 corner LEDs |
| Light sensor | ams **TSL2591** (0x29) or Vishay **VEML7700** (0x10) on I²C, SDA/SCL default to GPIO 5/6 — optional |

The strip is fed in at the top left and runs serpentine downwards, with the
four corner LEDs last.

> The wiring comment in
> [`src/LedDriverWS2812FastLED.h`](src/LedDriverWS2812FastLED.h) names a corner
> order that is **not** the one the clock lights. The mapping that holds is
> written down at `Renderer::setCorners`, and it was established by watching a
> real clock rather than by reading the code — the path from a frame buffer row
> to a pixel goes through two remappings that partly cancel.

**Either light sensor works, and the choice is made at run time** — the firmware
tries each in turn at boot and keeps the first that answers, so one build
serves every clock and a chip swapped on the bench needs no rebuild. The
TSL2591 is asked first: the two do not share an address, so a clock carrying
both answers twice, and the more sensitive one is the one to keep. With no
sensor fitted, the automatic hides itself rather than offering a switch that
does nothing.

The board and the case are in [`hardware/`](hardware/) — a KiCad project and an
OpenSCAD model. The letter mask is **not** drawn by hand: `scripts/panels.py`
reads the ten rows out of the firmware's own language files and writes
`panels.scad`, so the sheet that gets milled and the words the clock lights
cannot drift apart.

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

The clock can be renamed in the WLAN tab, which changes all six places the name
appears — mDNS, the DHCP name the router lists, the setup access point, the
espota target, the RemoteDebug host and the heading of the interface. It
restarts to do it, and says so.

Four tabs are open to anyone on the network:

| Tab | |
|---|---|
| Display | operating mode, language, corner LED direction and colour |
| Colour | hue, saturation, brightness, the automatic, and a preview of the face |
| Time zone | NTP server, a region/place picker, and both changeover rules |
| WLAN | status, scan, switching networks, renaming the clock |

Switching networks is deliberately careful: the clock tries the new
credentials, and if they do not come up within twenty seconds it returns to the
previous network by itself, so a typo cannot lock you out.

## Expert mode

Three more tabs — **Update**, **Debug** and **Storage** — are behind a
password, because flashing the clock, reading its log and rummaging through its
filesystem are not things every guest on the network should be able to do.

**The way in is the address bar: `http://<clock>/#expert`.** There is no chip
in the tab row, on purpose; a visible one would only invite guessing.

- A clock that has never been given a password takes the first one offered, and
  keeps it. Until then it is exactly as open as every clock was before this
  existed.
- It is a **mode, not a login**: one flag in NVS, so the password crosses the
  wire once instead of on every poll of the log. While unlocked, nothing is
  protected — hence the visible "lock again" button.
- **The way back from a forgotten password is the plug.** The enrolment can be
  cleared within five minutes of a *power-on* reset, which a restart from the
  clock's own update tab does not count as. Anyone who can pull the plug can
  read the flash over USB anyway, so this admits what is already true rather
  than pretending otherwise.
- Five wrong answers stop the endpoint for five minutes.

What it is worth: it keeps out someone who joins the network and goes looking.
It does not keep out someone watching the traffic — there is no TLS on the
clock — and it does not keep out anyone holding it.

Once unlocked:

| Tab | |
|---|---|
| Update | installed versions, browser upload, release channel and automatic installs |
| Debug | uptime, reset reason, heap, and the clock's last 200 log lines |
| Storage | a file explorer over LittleFS, and over NVS drawn as a tree |

Two more screens have no chip either. `#luminance` draws what the automatic
brightness has learned — deliberately *outside* expert mode, since there is no
secret in a brightness curve. `#expert` is the one above.

## Automatic brightness

Switch it on in the colour tab and the display follows the room. The curve is a
straight line in **log light**, because perception is roughly logarithmic and
the range to cover spans decades:

```
brightness = slope × log10(lux) + offset      held between 20 % and 100 %
```

**There is no "remember this" button, and that is the design.** Moving the
slider is the only signal anyone ever gives about whether the automatic got it
right, so the nudge *is* the calibration:

1. Move the brightness slider while the automatic is on. The automatic steps
   aside at once — nobody wants the clock arguing back mid-drag.
2. Ten seconds after the last move, the clock keeps the pair *(light now,
   brightness asked for)* and fits a new line through everything it has kept.

The switch keeps saying "automatic" throughout, because it still is: it is
being taught, not turned off. The timer is in the firmware, so closing the tab
mid-adjustment does not lose the point.

Ten points are kept, in NVS. A new one **replaces a near neighbour** rather
than joining the queue, or ten corrections made in one evening would push the
one daylight point out and collapse the line onto a single lighting condition.
If the points sit too close together to say anything about steepness, only the
level is re-fitted and the slope stands. A slope that would make a darker room
brighter is refused outright.

The reset button in the colour tab throws it all away and restores a cautious
default line.

## The two stores

The clock keeps things in two places, and the difference decides what survives
an update:

- **LittleFS**, 3.5 MB — the web interface, the timezone list, the icons. A
  filesystem update **overwrites it whole**.
- **NVS** — the settings, the expert password, the brightness curve. An update
  leaves it **untouched**, which is exactly why they live there.

The **Storage** tab shows both. NVS is not a filesystem and is drawn as one
anyway: namespaces as folders, keys as files, `.json` or `.bin` depending on
what the value turns out to be. Files can be downloaded, uploaded and edited in
place, JSON laid out on the way in and written back compact. Right-click, or
press and hold on a touch screen, for the menu.

Two things it will not do. The expert password hash is listed but never handed
out — it is the one secret whose leak would outlive the unlock that leaked it.
And deleting is not recursive, because a file explorer that empties a tree on
one click is how the web interface gets deleted by someone who meant to tidy
up.

> Clearing NVS resets the clock to defaults. Reflashing the filesystem no
> longer does.

## Updating over the air

There are two images, and they are independent: `firmware.bin` (the program)
and `littlefs.bin` (the web interface). Changing the interface and only
flashing the firmware leaves the old interface in place.

Ready-built images come from
[the releases page](https://github.com/FranzKugler/QlockThreeW32/releases), in
two channels:

- **stable** — tagged versions, built from a `v*` tag.
- **edge** — a rolling build of the latest commit on `main`, replaced on every
  push. Untested by definition.

The clock can poll its channel by itself and install what it finds. Automatic
installs are off by default and only ever run between 02:00 and 05:00 local, so
the face never goes dark in the evening.

Locally the images are produced by `pio run` and `pio run -t buildfs` in
`.pio/build/seeed_xiao_esp32s3/`.

Upload either image in the **Update** tab — the clock recognises which is which
from the image itself (ESP32 program images start with the magic byte `0xE9`)
and picks the right partition.

The checksum is verified before the boot partition is switched, so an
interrupted upload does no harm: the clock simply keeps running the version it
has. There is no automatic rollback for an image that flashes correctly but
then crashes — that case needs a USB cable.

**The update endpoints are behind expert mode**, along with the log and the
storage tab. They were open by design while none of them existed as a web page.

## Layout of the repository

```
src/            firmware (PlatformIO / Arduino-ESP32)
  Renderer.*      hour/minute -> which words light up, no hardware involved
  languages/      one file per language: its letters, its words, its grammar
  LedDriver*      WS2812B output, colour and brightness
  LightSensor.*   TSL2591 / VEML7700 behind one interface, sampled on core 0
  Luminance.*     the brightness curve, and how it is learned
  Settings.*      persistence in NVS
  WebRoutes.*     the HTTP handlers; OtaUpdate, FileRoutes, NvsRoutes beside it
web/            configuration interface (Svelte 5 + Vite), built into data/
hardware/       KiCad project, and the OpenSCAD case and letter mask
scripts/        generators: the letter mask, the timezone list, the app icons
server.js       mock of the clock's REST API, for developing the UI without hardware
```

Everything in `scripts/` writes files that are **committed**, so neither a
build nor a 3D print needs Python. Edit the generator, never its output.

For UI work, run the mock and the dev server side by side — the interface then
behaves as it does on the device, on a desktop browser:

```sh
npm run mock     # REST API on :8080
npm run dev      # Vite with hot reload on :5173
```

[`CLAUDE.md`](CLAUDE.md) goes into the architecture in more detail.

## Origins

This is a long-running hobby project, and not the beginning of the story. It
descends from **Qlockthree** by *Christian Aschoff* — firmware for a
self-built QLOCKTWO-style clock, written for the Arduino/ATmega328 from
November 2011 onwards and maintained through version 3.4.9. Much of the
rendering code here is still his, as the file headers record.

- Project and downloads: <http://www.christians-bastel-laden.de/DOWNLOADS/index.html>
- Shop and build instructions: <http://www.christians-bastel-laden.de/>

The upstream firmware is not on GitHub itself; it is published as ZIP archives
on that page. Several people have mirrored and continued it, among them
[bracci/Qlockthree](https://github.com/bracci/Qlockthree),
[schwabe/qlockthree](https://github.com/schwabe/qlockthree) and
[cactus-online/Qlockthree](https://github.com/cactus-online/Qlockthree).
[bracci/QlockWiFive](https://github.com/bracci/QlockWiFive) and
[ch570512/Qlockwork](https://github.com/ch570512/Qlockwork) are separate ESP
firmwares for the same hardware idea.

Beyond that, the Swiss German variant goes back to *Thomas Schuler* and the
Dutch one to *Rudolf Klimesch*.

This repository takes that lineage to an ESP32-S3, and adds the web interface,
the WiFi handling and the over-the-air updates.

## License

**CC BY-NC-SA 3.0** — Attribution, NonCommercial, ShareAlike. See
[`LICENSE`](LICENSE).

This is inherited, not chosen: Christian Aschoff puts the original firmware
under CC BY-NC-SA 3.0, and the ShareAlike term carries over to anything built
on it. In short — keep the attributions, do not use it commercially, and pass
derivatives on under the same terms.
