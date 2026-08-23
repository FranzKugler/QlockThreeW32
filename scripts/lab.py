# -*- coding: utf-8 -*-
"""Client for the clock's /lab interface, and the experiments run through it.

The clock can see its own face: on the clock this was written for, taking the
display from 20 % to 100 % moved the light sensor from 0.42 lx to 16.79 lx in
an unchanged room. That closes a positive feedback loop and poisons everything
the automatic brightness learns. How much, from which cells, and whether the
infrared channel escapes it are questions with numbers for answers, and this is
what asks them.

Nothing here needs a rebuild of the firmware. That is the point: /lab is a set
of primitives, the experiments live on this side, and a new idea costs a script
rather than a flash cycle.

    python scripts/lab.py <address> state
    python scripts/lab.py <address> find      # which cell is the sensor in
    python scripts/lab.py <address> ir        # does the infrared channel escape
    python scripts/lab.py <address> map       # coupling, cell by cell
    python scripts/lab.py <address> calibrate # the compensation, three per cell
    python scripts/lab.py <address> check     # predicted against measured, live
    python scripts/lab.py <address> feedback # sweep the display, is the loop shut
    python scripts/lab.py <address> upload   # put the map on the clock
    python scripts/lab.py <address> wiring    # cell to pixel, against the strip
    python scripts/lab.py <address> off       # give the strip back

Cover the clock for `find` and `map`: with no ambient light the measured
difference is the display's own contribution and nothing else. Every sweep
takes a dark reading beside each frame anyway, so slow drift cancels - but
starting from darkness makes the numbers larger than the noise.
"""
import io
import json
import os
import sys
import time
import urllib.error
import urllib.request

ROWS = 10
COLUMNS = 11
# The corners are not cells; they sit after the letters, in the order they are
# wired. See LabRoutes.cpp.
CORNERS = {"bottomRight": 110, "topRight": 111, "topLeft": 112, "bottomLeft": 113}

# Where the ladder is parked for a scan. Every reading in one run has to be on
# the same rung or the differences mean nothing, and this one holds a single
# white cell seen from an adjacent cell without running out of scale.
DEFAULT_RUNG = 4

# Where the calibration runs. Rung 4 holds the strongest cell just under
# saturation (about 80 % of full scale) and still resolves a cell two away, so
# every coefficient comes off one scale and no stitching is needed.
COARSE_RUNG = 4


class Lab(object):
    """The clock, as far as an experiment needs it."""

    def __init__(self, host, timeout=120):
        self.base = "http://%s" % host.rstrip("/").replace("http://", "")
        self.timeout = timeout

    def _call(self, path, body=None):
        url = self.base + path
        data = None
        headers = {}
        if body is not None:
            data = json.dumps(body).encode("utf-8")
            headers["Content-Type"] = "application/json"
        request = urllib.request.Request(url, data=data, headers=headers)
        try:
            with urllib.request.urlopen(request, timeout=self.timeout) as answer:
                return json.loads(answer.read().decode("utf-8"))
        except urllib.error.HTTPError as failure:
            detail = failure.read().decode("utf-8", "replace")
            raise SystemExit("%s -> HTTP %d %s" % (path, failure.code, detail))

    # ------ the four primitives ------

    def post_raw(self, path, body):
        """POST, and deliberately do not read the answer.

        Six handlers in WebRoutes.cpp still send the literal `{msg: ''}` under
        an application/json content type, left from the jQuery UI that never
        looked at it. It is not JSON and must not be parsed.
        """
        data = json.dumps(body).encode("utf-8")
        request = urllib.request.Request(self.base + path, data=data,
                                         headers={"Content-Type": "application/json"})
        urllib.request.urlopen(request, timeout=self.timeout).read()

    def state(self):
        return self._call("/lab/state")

    def take(self, on=True):
        return self._call("/lab/mode", {"on": on})

    def leds(self, sets, clear=True, show=True):
        return self._call("/lab/leds", {"clear": clear, "set": sets, "show": show})

    def sensor(self, rung=None):
        path = "/lab/sensor" + ("?rung=%d" % rung if rung is not None else "")
        return self._call(path)

    def sweep(self, frames, settle_ms=80, dark=False, rung=None):
        body = {"frames": frames, "settleMs": settle_ms, "dark": dark}
        if rung is not None:
            body["rung"] = rung
        return self._call("/lab/sweep", body)


