# -*- coding: utf-8 -*-
"""The automatic brightness, run from here instead of from the clock.

The regulator on the clock cannot be trusted while the sensor can see the face
it is regulating: on this clock one letter - the N of SIEBEN, directly under
the sensor - puts 32 lx into a reading whose room contributes a fraction of
one. Compensating for that means subtracting the display's own contribution
before the curve ever sees the number, and scripts/lab.py has measured what to
subtract. This is that compensation driving the clock, with every part of the
loop on this side where it can be watched, changed and reverted in a second.

    python scripts/regulate.py <address>            # regulate, and say why
    python scripts/regulate.py <address> --zero     # blink dark once, first
    python scripts/regulate.py <address> --dry      # watch, change nothing
    python scripts/regulate.py <address> lernen     # guided calibration run
    python scripts/regulate.py <address> --zero-every 10 --log lauf.csv

**Switch the clock's own automatic off first** (colour tab). Two regulators on
one clock would fight, and with the automatic off the brightness slider becomes
what this needs: a plain number in /currentState that the firmware applies the
instant it is moved. That is where the immediate response comes from - the hand
on the slider reaches the LEDs without passing through here at all.

How a nudge is noticed: this remembers the last value it wrote, so a `lum` in
/currentState that is anything else is a hand on the slider. The log would have
been the other candidate and is worse - with the automatic off the firmware
logs nothing when the brightness changes, and a line of text would have to be
parsed to recover a number that /currentState hands over as a number.

Nothing here is a new idea. The curve, the settle time, the replacement of a
near neighbour, the anchoring on the newest point and the easing are all ported
straight from Luminance.cpp and main .cpp so that what is learned here can be
carried back without a translation step. The one thing that is new is the line
marked "compensation" in measure().
"""
import argparse
import io
import json
import math
import os
import sys
import threading
import time
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lab import Lab, predict, CALIBRATION_FILE   # noqa: E402

# ---- ported from Luminance.h, deliberately unchanged ----
LUM_POINTS = 10
LUM_MIN_PERCENT = 20
LUM_MAX_PERCENT = 100
LUM_SETTLE_S = 10.0
LUM_SAME_LIGHT_RATIO = 1.3
LUM_FIT_MIN_DECADES = 0.6
LUM_DEFAULT_LOW_LUX, LUM_DEFAULT_LOW_PERCENT = 0.3, 20
LUM_DEFAULT_HIGH_LUX, LUM_DEFAULT_HIGH_PERCENT = 9.0, 100

# ---- ported from LightSensor.cpp ----
SMOOTHING_SECONDS = 30.0

# What a point is *taught* from, which is not the average the regulator runs
# on - and the difference is a defect the first real session exposed.
#
# The settle time is 10 s and the smoothing is 30 s, so a correction made just
# after the light changed is stored against a reading that has travelled only a
# third of the way. In lauf.csv, 22:14: the room went to 0.0009 lx, the slider
# was set to 30 %, and ten seconds later the pair kept was `0.1184 lx -> 30 %`
# - a light level 148 times the one the person was actually sitting in, and it
# went straight into the slope.
#
# Regulating wants a long average, because it must not chase a passing shadow.
# Learning wants a short one, because the person is telling the clock about the
# light in the room *now*. Three seconds averages the sensor and has still
# arrived well inside the settle time.
TEACH_SECONDS = 3.0

# How often the loop runs. The clock's own sampler runs at 2 s and the web
# server is synchronous, so three requests a second would be taking room from
# the browser somebody has left open on the colour tab. Two seconds is also
# well inside the ten second settle, which is the only deadline here.
TICK_S = 2.0

# The learned points, kept between runs of this script. Per clock and per room,
# the same as coupling.json, and gitignored for the same reason.
CURVE_FILE = "curve.json"


def log_lux(lux):
    """log10, with the floor Luminance.cpp uses. Zero light is not a number."""
    return -2.0 if lux < 0.01 else math.log10(lux)


