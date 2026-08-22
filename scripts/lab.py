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
    python scripts/lab.py <address> off       # give the strip back

Cover the clock for `find` and `map`: with no ambient light the measured
difference is the display's own contribution and nothing else. Every sweep
takes a dark reading beside each frame anyway, so slow drift cancels - but
starting from darkness makes the numbers larger than the noise.
"""
import json
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


def infrared(lab, cell=None, settle_ms=400):
    """Does the infrared channel escape the display?

    WS2812B put out almost no infrared; room light and daylight put out
    plenty. If CH1 barely moves while CH0 swings with the face, then CH1 is an
    ambient reading the clock cannot pollute - which would be worth more than
    any compensation model, because it needs no calibration at all.

    Run this with the room lit, not covered: the question is whether the two
    channels can be told apart, and a dark room has nothing to tell apart.
    """
    # Never the whole face. 114 pixels of white want about 6.8 A, and the first
    # attempt browned the clock out and reset it - the firmware refuses such a
    # frame now, but there is no reason to ask for one: two rows are already a
    # far bigger swing than the sensor needs to answer the question.
    row = lambda r: [(r, c) for c in range(COLUMNS)]
    frames = [
        {"clear": True, "set": []},
        white(row(0)),
        white(row(0) + row(ROWS - 1)),
    ]
    # Pinned, or the ladder moves between frames and the counts stop being
    # comparable - which is exactly what we are comparing.
    result = lab.sweep(frames, settle_ms=settle_ms, dark=False, rung=6)

    print("\n%-14s %10s %10s %10s %8s" % ("Bild", "CH0", "CH1", "lux", "CH1/CH0"))
    base = None
    for name, frame in zip(("aus", "eine Zeile", "zwei Zeilen"), result["frames"]):
        reading = frame["lit"]
        ch0, ch1 = reading.get("ch0"), reading.get("ch1")
        if ch0 is None:
            raise SystemExit("Dieser Sensor liefert keine Rohkanäle.")
        ratio = (ch1 / ch0) if ch0 else 0.0
        print("%-14s %10d %10d %10.3f %8.3f" % (name, ch0, ch1, reading["lux"], ratio))
        if base is None:
            base = (ch0, ch1)

    ch0_swing = result["frames"][-1]["lit"]["ch0"] - base[0]
    ch1_swing = result["frames"][-1]["lit"]["ch1"] - base[1]
    print("\nHub durch die Anzeige:  CH0 %+d   CH1 %+d" % (ch0_swing, ch1_swing))
    if ch0_swing > 0:
        print("CH1 bewegt sich zu %.1f %% von CH0." % (100.0 * ch1_swing / ch0_swing))
        print("Je kleiner, desto brauchbarer ist CH1 als Umgebungsmesswert.")


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

    state = lab.state()
    if not state["sensor"]["present"]:
        raise SystemExit("Diese Uhr hat keinen Lichtsensor.")
    print("Sensor: %s, %d Sprossen" % (state["sensor"]["name"], state["sensor"].get("rungs", 0)))

    lab.take(True)
    try:
        if what == "find":
            find_sensor(lab)
        elif what == "ir":
            infrared(lab)
        elif what == "map":
            coupling_map(lab)
        else:
            raise SystemExit("Unbekannt: %s" % what)
    finally:
        # Always hand the strip back, even after an exception - otherwise the
        # clock is left dark on the wall with no sign of why.
        lab.take(False)
        print("\nStreifen zurückgegeben.")


if __name__ == "__main__":
    main()