def white(cells):
    """A frame lighting these cells at full white."""
    return {"clear": True, "set": [{"cell": list(c), "rgb": [255, 255, 255]} for c in cells]}


def signal(frame):
    """A frame's own contribution: lit minus the dark reading taken beside it."""
    if "dark" not in frame:
        return frame["lit"]["lux"]
    return frame["lit"]["lux"] - frame["dark"]["lux"]


def warn_saturated(result):
    """Says so when a reading ran out of scale rather than letting it pass.

    A pinned rung cannot range away from a bright frame, which is the point -
    but it means saturation has to be noticed here instead. Full scale is 36863
    counts at 100 ms and 65535 above that; anything near it is a floor, not a
    measurement, and the cell it belongs to may be brighter than it looks.
    """
    worst = 0
    for frame in result["frames"]:
        reading = frame["lit"]
        if "ch0" in reading:
            ceiling = 36863 if reading.get("ms", 200) == 100 else 65535
            worst = max(worst, reading["ch0"] / float(ceiling))
    if worst > 0.9:
        print("  WARNUNG: bis %.0f %% der Vollskala - eine Sprosse unempfindlicher"
              % (100 * worst))


# ------ the experiments ------

def find_sensor(lab, settle_ms=120, rung=DEFAULT_RUNG):
    """Which cell the sensor sits behind, in 21 measurements rather than 110.

    Rows first, then columns: the sensor's row is the one that couples most,
    and likewise its column, and the two cross at the cell. It also answers a
    second question on the way - if every row couples about equally, the light
    is not reaching the sensor cell by cell but through the front sheet, and a
    coupling map per cell would be measuring noise.

    **The rung has to be pinned**, and this is not a detail. Run without it,
    the first version of this put the sensor two cells away from where it is:
    a bright row saturates, the ladder drops a rung, and the dark reading taken
    beside the next frame is on a different scale - which came out as a
    confident -2.24 lx for the row the sensor is actually in. Two readings on
    two rungs are not comparable, and the difference between them is not a
    measurement of anything.
    """
    # A whole row is eleven pixels of white, about 660 mA - comfortably inside
    # what the firmware allows, which is 25 pixels' worth.
    print("Zeilen ...")
    rows = lab.sweep([white([(r, c) for c in range(COLUMNS)]) for r in range(ROWS)],
                     settle_ms=settle_ms, dark=True, rung=rung)
    warn_saturated(rows)
    row_signal = [signal(f) for f in rows["frames"]]

    print("Spalten ...")
    cols = lab.sweep([white([(r, c) for r in range(ROWS)]) for c in range(COLUMNS)],
                     settle_ms=settle_ms, dark=True, rung=rung)
    warn_saturated(cols)
    col_signal = [signal(f) for f in cols["frames"]]

    def report(name, values):
        top = max(values)
        floor = min(values)
        print("\n%s (lx über dunkel):" % name)
        for i, v in enumerate(values):
            bar = "#" * int(round(40 * v / top)) if top > 0 else ""
            print("  %2d  %9.4f  %s" % (i, v, bar))
        # A flat profile means the coupling is not local, and that is the
        # finding - not a failure of the measurement.
        spread = (top / floor) if floor > 0 else float("inf")
        print("  Verhältnis hellste/dunkelste: %.1f" % spread)
        return values.index(top), spread

    best_row, row_spread = report("Zeile", row_signal)
    best_col, col_spread = report("Spalte", col_signal)

    print("\nSchnittpunkt: Zeile %d, Spalte %d" % (best_row, best_col))
    if row_spread < 2.0 and col_spread < 2.0:
        print("ABER: beide Profile sind flach. Das Licht erreicht den Sensor")
        print("nicht zellenweise, sondern diffus - vermutlich über die")
        print("Frontscheibe. Eine Kopplungsmatrix pro Zelle wäre dann Rauschen.")
    return best_row, best_col