class Curve(object):
    """The straight line in log light, and the points it is fitted through.

    A transcription of Luminance.cpp rather than a fresh design: the point of
    running it here is to see this exact behaviour against a compensated
    reading, so any improvement has to be made deliberately and separately.
    """

    def __init__(self):
        self.points = []          # oldest first, as on the clock
        self.default_line()

    def default_line(self):
        low, high = log_lux(LUM_DEFAULT_LOW_LUX), log_lux(LUM_DEFAULT_HIGH_LUX)
        self.slope = (LUM_DEFAULT_HIGH_PERCENT - LUM_DEFAULT_LOW_PERCENT) / (high - low)
        self.offset = LUM_DEFAULT_LOW_PERCENT - self.slope * low
        self.fitted = False

    def for_lux(self, lux):
        value = int(round(self.slope * log_lux(lux) + self.offset))
        return max(LUM_MIN_PERCENT, min(LUM_MAX_PERCENT, value))

    def remember(self, lux, percent):
        """Adds a point, replacing a near neighbour rather than appending.

        Taken out and re-appended, not overwritten in place: the line is
        anchored on the newest point, so "newest" has to keep meaning something.
        """
        for i, (old_lux, _) in enumerate(self.points):
            if old_lux > 0.0 and lux > 0.0:
                ratio = lux / old_lux if lux > old_lux else old_lux / lux
                if ratio <= LUM_SAME_LIGHT_RATIO:
                    self.points.pop(i)
                    break
        if len(self.points) == LUM_POINTS:
            self.points.pop(0)
        self.points.append((lux, percent))
        self.fit()

    def fit(self):
        """Slope from every point, level from the newest one alone.

        The slope is the room and is learned slowly; the level is the
        instruction and is owed in full. Least squares on both put the line
        through the centroid, which meant asking for 55 % and being given 47 %
        however often it was repeated.
        """
        if not self.points:
            self.default_line()
            return

        xs = [log_lux(lux) for lux, _ in self.points]
        ys = [float(percent) for _, percent in self.points]
        mean_x, mean_y = sum(xs) / len(xs), sum(ys) / len(ys)

        can_fit = (max(xs) - min(xs)) >= LUM_FIT_MIN_DECADES
        if can_fit:
            top = sum((x - mean_x) * (y - mean_y) for x, y in zip(xs, ys))
            bottom = sum((x - mean_x) ** 2 for x in xs)
            candidate = top / bottom if bottom > 0.0 else 0.0
            # Darker room, brighter clock is not a thing anybody wants, and one
            # careless nudge in daylight produces it.
            if candidate > 0.0:
                self.slope = candidate
            else:
                can_fit = False

        self.offset = ys[-1] - self.slope * xs[-1]
        self.fitted = can_fit

    def load(self, path=CURVE_FILE):
        if not os.path.exists(path):
            return self
        with io.open(path, encoding="utf-8") as handle:
            stored = json.load(handle)
        self.points = [(p[0], p[1]) for p in stored.get("points", [])][-LUM_POINTS:]
        # Re-fitted rather than trusted: the points are the record and the line
        # is derived from them, the same rule the firmware follows.
        self.fit()
        return self

    def save(self, path=CURVE_FILE):
        record = {"slope": round(self.slope, 4), "offset": round(self.offset, 4),
                  "fitted": self.fitted,
                  "points": [[round(lux, 4), percent] for lux, percent in self.points]}
        with io.open(path, "w", encoding="utf-8") as handle:
            handle.write(json.dumps(record, indent=1))

    def describe(self):
        return "%.1f %%/Dekade bei %.1f %%, %d Punkte, Steigung %s" % (
            self.slope, self.offset, len(self.points),
            "gefittet" if self.fitted else "behalten")


class Clock(object):
    """The clock as a regulator needs it: a reading, a face, and a slider."""

    def __init__(self, host):
        self.lab = Lab(host, timeout=20)

    def settings(self):
        return self.lab._call("/currentState")

    def set_brightness(self, hue, sat, lum):
        """Writes the brightness, and deliberately does not read the answer.

        hue and sat travel with it because updateColor() reads all three from
        the document and a missing one arrives as zero - which would turn the
        face red at the first correction.

        The answer is thrown away because it is not JSON: six handlers in
        WebRoutes.cpp still send the literal `{msg: ''}` under an
        application/json content type, left over from the jQuery UI that never
        looked at it. Parsing it is what broke this the first time it tried to
        correct anything.
        """
        body = json.dumps({"hue": hue, "sat": sat, "lum": lum}).encode("utf-8")
        request = urllib.request.Request(self.lab.base + "/color", data=body,
                                         headers={"Content-Type": "application/json"})
        urllib.request.urlopen(request, timeout=self.lab.timeout).read()

    def face(self):
        """The strip as it is, without taking it - /lab/leds answers either way."""
        return self.lab._call("/lab/leds")["leds"]

    def sensor(self):
        return self.lab._call("/lab/sensor")