def infrared(lab, settle_ms=400, rung=DEFAULT_RUNG, path=None):
    """Can the two channels tell the room apart from the face?

    WS2812B put out almost no infrared and daylight puts out plenty, so the
    ratio CH1/CH0 is a property of the *source*. If the room and the display
    have different ratios, then two channels and two ratios are enough to solve
    for the room:

        CH0 = A0 + D0                    A0 = (rd * CH0 - CH1) / (rd - ra)
        CH1 = ra * A0 + rd * D0

    - and that would need no per-cell map at all, on any face, with no
    calibration of the geometry. It fails exactly when the two ratios are close,
    which is the case worth knowing about: white LED room lighting sits near the
    display's own ratio, daylight does not.

    Run it with the room lit. The display's ratio is taken from the increments
    over a dark frame beside it, so what the room contributes cancels; the
    room's own ratio is the dark frame itself.
    """
    path = path or CALIBRATION_FILE
    cells = None
    if os.path.exists(path):
        with io.open(path, encoding="utf-8") as handle:
            cells = [tuple(int(n) for n in key.split(","))
                     for key in json.load(handle)["cells"]]
    # The cells that actually reach the sensor if they are known, and the two
    # rows either side of it otherwise. Not the whole face: the question needs a
    # large display signal, not a large current.
    if not cells:
        cells = [(r, c) for r in (7, 8) for c in range(COLUMNS)]

    colours = [("weiss", (255, 255, 255)), ("weiss halb", (128, 128, 128)),
               ("rot", (255, 0, 0)), ("gruen", (0, 255, 0)), ("blau", (0, 0, 255))]
    frames = [{"clear": True, "set": []}]
    frames += [{"clear": True,
                "set": [{"cell": list(c), "rgb": list(rgb)} for c in cells]}
               for _, rgb in colours]

    result = lab.sweep(frames, settle_ms=settle_ms, dark=False, rung=rung)
    warn_saturated(result)
    got = result["frames"]
    dark = got[0]["lit"]
    if dark.get("ch0") is None:
        raise SystemExit("Dieser Sensor liefert keine Rohkanäle.")

    ambient_ratio = dark["ch1"] / float(dark["ch0"]) if dark["ch0"] else 0.0
    print("\nRaum allein:   CH0 %6d   CH1 %6d   CH1/CH0 %6.3f   (%.2f lx)"
          % (dark["ch0"], dark["ch1"], ambient_ratio, dark["lux"]))

    print("\n%-12s %8s %8s %8s   %s" % ("Anzeige", "dCH0", "dCH1", "CH1/CH0", "Trennung"))
    display_ratio = None
    for (name, _), frame in zip(colours, got[1:]):
        d0 = frame["lit"]["ch0"] - dark["ch0"]
        d1 = frame["lit"]["ch1"] - dark["ch1"]
        ratio = d1 / float(d0) if d0 else 0.0
        if name == "weiss":
            display_ratio = ratio
        # How far apart the two sources are, as the factor the solution divides
        # by. Below about 2 the arithmetic amplifies sensor noise faster than it
        # removes display light.
        apart = (ambient_ratio / ratio) if ratio else float("inf")
        print("%-12s %8d %8d %8.3f   %6.2fx" % (name, d0, d1, ratio, apart))

    if display_ratio and ambient_ratio > display_ratio:
        print("\nDie Kanaele trennen den Raum um Faktor %.2f vom Gesicht."
              % (ambient_ratio / display_ratio))
        print("Raumanteil = (%.3f * CH0 - CH1) / %.3f"
              % (display_ratio, display_ratio - ambient_ratio))
    else:
        print("\nDie Kanaele trennen hier nichts - Raum und Anzeige sehen gleich aus.")