def measure(clock, coupling):
    """One reading, with the display's own contribution taken out of it.

    This is the whole experiment in four lines. `raw` is what the sensor sees,
    `display` is what the face on the strip right now is putting into it
    according to the coefficients measured by lab.py, and the difference is the
    room. Clamped at zero: a small overshoot in the model must not come out as
    negative light, which log10 has no answer for.
    """
    leds = clock.face()
    reading = clock.sensor()
    raw = reading["lux"] or 0.0
    display = predict(coupling, leds)
    return raw, display, max(0.0, raw - display)


def zero_check(clock, coupling):
    """Blinks the face dark for a moment and compares. The ground truth.

    With the display off the sensor reads the room and nothing else, so this
    says how good the compensation is on this clock in this room - which no
    amount of arithmetic on a lit face can. One second of darkness, and worth
    it: an uncalibrated regulator that believes its own compensation is exactly
    the failure this is meant to replace.
    """
    raw, display, estimate = measure(clock, coupling)

    clock.lab.take(True)
    try:
        clock.lab.leds([])
        time.sleep(0.6)
        truth = clock.sensor()["lux"] or 0.0
    finally:
        clock.lab.take(False)

    error = truth - estimate
    print("Nullpunkt:")
    print("  Sensor roh        %9.4f lx" % raw)
    print("  Anzeige (Modell)  %9.4f lx" % display)
    print("  Umgebung gerechnet%9.4f lx" % estimate)
    print("  Umgebung dunkel   %9.4f lx  <- gemessen" % truth)
    print("  Abweichung        %9.4f lx  (%s)" % (
        error, "%.1f %%" % (100.0 * error / truth) if truth > 0.001 else "Raum ist dunkel"))
    print("")
    return truth


def regulate(host, do_zero=False, zero_every=0.0, dry=False, log_path=None):
    with io.open(CALIBRATION_FILE, encoding="utf-8") as handle:
        coupling = json.load(handle)

    clock = Clock(host)
    curve = Curve().load()

    state = clock.settings()
    if state.get("automaticLum"):
        raise SystemExit("Die Automatik der Uhr ist an. Bitte im Farbreiter "
                         "ausschalten - zwei Regler auf einer Uhr streiten sich.")

    hue, sat = state["hue"], state["sat"]
    print("Kurve: %s" % curve.describe())
    print("Kompensation: %d Zellen aus %s\n" % (len(coupling["cells"]), CALIBRATION_FILE))

    if do_zero or zero_every:
        zero_check(clock, coupling)
    next_zero = time.time() + zero_every * 60.0 if zero_every else None

    written = state["lum"]         # what the slider says, and what I last wrote
    applied = float(written)       # the eased value, as main .cpp keeps it
    smoothed = None                # what the regulator runs on, 30 s
    teaching = None                # what a point is taught from, 3 s
    wanted = None                  # a nudge waiting out its settle time
    settle_at = 0.0
    logfile = io.open(log_path, "a", encoding="utf-8") if log_path else None
    if logfile and logfile.tell() == 0:
        logfile.write("zeit,roh,anzeige,umgebung,regeln,lernen,ziel,gestellt,nudge\n")

    print("%-8s %9s %9s %9s %9s %9s %6s %6s" % (
        "Zeit", "roh", "Anzeige", "Umgebung", "regeln", "lernen", "Kurve", "gestellt"))

    last = time.time()
    while True:
        now = time.time()
        elapsed, last = now - last, now

        # 1. Has the hand been on the slider? With the automatic off, the
        #    firmware has already applied whatever it says - the point of
        #    running it that way round is that the response is immediate.
        state = clock.settings()
        hue, sat = state["hue"], state["sat"]
        # One tick wide, and that is the whole exposure: a move made in the
        # window between a write of mine and this read is overwritten and lost.
        # It stays small because a settled regulator writes nothing at all -
        # the easing only produces writes while the room is actually changing.
        if state["lum"] != written:
            wanted = max(LUM_MIN_PERCENT, min(LUM_MAX_PERCENT, state["lum"]))
            written = state["lum"]
            applied = float(wanted)
            settle_at = now + LUM_SETTLE_S       # every move only pushes it out

        # 2. What the room is doing, with the face subtracted.
        raw, display, ambient = measure(clock, coupling)
        weight = elapsed / (SMOOTHING_SECONDS + elapsed)
        smoothed = ambient if smoothed is None else smoothed + weight * (ambient - smoothed)
        quick = elapsed / (TEACH_SECONDS + elapsed)
        teaching = ambient if teaching is None else teaching + quick * (ambient - teaching)

        # 3. Keep the pair once the slider has been still long enough.
        learned = ""
        if wanted is not None and now >= settle_at:
            # From the short average, not the long one: see TEACH_SECONDS.
            curve.remember(teaching, wanted)
            curve.save()
            learned = "  <- gelernt: %.4f lx -> %d %%, %s" % (
                teaching, wanted, curve.describe())
            wanted = None

        # 4. The target, approached by an eighth of the distance per second.
        #    Not about noise - the reading is already averaged over 30 s - but
        #    about the step when somebody switches a lamp on.
        target = curve.for_lux(smoothed)
        if wanted is None:
            for _ in range(max(1, int(round(elapsed)))):
                distance = target - applied
                if abs(distance) < 1.0:
                    applied = float(target)
                    break
                step = max(1.0, abs(distance) / 8.0)
                applied += step if distance > 0 else -step

        want_written = int(round(applied))
        if wanted is None and want_written != written and not dry:
            clock.set_brightness(hue, sat, want_written)
            written = want_written

        if next_zero is not None and now >= next_zero:
            zero_check(clock, coupling)
            next_zero = now + zero_every * 60.0

        stamp = time.strftime("%H:%M:%S")
        print("%-8s %9.4f %9.4f %9.4f %9.4f %9.4f %6d %6d%s%s" % (
            stamp, raw, display, ambient, smoothed, teaching, target, written,
            "  (Regler)" if wanted is not None else "", learned))
        if logfile:
            logfile.write("%s,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%d,%d\n" % (
                stamp, raw, display, ambient, smoothed, teaching, target, written,
                1 if wanted is not None else 0))
            logfile.flush()

        time.sleep(max(0.0, TICK_S - (time.time() - now)))