def coupling_map(lab, settle_ms=120, rung=DEFAULT_RUNG):
    """Every cell on its own, against a dark reading beside it.

    Only worth running once `find` has shown the coupling to be local. 110
    frames, so it is split into runs the firmware will accept in one request.
    """
    cells = [(r, c) for r in range(ROWS) for c in range(COLUMNS)]
    values = {}
    batch = 55
    for start in range(0, len(cells), batch):
        chunk = cells[start:start + batch]
        print("Zellen %d..%d ..." % (start, start + len(chunk) - 1))
        result = lab.sweep([white([cell]) for cell in chunk],
                           settle_ms=settle_ms, dark=True, rung=rung)
        warn_saturated(result)
        for cell, frame in zip(chunk, result["frames"]):
            values[cell] = signal(frame)

    top = max(values.values())
    print("\nKopplung je Zelle, relativ zur stärksten (%.4f lx):" % top)
    for r in range(ROWS):
        print("  " + " ".join("%4.0f" % (1000 * values[(r, c)] / top) for c in range(COLUMNS)))
    print("\n(Promille der stärksten Zelle. 1000 = der Sensor sitzt dort.)")
    return values


# ------ the compensation ------
#
# What the clock needs in order to subtract its own face from what its sensor
# reads. The model is three numbers per cell:
#
#     contribution_lux = sum over lit cells of  r/255*Lr + g/255*Lg + b/255*Lb
#
# where Lr, Lg, Lb are the lux that cell puts into the sensor with that one
# channel at full drive. Both halves of that were measured before it was
# written rather than assumed:
#
#   * superposition across channels - white came out 1.3 % from red+green+blue;
#   * superposition across cells - the word SIEBEN came out 0.9 % from the sum
#     of its six cells;
#   * linearity in drive - grey 128 came out 3.6 % from half of white.
#
# Three coefficients rather than one, because the coupling is wavelength
# dependent and not slightly: at two cells' distance red carries almost twice
# as far as blue. One white coefficient would be about 9 % out on a green face
# and worse on a red one.

CALIBRATION_FILE = "coupling.json"

# Cells below this share of the strongest one are dropped. They are not zero,
# they are irrelevant: at 1/1000 of the peak a whole face of them would still
# be a tenth of one cell next to the sensor, and every one kept costs three
# more measurements and three more numbers to store.
KEEP_ABOVE_PERMILLE = 1.0