# ------ the guided run ------
#
# Teaching by nudging works, but it leaves the person to work out on their own
# what the clock still needs - and what it needs is *spread*, which is the one
# thing that is invisible while doing it. Four corrections made in one evening
# look like diligence and carry no slope at all.
#
# So the script says what to do at each point instead: how much the light has
# to change before the next one is worth taking, which direction, and when
# there is enough. It is the same thing the on-clock calibration run will have
# to say later, written down here first where it costs nothing to change.

# What a new point must differ from every existing one by, or it is telling
# the clock something it already knows. A factor of four is LUM_FIT_MIN_DECADES
# expressed the way a person can act on it.
TEACH_MIN_FACTOR = 4.0

# What to aim for between the darkest and the brightest point. Not a
# requirement - a home may not offer it - but it is what makes a slope worth
# trusting, so it is what the run asks for.
TEACH_GOOD_DECADES = 1.5

# How still the light has to be before a point is taken. The short average
# needs a few seconds to arrive, and a hand still on the light switch is not a
# lighting condition.
TEACH_STABLE_S = 6.0
TEACH_STABLE_RATIO = 1.15


class Prompt(object):
    """ENTER without blocking, so the light can go on being shown while waiting.

    input() would freeze the display at whatever it last printed, which is
    exactly the number the person is watching in order to decide when to press
    the key.
    """

    def __init__(self):
        self.hit = False
        thread = threading.Thread(target=self._wait)
        thread.daemon = True
        thread.start()

    def _wait(self):
        for _ in sys.stdin:
            self.hit = True

    def taken(self):
        was, self.hit = self.hit, False
        return was


def lag(value, sample, interval):
    """One step of the same first order lag the regulator uses, over `interval`.

    Written out rather than left as a constant, because the two loops below
    tick at different rates and a weight that is right in one is wrong in the
    other.
    """
    if value is None:
        return sample
    return value + interval / (TEACH_SECONDS + interval) * (sample - value)


def describe_light(lux):
    """A word for a lux value, so the run reads like an instruction."""
    if lux < 0.01:
        return "voellig dunkel"
    if lux < 0.2:
        return "Nacht, kein Licht an"
    if lux < 1.5:
        return "gedaempft, eine Lampe"
    if lux < 6.0:
        return "Zimmer beleuchtet"
    if lux < 30.0:
        return "heller Tag"
    return "sehr hell"


def teach(host, fresh=False, wanted=4):
    """A guided calibration run: the script says what to do, point by point."""
    with io.open(CALIBRATION_FILE, encoding="utf-8") as handle:
        coupling = json.load(handle)

    clock = Clock(host)
    curve = Curve() if fresh else Curve().load()

    state = clock.settings()
    if state.get("automaticLum"):
        raise SystemExit("Die Automatik der Uhr ist an. Bitte im Farbreiter "
                         "ausschalten - sonst stellt sie den Regler selbst.")

    print("Kalibrierlauf. Ich sage jeweils, was zu tun ist; abbrechen mit Strg-C.")
    if curve.points:
        print("Vorhandene Punkte: %s"
              % ", ".join("%.3g lx -> %d %%" % p for p in curve.points))
    else:
        print("Keine Punkte bisher - wir fangen bei null an.")
    print("")

    prompt = Prompt()
    teaching = None
    hue, sat = state["hue"], state["sat"]

    def sample():
        """One compensated reading, into the short average used for teaching."""
        raw, display, ambient = measure(clock, coupling)
        return raw, display, ambient

    taken = 0
    while taken < wanted:
        number = len(curve.points) + 1
        print("--- Punkt %d ---" % number)

        # ---- 1. the light, and what it has to be
        if curve.points:
            print(advice(curve))
            print("    Ich zeige das Licht laufend an. ENTER, wenn es soweit ist.")
        else:
            print("    Wir nehmen das Licht, das gerade da ist.")
            print("    ENTER, wenn Du bereit bist.")

        history = []
        while True:
            _, _, ambient = sample()
            teaching = lag(teaching, ambient, TICK_S)
            history.append((time.time(), teaching))
            history[:] = [h for h in history if h[0] > time.time() - TEACH_STABLE_S]
            steady = stable(history)
            sys.stdout.write("\r    %8.3f lx  (%s)  %-28s" % (
                teaching, describe_light(teaching), note(curve, teaching, steady)))
            sys.stdout.flush()
            if prompt.taken():
                if not steady:
                    print("\n    Das Licht ist noch in Bewegung - einen Moment, "
                          "dann nochmal ENTER.")
                    continue
                break
            time.sleep(TICK_S)
        print("")

        light = teaching

        # ---- 2. the brightness, from the slider
        print("    Stelle den Helligkeitsregler jetzt so, wie die Uhr Dir bei")
        print("    DIESEM Licht gefallen soll. Ich merke selbst, wenn Du fertig")
        print("    bist - oder ENTER, wenn die Helligkeit schon passt.")

        start = clock.settings()["lum"]
        last_move = None
        while True:
            now = clock.settings()
            hue, sat = now["hue"], now["sat"]
            _, _, ambient = sample()
            teaching = lag(teaching, ambient, 1.0)

            if now["lum"] != start:
                start = now["lum"]
                last_move = time.time()

            if last_move is not None:
                left = LUM_SETTLE_S - (time.time() - last_move)
                if left <= 0:
                    break
                sys.stdout.write("\r    Regler auf %3d %%, noch %4.1f s ruhig halten   "
                                 % (start, left))
            else:
                sys.stdout.write("\r    Regler steht auf %3d %%                       " % start)
            sys.stdout.flush()

            if prompt.taken() and last_move is None:
                break
            time.sleep(1.0)
        print("")

        percent = max(LUM_MIN_PERCENT, min(LUM_MAX_PERCENT, start))
        curve.remember(light, percent)
        curve.save()
        taken += 1

        print("    Gemerkt: %.4g lx -> %d %%" % (light, percent))
        if percent >= LUM_MAX_PERCENT or percent <= LUM_MIN_PERCENT:
            print("    Achtung: %d %% ist der Anschlag. Der Punkt sagt "
                  "'mindestens so viel',"  % percent)
            print("    nicht 'genau so viel' - er macht die Steigung eher zu flach.")
            print("    Ein Punkt bei Licht, wo Du zwischen 21 und 99 landest, "
                  "waere mehr wert.")
        print("    Kurve: %s" % curve.describe())
        print("")

        if enough(curve):
            print("Das reicht: %s" % spread_line(curve))
            break

    print("")
    print("Kurve steht in %s" % CURVE_FILE)
    print("  %s" % curve.describe())
    print("  %s" % spread_line(curve))
    print("")
    print("%-12s %s" % ("Licht", "Helligkeit"))
    for lux in (0.01, 0.1, 0.5, 2.0, 8.0, 40.0, 200.0):
        print("%9.2f lx %6d %%   %s" % (lux, curve.for_lux(lux), describe_light(lux)))
    print("")
    print("Jetzt regeln lassen:  python scripts/regulate.py %s" % host)