def calibrate(lab, settle_ms=150, out=CALIBRATION_FILE):
    """Measures the coupling of every cell that matters, per colour channel.

    Two passes. The first lights every cell white and finds the few that reach
    the sensor at all; the second lights only those, once per channel, and
    records what each puts in. Cover the clock: the numbers are differences
    against a dark reading taken beside each frame, so slow drift cancels, but
    a dark room is what makes them large against the noise.
    """
    cells = [(r, c) for r in range(ROWS) for c in range(COLUMNS)]

    print("Durchgang 1: alle %d Zellen in Weiss ..." % len(cells))
    coarse = {}
    for start in range(0, len(cells), 50):
        chunk = cells[start:start + 50]
        result = lab.sweep([{"clear": True, "set": []}] + [white([c]) for c in chunk],
                           settle_ms=settle_ms, dark=False, rung=COARSE_RUNG)
        frames = result["frames"]
        warn_saturated(result)
        floor = frames[0]["lit"]["ch0"]
        for cell, frame in zip(chunk, frames[1:]):
            coarse[cell] = frame["lit"]["ch0"] - floor
        print("  %d..%d" % (start, start + len(chunk) - 1))

    peak = max(coarse.values())
    keep = sorted(c for c in cells if 1000.0 * coarse[c] / peak >= KEEP_ABOVE_PERMILLE)
    print("\n%d von %d Zellen ueber %.1f Promille." % (len(keep), len(cells), KEEP_ABOVE_PERMILLE))

    print("\nDurchgang 2: %d Zellen x 3 Kanaele ..." % len(keep))
    channels = (("r", (255, 0, 0)), ("g", (0, 255, 0)), ("b", (0, 0, 255)))
    frames = [{"clear": True, "set": []}]
    for _, rgb in channels:
        for cell in keep:
            frames.append({"clear": True, "set": [{"cell": list(cell), "rgb": list(rgb)}]})

    result = lab.sweep(frames, settle_ms=settle_ms, dark=False, rung=COARSE_RUNG)
    warn_saturated(result)
    got = result["frames"]
    floor_lux = got[0]["lit"]["lux"] or 0.0

    coefficients = {}
    index = 1
    for name, _ in channels:
        for cell in keep:
            lux = got[index]["lit"]["lux"]
            value = max(0.0, (lux or 0.0) - floor_lux)
            coefficients.setdefault("%d,%d" % cell, {})[name] = round(value, 6)
            index += 1

    # The strongest cell carries the drive curve: it has the most signal left
    # at a drive of four, which is where the curve matters and the counts are
    # smallest.
    strongest = max(keep, key=lambda c: coarse[c])
    print("\nDurchgang 3: Treiberkennlinie an Zelle %s ..." % (strongest,))
    curve = drive_curve(lab, strongest)

    record = {
        "rows": ROWS, "columns": COLUMNS,
        "rung": COARSE_RUNG,
        "keptAbovePermille": KEEP_ABOVE_PERMILLE,
        "drive": {"cell": list(strongest), "levels": DRIVE_LEVELS, "response": curve},
        "cells": coefficients,
    }
    with io.open(out, "w", encoding="utf-8") as handle:
        handle.write(json.dumps(record, indent=1, sort_keys=True))

    print("\n%-9s %10s %10s %10s   lux bei vollem Kanal" % ("Zelle", "rot", "gruen", "blau"))
    for key in sorted(coefficients, key=lambda k: -sum(coefficients[k].values())):
        c = coefficients[key]
        print("  %-7s %10.4f %10.4f %10.4f" % (key, c["r"], c["g"], c["b"]))
    print("\nGeschrieben nach %s" % out)
    return record


# Drive values the response curve is sampled at. Below 4 the contribution is
# under half a percent of full and the counts are in the noise, so the curve
# stops there and anything smaller is interpolated down to zero.
DRIVE_LEVELS = [255, 192, 128, 96, 64, 48, 32, 24, 16, 12, 8, 6, 4]


def drive_curve(lab, cell, settle_ms=170, rung=COARSE_RUNG):
    """How the light out of one LED depends on the eight bit value written.

    Not linear, and not a gamma either. Measured on this clock: half drive
    gives 0.48 of full, a quarter gives 0.216, an eighth gives 0.082, and at 16
    it is 0.024 against the 0.063 a proportional lamp would give. The deviation
    is small at the top and enormous at the bottom - and the bottom is where
    the clock lives, because 20 % brightness through the gamma curve comes out
    as a drive of about seven.

    One curve for all three channels and all cells: measured separately for
    white, red, green and blue it came out the same to within a few parts in a
    thousand, so it belongs to the LED and its driver rather than to the colour.
    A table rather than a fitted model, because nothing fits it well - an offset
    of twelve counts holds from 255 down to 24 and then breaks completely.
    """
    frames = [{"clear": True, "set": []}]
    frames += [{"clear": True, "set": [{"cell": list(cell), "rgb": [v, v, v]}]}
               for v in DRIVE_LEVELS]
    result = lab.sweep(frames, settle_ms=settle_ms, dark=False, rung=rung)
    warn_saturated(result)
    got = result["frames"]
    floor = got[0]["lit"]["ch0"]
    counts = [frame["lit"]["ch0"] - floor for frame in got[1:]]
    top = float(counts[0])
    return [round(c / top, 5) for c in counts]


def response(record, value):
    """The curve, interpolated. 0 at 0, and linear between measured points."""
    if value <= 0:
        return 0.0
    levels = record["drive"]["levels"]
    curve = record["drive"]["response"]
    if value >= levels[0]:
        return curve[0]
    for i in range(len(levels) - 1):
        high, low = levels[i], levels[i + 1]
        if low <= value <= high:
            span = float(high - low)
            return curve[i + 1] + (curve[i] - curve[i + 1]) * (value - low) / span
    # Below the lowest measured point, straight down to zero.
    return curve[-1] * value / float(levels[-1])


def predict(record, leds):
    """What the face on the strip right now should be putting into the sensor.

    `leds` is the whole strip as /lab/leds returns it, so this works on the
    running clock without taking it over - which is the point. The firmware
    will do exactly this from its own frame buffer.
    """
    total = 0.0
    for key, coefficient in record["cells"].items():
        row, column = (int(n) for n in key.split(","))
        red, green, blue = leds[cell_index(row, column)]
        # Through the drive curve, not proportionally. Proportionally was 22 %
        # out at quarter drive and far worse below that.
        total += response(record, red) * coefficient["r"]
        total += response(record, green) * coefficient["g"]
        total += response(record, blue) * coefficient["b"]
    return total


def gamma_scale(percent):
    """The driver's own brightness curve, copied from _gammaScale().

    Copied rather than asked for, because the whole point of the run below is
    to reach the drive values the clock really uses: 20 % comes out as 7, which
    is where the LED response is least proportional and where the clock spends
    its evenings.
    """
    if percent <= 0:
        return 0
    if percent >= 100:
        return 255
    return max(1, int(round(255.0 * (percent / 100.0) ** 2.2)))


# The word that sits under the sensor on the German panel. Row 8, the first six
# columns: SIEBEN. Nothing else the clock can display couples anywhere near as
# strongly, so this is the worst case rather than a typical one.
WORST_WORD = [(8, c) for c in range(6)]


def upload_coupling(lab, path=None):
    """Puts the measured map on the clock, where the regulator can use it.

    The clock keeps it in NVS and subtracts it from every sample before the
    averages see the number - so this is the moment the whole measurement
    stops being a report and starts being a correction. Behind expert mode: it
    changes how the clock reads its own sensor.
    """
    path = path or CALIBRATION_FILE
    with io.open(path, encoding="utf-8") as handle:
        record = json.load(handle)

    answer = lab._call("/light", {"coupling": {"cells": record["cells"],
                                              "drive": record["drive"]}})
    print("Auf der Uhr: %d Zellen. Anzeige gerade %.4f lx von %.4f lx roh."
          % (answer.get("coupled", 0), answer.get("display", 0.0),
             answer.get("raw", 0.0)))
    return answer


def base_colour(lab):
    """What the driver writes at full brightness, read back off the strip.

    Not computed from hue and saturation: FastLED's rainbow wheel is not the
    HSV anybody would write down by hand, and this run is only worth doing at
    the values the clock really uses.

    **The automatic has to be switched off first**, and that is not a detail.
    With it on, POST /color is not a setting, it is a lesson: the clock takes
    the number as "at this light, I want this much". The first version of this
    function turned the brightness up to 100 and back to read the colour, and
    in a real calibration it taught the clock three times that 0 lx deserves
    50 % - the strip was blanked by the lab at the time - and the fitted slope
    collapsed from 27.5 to 3.6 %/decade. Three junk points, from a function
    that was only trying to look.

    Reading it at the running brightness without writing anything was the other
    option and is worse: at a drive of eighteen the colour comes back as
    [36, 255, 0] where it is really [32, 245, 11] - green into the clamp, blue
    quantised away entirely.

    Everything is put back, including the automatic.
    """
    state = lab._call("/currentState")
    hue, sat, lum = state["hue"], state["sat"], state["lum"]
    auto = bool(state.get("automaticLum"))

    try:
        if auto:
            lab.post_raw("/autoluminance", {"automaticLum": 0})
        lab.post_raw("/color", {"hue": hue, "sat": sat, "lum": 100})
        time.sleep(0.5)
        lit = [c for c in lab._call("/lab/leds")["leds"] if c != [0, 0, 0]]
        return max(lit, key=sum) if lit else [255, 255, 255]
    finally:
        lab.post_raw("/color", {"hue": hue, "sat": sat, "lum": lum})
        if auto:
            lab.post_raw("/autoluminance", {"automaticLum": 1})