def stable(history):
    """True when the light has not moved much for TEACH_STABLE_S."""
    if len(history) < 3 or history[-1][0] - history[0][0] < TEACH_STABLE_S - TICK_S:
        return False
    values = [v for _, v in history if v > 0]
    if not values:
        return True
    return max(values) / min(values) <= TEACH_STABLE_RATIO


def factor_to_nearest(curve, lux):
    """How far this light is from the closest point already taken."""
    if not curve.points or lux <= 0:
        return float("inf")
    best = float("inf")
    for old, _ in curve.points:
        if old <= 0:
            continue
        best = min(best, max(lux / old, old / lux))
    return best


def note(curve, lux, steady):
    """The one line that says whether pressing ENTER now is worth anything."""
    factor = factor_to_nearest(curve, lux)
    if not steady:
        return "Licht noch in Bewegung"
    if not curve.points:
        return "erster Punkt - passt"
    if factor < TEACH_MIN_FACTOR:
        return "Faktor %.1f - noch zu nah dran" % factor
    return "Faktor %.1f - gut" % factor


def advice(curve):
    """Which way the light has to go for the next point to be worth taking."""
    lows = [lux for lux, _ in curve.points if lux > 0]
    darkest, brightest = min(lows), max(lows)
    span = math.log10(brightest / darkest) if darkest > 0 else 0.0

    if span >= TEACH_GOOD_DECADES:
        return ("    Der Abstand reicht schon. Ein Punkt dazwischen macht die "
                "Kurve\n    genauer - oder brich mit Strg-C ab.")
    if brightest < 3.0:
        return ("    Jetzt bitte deutlich HELLER: Rollo auf, Deckenlicht an, oder\n"
                "    warte auf den Tag. Mindestens Faktor %d gegenueber %.3g lx."
                % (TEACH_MIN_FACTOR, brightest))
    if darkest > 0.5:
        return ("    Jetzt bitte deutlich DUNKLER: Rollo zu, Lampen aus, oder\n"
                "    warte auf den Abend. Mindestens Faktor %d unter %.3g lx."
                % (TEACH_MIN_FACTOR, darkest))
    return ("    Jetzt ein Licht dazwischen oder ausserhalb - Hauptsache "
            "mindestens\n    Faktor %d von jedem bisherigen Punkt entfernt "
            "(%.3g bis %.3g lx sind belegt)." % (TEACH_MIN_FACTOR, darkest, brightest))


def spread_line(curve):
    lows = [lux for lux, _ in curve.points if lux > 0]
    if len(lows) < 2:
        return "%d Punkt, noch keine Spanne" % len(lows)
    span = math.log10(max(lows) / min(lows))
    return ("%d Punkte ueber %.2f Dekaden (%.3g bis %.3g lx), Steigung %s"
            % (len(curve.points), span, min(lows), max(lows),
               "gefittet" if curve.fitted else "noch behalten"))


def enough(curve):
    lows = [lux for lux, _ in curve.points if lux > 0]
    if len(lows) < 3:
        return False
    return math.log10(max(lows) / min(lows)) >= TEACH_GOOD_DECADES


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("address")
    parser.add_argument("was", nargs="?", default="regeln", choices=["regeln", "lernen"],
                        help="regeln (Vorgabe) oder lernen - der gefuehrte Kalibrierlauf")
    parser.add_argument("--fresh", action="store_true",
                        help="beim Lernen mit einer leeren Kurve anfangen")
    parser.add_argument("--zero", action="store_true",
                        help="einmal kurz dunkel schalten und die Kompensation pruefen")
    parser.add_argument("--zero-every", type=float, default=0.0, metavar="MINUTEN",
                        help="die Kompensation regelmaessig gegen die Dunkelmessung pruefen")
    parser.add_argument("--dry", action="store_true",
                        help="nur zuschauen, die Helligkeit nicht stellen")
    parser.add_argument("--log", dest="log_path", default=None,
                        help="jede Zeile zusaetzlich als CSV anhaengen")
    args = parser.parse_args()

    try:
        if args.was == "lernen":
            teach(args.address, fresh=args.fresh)
        else:
            regulate(args.address, args.zero, args.zero_every, args.dry, args.log_path)
    except KeyboardInterrupt:
        print("\nBeendet. Die Uhr behaelt die zuletzt gestellte Helligkeit.")


if __name__ == "__main__":
    main()