def feedback(lab, base=None, cells=None, settle_ms=400, rung=3, path=None):
    """Does the compensation actually break the loop?

    Every check so far has been a single frame. This is the one that matters:
    the worst face on the clock, swept over the display's whole brightness
    range, with a dark reading taken beside every frame so that a cloud passing
    over cannot be mistaken for a result.

    Three numbers per level. `dunkel` is the room with the display off, which
    is the truth. `hell` is what the sensor reports with the face lit, which is
    what the regulator would have used. `Rest` is what the regulator gets after
    the model is subtracted - and it is the only one that has to stay flat.
    """
    path = path or CALIBRATION_FILE
    with io.open(path, encoding="utf-8") as handle:
        record = json.load(handle)
    cells = cells or WORST_WORD
    base = base or base_colour(lab)

    levels = [20, 30, 40, 50, 60, 70, 80, 90, 100]
    drives = [gamma_scale(p) for p in levels]
    frames = [{"clear": True,
               "set": [{"cell": list(c),
                        "rgb": [int(round(v * d / 255.0)) for v in base]} for c in cells]}
              for d in drives]

    lab.take(True)
    try:
        result = lab.sweep(frames, settle_ms=settle_ms, dark=True, rung=rung)
    finally:
        lab.take(False)
    warn_saturated(result)

    print("\n%d Zellen, Grundfarbe %s, Sprosse %d\n" % (len(cells), base, rung))
    print("%-6s %6s %9s %9s %9s %9s %8s" % (
        "Anz.", "Wert", "dunkel", "hell", "Modell", "Rest", "Fehler"))

    worst = 0.0
    for percent, drive, frame in zip(levels, drives, result["frames"]):
        rgb = [int(round(v * drive / 255.0)) for v in base]
        leds = [[0, 0, 0]] * 114
        for cell in cells:
            leds[cell_index(*cell)] = rgb
        model = predict(record, leds)
        dark = frame["dark"]["lux"]
        lit = frame["lit"]["lux"]
        rest = lit - model
        # Against the room where there is one, against the model where there is
        # not. In a properly dark room `dark` is zero, and dividing by it
        # reported a serene 0.0 % for every row - which is the one number this
        # table exists to produce, printed without having been computed.
        reference = dark if dark > 0.01 else model
        error = (rest - dark) / reference * 100.0 if reference > 0 else 0.0
        worst = max(worst, abs(error))
        print("%5d%% %6d %9.3f %9.3f %9.3f %9.3f %7.1f%%" % (
            percent, drive, dark, lit, model, rest, error))

    first, last = result["frames"][0], result["frames"][-1]
    low = first["lit"]["lux"]
    swing = (last["lit"]["lux"] - low) / low * 100.0 if low > 0.001 else None
    if swing is None:
        print("\nOhne Kompensation waere der Messwert von praktisch 0 auf %.2f lx gewandert." % last["lit"]["lux"])
    else:
        print("\nOhne Kompensation wandert der Messwert um %+.1f %% ueber den Regelbereich." % swing)
    print("Mit Kompensation bleibt der Rest im schlechtesten Fall %.1f %% daneben." % worst)


def cell_index(row, column):
    """Cell to strip index - the same arithmetic physicalFor() does on the clock.

    Repeated here rather than asked for, because it is needed once per cell per
    prediction and a round trip for each would be absurd. It is checked against
    the clock instead: `python lab.py <address> wiring` lights cells and reads
    back which pixels they were.
    """
    number = (10 - column) + 11 * (9 - row)
    if (number // 11) % 2 == 0:
        return number
    return (number // 11) * 11 + 10 - (number % 11)


def check(lab, path=CALIBRATION_FILE):
    """Predicted against measured, on the clock as it is running.

    No lab mode: /lab/leds and /lab/sensor both answer while the clock shows
    the time, so this is the compensation being tried on the real thing rather
    than on a test pattern built to suit it.
    """
    with io.open(path, encoding="utf-8") as handle:
        record = json.load(handle)

    leds = lab._call("/lab/leds")["leds"]
    reading = lab.sensor()
    clock = lab._call("/light")
    expected = predict(record, leds)
    measured = reading["lux"] or 0.0

    print("  gemessen     %9.4f lx" % measured)
    print("  Anzeige      %9.4f lx  (vorhergesagt aus %d Zellen)"
          % (expected, len(record["cells"])))
    print("  Umgebung     %9.4f lx  <- was uebrig bleibt" % (measured - expected))

    # The same sum, done by the clock from its own frame buffer. Two
    # implementations of one model, and this is the only place they meet - the
    # firmware could have the map, the drive table or the cell-to-pixel
    # arithmetic wrong and every other check here would still pass.
    print("")
    print("  Uhr: %d Zellen gespeichert, Anzeige %9.4f lx von %.4f lx roh"
          % (clock.get("coupled", 0), clock.get("display", 0.0), clock.get("raw", 0.0)))
    own = clock.get("display", 0.0)
    if expected > 0.01 or own > 0.01:
        gap = (own - expected) / max(expected, 1e-9) * 100.0
        print("  Abweichung Skript gegen Uhr: %+.1f %%" % gap)
    else:
        print("  Nichts beleuchtet, was koppelt - kein Vergleich moeglich.")
    return measured, expected


def wiring(lab):
    """Checks the cell-to-pixel mapping against the strip, both directions."""
    lab.take(True)
    try:
        wrong = 0
        for cell in [(9, 10), (9, 0), (8, 0), (8, 10), (0, 0), (0, 10), (7, 5)]:
            lab.leds([{"cell": list(cell), "rgb": [255, 255, 255]}])
            lit = [i for i, c in enumerate(lab._call("/lab/leds")["leds"]) if c != [0, 0, 0]]
            mine = cell_index(*cell)
            ok = lit == [mine]
            wrong += 0 if ok else 1
            print("  Zelle %-8s Uhr %-6s Skript %-4d %s"
                  % (cell, lit, mine, "" if ok else "<-- ABWEICHUNG"))
        print("\n%s" % ("Abbildung stimmt ueberein." if not wrong else "%d Abweichungen!" % wrong))
    finally:
        lab.take(False)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        raise SystemExit(2)

    lab = Lab(sys.argv[1])
    what = sys.argv[2]

    if what == "state":
        print(json.dumps(lab.state(), indent=2))
        return
    if what == "off":
        print(lab.take(False))
        return
    if what == "check":
        # Deliberately outside lab mode: this is the compensation tried on the
        # clock while it is showing the time, not on a pattern chosen to suit.
        check(lab)
        return
    if what == "wiring":
        wiring(lab)
        return
    if what == "upload":
        # No lab mode: this writes a setting, it does not measure.
        upload_coupling(lab)
        return
    if what == "feedback":
        # Takes the strip itself: the clock's own colour has to be read off it
        # before the lab blanks it.
        feedback(lab)
        return

    state = lab.state()
    if not state["sensor"]["present"]:
        raise SystemExit("Diese Uhr hat keinen Lichtsensor.")
    print("Sensor: %s, %d Sprossen" % (state["sensor"]["name"], state["sensor"].get("rungs", 0)))

    lab.take(True)
    try:
        if what == "find":
            find_sensor(lab)
        elif what == "ir":
            infrared(lab, rung=int(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_RUNG)
        elif what == "map":
            coupling_map(lab)
        elif what == "calibrate":
            calibrate(lab)
        else:
            raise SystemExit("Unbekannt: %s" % what)
    finally:
        # Always hand the strip back, even after an exception - otherwise the
        # clock is left dark on the wall with no sign of why.
        lab.take(False)
        print("\nStreifen zurückgegeben.")


if __name__ == "__main__":
    main()
