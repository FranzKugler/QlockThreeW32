# -*- coding: utf-8 -*-
"""The display's colour/output model, on this side of the network.

The automatic brightness fits `percent = a*log10(lux) + b`. Percent is the
wrong dependent variable: the eye responds to light, and how much light a
percentage produces depends on the colour. This clock's own measurements say
so - the same setting in full blue emits about **0.99 decades** less than in
the green it runs, and in percent that correction is neither an offset nor a
factor, while in log emitted light it is a constant to within 2 %.

So this module builds the coordinate the fit should be in:

    d(p)     = gammaScale(percent)                  # the driver's gamma 2.2
    R,G,B    = CRGB(CHSV(hue, sat, 255))            # FastLED's *rainbow* wheel
    drive_c  = (c * d + 127) / 255                  # _brightnessScaleColor()
    Y        = wr*r(drive_r) + wg*r(drive_g) + wb*r(drive_b)
    z        = log10(max(Y, floor))

`r()` is the measured LED drive response - the table `scripts/lab.py calibrate`
produces and `Coupling` stores, because the drive response is neither linear
nor a gamma and the dim hours live in the part of it that is worst.

Nothing here talks to a clock. It reads a saved `GET /luminance` snapshot and a
saved coupling record, so the model can be argued with before any of it is
compiled into firmware.

**Every constant is copied from a named source file**, and where a number could
have been re-derived it was not: the point of this module is to agree with the
firmware exactly, so a difference in rounding is a difference in the model.

    python3 scripts/colour_luminance.py compare --luminance snapshot.json \
        --coupling coupling.json --colours 140/80,240/100 --csv out.csv
"""

import io
import json
import math

# --------------------------------------------------------------------------
# The optical stack is part of the photometry, and therefore part of the
# identity of every calibration. `LED -> cell -> diffuser -> letter mask` is
# what the clock this was written for has today; a build that puts the
# diffuser in front of the mask is a different device photometrically, and its
# white curve, weights and coupling map are not transferable until an A/B
# measurement says they are. Hence a tag on everything this produces rather
# than a comment in a notebook.
# --------------------------------------------------------------------------
STACK_ID_DEFAULT = "current-diffuser-before-mask"


# ==========================================================================
# FastLED 3.9.15, hsv2rgb.cpp - the *rainbow* wheel
#
# `CRGB(CHSV(h, s, v))` calls `hsv2rgb_rainbow` (crgb.h, the CHSV constructor),
# and that is what `setColorHS()` writes. It is not geometric HSV and not sRGB
# HSV: pure yellow comes out near (171, 171, 0) because the wheel spends more
# of itself on the colours the eye separates, and the desaturation branch adds
# a brightness floor rather than mixing towards white linearly.
#
# The integer scaling below is FastLED's own. On the ESP32 `SCALE8_C` is 1 (it
# is not AVR) and `FASTLED_SCALE8_FIXED` is 1 (fastled_config.h), which is the
# `(i * (1 + s)) >> 8` form. `scale8_video` is deliberately *not* the fixed
# form - it adds one instead - and mixing the two up moves the floor of every
# desaturated colour by a count.
# ==========================================================================

K255, K171, K170, K85 = 255, 171, 170, 85


def scale8(i, scale):
    """FastLED `scale8` / `scale8_LEAVING_R1_DIRTY` with FASTLED_SCALE8_FIXED."""
    return ((i & 0xFF) * (1 + (scale & 0xFF))) >> 8


def scale8_video(i, scale):
    """FastLED `scale8_video` - never zero unless an input is."""
    i &= 0xFF
    scale &= 0xFF
    return ((i * scale) >> 8) + (1 if (i and scale) else 0)


def hsv2rgb_rainbow(hue, sat, val=255):
    """`CRGB(CHSV(hue, sat, val))`, byte for byte.

    A transcription of FastLED 3.9.15 hsv2rgb.cpp with Y1=1, Y2=0, G2=0 and
    Gscale=0 - the library defaults, which is what the firmware links against.
    Checked against the library itself in tests/golden/.
    """
    hue &= 0xFF
    sat &= 0xFF
    val &= 0xFF

    offset = hue & 0x1F                     # 0..31
    offset8 = (offset << 3) & 0xFF
    third = scale8(offset8, 256 // 3)       # max 85

    if not (hue & 0x80):
        if not (hue & 0x40):
            if not (hue & 0x20):
                # 000: R -> O
                red, green, blue = K255 - third, third, 0
            else:
                # 001: O -> Y (Y1)
                red, green, blue = K171, K85 + third, 0
        else:
            if not (hue & 0x20):
                # 010: Y -> G (Y1)
                twothirds = scale8(offset8, (256 * 2) // 3)
                red, green, blue = K171 - twothirds, K170 + third, 0
            else:
                # 011: G -> A
                red, green, blue = 0, K255 - third, third
    else:
        if not (hue & 0x40):
            if not (hue & 0x20):
                # 100: A -> B
                twothirds = scale8(offset8, (256 * 2) // 3)
                red, green, blue = 0, K171 - twothirds, K85 + twothirds
            else:
                # 101: B -> P
                red, green, blue = third, 0, K255 - third
        else:
            if not (hue & 0x20):
                # 110: P -> K
                red, green, blue = K85 + third, 0, K171 - third
            else:
                # 111: K -> R
                red, green, blue = K170 + third, 0, K85 - third

    if sat != 255:
        if sat == 0:
            red = green = blue = 255
        else:
            desat = scale8_video(255 - sat, 255 - sat)
            satscale = 255 - desat
            red = scale8(red, satscale)
            green = scale8(green, satscale)
            blue = scale8(blue, satscale)
            red += desat
            green += desat
            blue += desat

    if val != 255:
        val = scale8_video(val, val)
        if val == 0:
            red = green = blue = 0
        else:
            red = scale8(red, val)
            green = scale8(green, val)
            blue = scale8(blue, val)

    return (red & 0xFF, green & 0xFF, blue & 0xFF)


# ==========================================================================
# The clock's units - src/main .cpp, the setColorHS() call
# ==========================================================================

# --------------------------------------------------------------------------
# Shape, before arithmetic
#
# `json.load` is happy with a list where a mapping belongs, a number where a
# list belongs and a string where a number belongs - all of it valid JSON and
# none of it a record. Each of those used to reach arithmetic and come back as
# an AttributeError or a TypeError, which the command line cannot turn into a
# message: it catches what this module refuses on purpose, and catching
# TypeError broadly would swallow real faults in this module as well. So the
# shape is checked where a record enters, once, by name.
# --------------------------------------------------------------------------

def _mapping(name, value):
    """A JSON object, not a list that happens to be indexable."""
    if not isinstance(value, dict):
        raise ValueError("%s is %s; a JSON object was expected"
                         % (name, _shape_of(value)))
    return value


def _sequence(name, value):
    """A JSON array. A string is iterable and is not one."""
    if isinstance(value, (str, bytes)) or not isinstance(value, (list, tuple)):
        raise ValueError("%s is %s; a JSON array was expected"
                         % (name, _shape_of(value)))
    return value


def _number(name, value):
    """A finite JSON number. `true` is not one, however much Python agrees."""
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError("%s is %s; a number was expected"
                         % (name, _shape_of(value)))
    number = float(value)
    if not math.isfinite(number):
        raise ValueError("%s is %r; it has to be finite" % (name, value))
    return number


def _shape_of(value):
    """What arrived, for a message that names it rather than repeating it."""
    if value is None:
        return "null"
    return {dict: "a JSON object", list: "a JSON array", tuple: "a JSON array",
            str: "a string", bool: "a boolean"}.get(type(value), repr(value))


def _bounded(name, value, low, high):
    """A whole number inside its range, or a refusal naming the range.

    Everything downstream of here wraps rather than complains - FastLED masks
    the hue to eight bits, so hue 400 is a colour and not an error - and a
    wrong answer that looks like an answer is the failure this module exists
    to avoid.
    """
    number = _number(name, value)
    if number != int(number):
        raise ValueError("%s must be a whole number, not %r" % (name, value))
    number = int(number)
    if not low <= number <= high:
        raise ValueError("%s is %r; it has to be %d..%d" % (name, value, low, high))
    return number


def _check_weights(weights):
    """Exactly three finite, non-negative weights that add up to something."""
    values = list(weights)
    if len(values) != 3:
        raise ValueError("the photopic weights are one per channel: three, not %d"
                         % len(values))
    numbers = []
    for name, value in zip(("red", "green", "blue"), values):
        number = _number("the %s weight" % name, value)
        if number < 0.0:
            raise ValueError("the %s weight is %r; it has to be not negative"
                             % (name, value))
        numbers.append(number)
    if sum(numbers) <= 0.0:
        raise ValueError("the photopic weights add up to %r, so no colour emits "
                         "anything" % sum(numbers))
    return tuple(numbers)


def hue_byte(degrees):
    """0..359 to FastLED's 0..255, rounded the way the firmware rounds it."""
    return int((_bounded("hue", degrees, 0, 359) * 255 + 179) // 359)


def sat_byte(percent):
    """0..100 to 0..255, likewise."""
    return int((_bounded("saturation", percent, 0, 100) * 255 + 50) // 100)


def display_rgb(hue_degrees, sat_percent):
    """What the strip is told the display colour is, at full value.

    `setColorHS()` converts at value 255 and the brightness is applied per
    channel afterwards - see `channel_drive()`. Doing it in one step instead
    would round differently and would not be what the sensor sees.
    """
    return hsv2rgb_rainbow(hue_byte(hue_degrees), sat_byte(sat_percent), 255)


# ==========================================================================
# The driver's gamma - src/LedDriverWS2812FastLED.cpp, _gammaScale()
# ==========================================================================

def gamma_scale(percent):
    """The setting as a drive value, with gamma 2.2.

    Copied from `_gammaScale()` rather than re-derived. The floor of 1 is
    load-bearing: without it 1..3 % rounds to zero and the clock goes dark
    while the web UI says it is on.
    """
    percent = int(percent)
    if percent <= 0:
        return 0
    if percent >= 100:
        return 255
    value = int(round(255.0 * (percent / 100.0) ** 2.2))
    return max(1, value)


def channel_drive(component, scaled):
    """One channel of the display colour once the brightness is on it.

    `_brightnessScaleColor()`: `(part * brightnessScaled + 127) / 255`, integer
    division. This is the number that reaches the LED, and therefore the number
    the drive-response table is asked about - `Coupling` reads exactly these
    bytes back off the driver's pixels.
    """
    return (int(component) * int(scaled) + 127) // 255


# ==========================================================================
# The measured drive response - src/Coupling.cpp responseFor(),
# scripts/lab.py response()
# ==========================================================================

# `COUPLING_MAX_LEVELS` in src/Coupling.h: a longer table is truncated on the
# clock, so a longer one here would describe a lamp the firmware is not using.
COUPLING_MAX_LEVELS = 20


class DriveTable(object):
    """What comes out of an LED at a given eight-bit drive value.

    Neither linear nor a gamma: half drive gives 0.48 of full, a quarter 0.216,
    an eighth 0.082, and 16 gives 0.024 where a proportional lamp would give
    0.063. An offset model held from 255 down to 24 and then broke completely,
    so it is a table - and it is one table for every channel and cell, which
    was checked: white, red, green and blue gave the same curve to a few parts
    in a thousand, so it belongs to the LED and its driver.

    `levels` descend from 255, `response` is the fraction of full each gives.
    The interpolation is `Coupling::responseFor()`'s, down to the last branch:
    below the lowest measured level it goes straight down to zero rather than
    holding flat, because the clock spends its evenings down there and a floor
    would be a lie about the dimmest light it emits.
    """

    def __init__(self, levels, response, assumed=False, source=None):
        levels = _sequence("the drive levels", levels)
        response = _sequence("the drive response", response)
        if not levels or len(levels) != len(response):
            raise ValueError(
                "the drive table needs matching, non-empty levels and response "
                "(got %d levels and %d responses)" % (len(levels), len(response)))
        if len(levels) > COUPLING_MAX_LEVELS:
            raise ValueError(
                "%d levels, but the firmware keeps at most %d (COUPLING_MAX_LEVELS "
                "in src/Coupling.h) and would silently drop the rest"
                % (len(levels), COUPLING_MAX_LEVELS))

        levels = [_bounded("the drive level at position %d" % i, v, 0, 255)
                  for i, v in enumerate(levels)]
        response = [_number("the drive response at position %d" % i, v)
                    for i, v in enumerate(response)]

        # Descending, strictly. `Coupling::responseFor()` walks the pairs
        # looking for the one that brackets a value and divides by the span; a
        # repeated or ascending level makes it answer something plausible for
        # the wrong bracket, which is worse than an error.
        for higher, lower in zip(levels, levels[1:]):
            if lower >= higher:
                raise ValueError(
                    "the drive levels must descend strictly, and %d does not "
                    "come below %d" % (lower, higher))
        if not 1 <= levels[0] <= 255:
            raise ValueError("the top drive level is %d; it has to be 1..255"
                             % levels[0])
        # The last one divides in the branch below the lowest measurement.
        if levels[-1] <= 0:
            raise ValueError(
                "the lowest drive level is %d; below the last measurement the "
                "table goes straight down to zero and divides by it" % levels[-1])

        for level, value in zip(levels, response):
            if value < 0.0:
                raise ValueError("the response at drive %d is %r; it has to be "
                                 "not negative" % (level, value))
        if response[0] <= 0.0:
            raise ValueError(
                "the response at full drive is %r, so the whole table is zero "
                "and nothing can be relative to it" % response[0])

        self.levels = levels
        self.response = response
        # Whether these were measured on a clock or invented to have something
        # to run with. A model quoting numbers off an assumed table has to say
        # so, or a guess reads exactly like a measurement.
        self.assumed = bool(assumed)
        self.source = source

    @classmethod
    def from_record(cls, record, source=None):
        """From the coupling record `lab.py calibrate` writes and NVS holds."""
        record = _mapping("the coupling record", record)
        if "drive" not in record:
            raise ValueError("no drive table in this record: expected {'drive': "
                             "{'levels': [...], 'response': [...]}}")
        drive = _mapping("the drive table", record["drive"])
        if "levels" not in drive or "response" not in drive:
            raise ValueError("the drive table needs both 'levels' and "
                             "'response'")
        return cls(drive["levels"], drive["response"], assumed=False, source=source)

    @classmethod
    def proportional(cls):
        """The fallback when no clock has been calibrated: light with drive.

        Wrong, and knowably wrong - 22 % out at quarter drive and far worse
        below - but a model has to be runnable before a measurement exists.
        `assumed` is set so nothing quotes it as fact.
        """
        # Levels 255 and 1 rather than 255 and 0: the bottom level divides, and
        # the straight line through (1, 1/255) and (255, 1) *is* value / 255.
        return cls([255, 1], [1.0, 1.0 / 255.0], assumed=True, source="proportional")

    def response_for(self, value):
        value = int(value)
        if value <= 0:
            return 0.0
        if value >= self.levels[0]:
            return self.response[0]
        for i in range(len(self.levels) - 1):
            high, low = self.levels[i], self.levels[i + 1]
            if low <= value <= high:
                span = float(high - low)
                if span <= 0.0:
                    return self.response[i]
                return (self.response[i + 1]
                        + (self.response[i] - self.response[i + 1]) * (value - low) / span)
        return self.response[-1] * value / float(self.levels[-1])


# ==========================================================================
# The coordinate the fit should be in
# ==========================================================================

# Relative luminance weights, Rec.709/sRGB. **A prior, not a characterisation**
# of this batch of WS2812B behind this front panel: without a spectrometer they
# are the physically grounded starting point, and they can be refined from
# measurements later. They already agree with the one measurement this clock
# has: log10(0.7152 / 0.0722) is 0.996 decades, and full blue was measured 0.99
# decades below the green the face runs.
#
# The light *sensor* cannot supply better ones. The TSL2591 is broadband, not
# photopic - its own coupling coefficients weight the channels 35/40/25 where
# the eye weights them 21/72/7 - so it sees blue three and a half times more
# strongly than a person does. Colour compensation has to come from luminance
# weights on the RGB, never from this clock's own sensor readings.
PHOTOPIC_WEIGHTS = (0.2126, 0.7152, 0.0722)

# Below this, `log10` has nothing useful to say and the face is off anyway.
OUTPUT_FLOOR = 1e-12


def log10_output(output):
    """Log of the relative output, floored so a dark face is a number."""
    return math.log10(max(float(output), OUTPUT_FLOOR))


def relative_photopic_output(hue, sat, percent, weights=PHOTOPIC_WEIGHTS,
                             drive=None):
    """What the face emits at this colour and this slider setting.

    Relative to white at 100 %, which is 1.0 - so the number reads as a
    fraction of everything the face can do, and a colour that cannot reach a
    white target says so by arithmetic rather than by convention.

    The path is the firmware's, in the firmware's order: the wheel at full
    value, the gamma on the setting, the per-channel scaling, and only then the
    measured drive response. Doing the arithmetic in any other order rounds
    differently, and at 20 % - a drive of seven - a count is several per cent.
    """
    if drive is None:
        drive = DriveTable.proportional()
    weights = _check_weights(weights)

    red, green, blue = display_rgb(hue, sat)
    scaled = gamma_scale(_bounded("percent", percent, 0, 100))

    total = 0.0
    for component, weight in zip((red, green, blue), weights):
        total += weight * drive.response_for(channel_drive(component, scaled))
    return total


class Inversion(object):
    """What the inverse found, and whether it had to give up on the way.

    `limited` is None when the target was reached inside the range, "ceiling"
    when even the top of the range emits less than was asked for - deep blue
    against a white-equivalent target, which is a gamut fact and not a fault -
    and "floor" when the bottom of the range already emits more.
    """

    __slots__ = ("percent", "log_output", "target", "limited")

    def __init__(self, percent, log_output, target, limited):
        self.percent = percent
        self.log_output = log_output
        self.target = target
        self.limited = limited

    @property
    def error(self):
        """Decades between what was asked for and what this percentage does."""
        return self.log_output - self.target

    def __repr__(self):
        return "Inversion(percent=%d, log_output=%.4f, target=%.4f, limited=%r)" % (
            self.percent, self.log_output, self.target, self.limited)


def percent_for_output(target, hue, sat, weights=PHOTOPIC_WEIGHTS, drive=None,
                       low=1, high=100):
    """The slider percentage that comes nearest to `target` in log output.

    A bounded search over integer percentages rather than a closed form, and
    deliberately so: the gamma rounds, the per-channel scaling rounds again and
    the drive table is interpolated between measured points, so an algebraic
    inverse would be a fiction that disagrees with the forward direction at
    exactly the settings the clock spends its evenings at.

    Ties go to the **lowest** percentage. Several percentages produce the same
    drive at the bottom of the curve - 1, 2 and 3 all come out as a drive of
    one - and when they are indistinguishable in light the dimmest is the
    honest answer.
    """
    # An infinity or a NaN would win or lose every comparison below and come
    # back as a percentage with no meaning behind it.
    target = _number("the target output", target)

    low = _bounded("the bottom of the range", low, 0, 100)
    high = _bounded("the top of the range", high, 0, 100)
    if low > high:
        raise ValueError("the range is empty: low %d above high %d" % (low, high))

    best_percent = low
    best_z = log10_output(relative_photopic_output(hue, sat, low, weights, drive))
    best_gap = abs(best_z - target)
    lowest_z, highest_z = best_z, best_z

    for percent in range(low + 1, high + 1):
        z = log10_output(relative_photopic_output(hue, sat, percent, weights, drive))
        highest_z = max(highest_z, z)
        lowest_z = min(lowest_z, z)
        gap = abs(z - target)
        if gap < best_gap:
            best_percent, best_z, best_gap = percent, z, gap

    limited = None
    if target > highest_z:
        limited = "ceiling"
    elif target < lowest_z:
        limited = "floor"
    return Inversion(best_percent, best_z, target, limited)


# ==========================================================================
# The stored points - src/Luminance.h, and GET /luminance
# ==========================================================================

# `LUM_HUE_UNKNOWN` in src/Luminance.h. Not zero, deliberately: hue 0 is red,
# and a point silently claiming to have been taught in red is worse than one
# that admits it does not know.
HUE_UNKNOWN = 0xFFFF

# `LUX_FLOOR` in src/LightSensor.h, applied by `Luminance::logLux()`.
LUX_FLOOR = 0.01


class UnknownColourError(ValueError):
    """A point kept before the colour was, asked to be transformed anyway.

    Refused rather than guessed. The colour decides how much light a
    percentage was, so an assumed colour is an assumed measurement - and the
    one assumption nobody may make is red, which is what hue 0 would be.
    """


class Point(object):
    """One calibration point as the clock keeps it."""

    __slots__ = ("lux", "percent", "hue", "sat", "seconds", "reported_used", "index")

    def __init__(self, lux, percent, hue=HUE_UNKNOWN, sat=0, seconds=0,
                 reported_used=True, index=None):
        self.lux = _number("the light at a point", lux)
        if self.lux < 0.0:
            raise ValueError("a point at %r lx: light is not negative" % (lux,))
        self.percent = _bounded("percent", percent, 0, 100)
        # HUE_UNKNOWN is not out of range: it is the value that says the colour
        # was never stored, and it has to survive being read back.
        self.hue = (HUE_UNKNOWN
                    if _number("hue", hue) == HUE_UNKNOWN
                    else _bounded("hue", hue, 0, 359))
        self.sat = _bounded("saturation", sat, 0, 100)
        self.seconds = int(_number("the age of a point", seconds))
        # What the **clock** said, against the clock's own range at the moment
        # the snapshot was taken. Kept because a disagreement with what this
        # report works out is worth seeing - and never used as an input, because
        # this report fits its own lines, over a range that may have been given
        # on the command line.
        self.reported_used = bool(reported_used)
        # Its position in the clock's own oldest-first array, because that is
        # how `POST /luminance {forget: n}` addresses it and sorting a display
        # must not renumber it.
        self.index = index

    @property
    def colour_known(self):
        return self.hue != HUE_UNKNOWN

    def at_ceiling(self, max_percent):
        """Censored, against the range being used *now*.

        "100 %" means "at least 100 %": the slider had nothing above the
        maximum to offer, and least squares reading it as an equality drags the
        bright end of the line down. Which points that catches depends on where
        the ceiling is, so it is an argument rather than a stored flag - the
        one in the snapshot answers a question about a different range.
        """
        return self.percent >= max_percent

    def __repr__(self):
        return "Point(lux=%.4f, percent=%d, hue=%s, sat=%d)" % (
            self.lux, self.percent,
            "unknown" if self.hue == HUE_UNKNOWN else self.hue, self.sat)


def points_from_luminance(doc):
    """The points out of a `GET /luminance` answer, oldest first.

    `hue`/`sat` are **absent** rather than null on a point from before the
    colour was kept, which is how a reader tells "taught in red" from "we do
    not know" - so a missing key becomes `HUE_UNKNOWN` and nothing else.
    """
    doc = _mapping("the snapshot", doc)
    points = []
    for index, entry in enumerate(_sequence("points", doc.get("points", []))):
        entry = _mapping("point %d" % index, entry)
        has_colour = "hue" in entry and entry["hue"] is not None
        points.append(Point(
            lux=_number("the light at point %d" % index, entry.get("lux", 0.0)),
            percent=_bounded("the percent at point %d" % index,
                             entry.get("percent", 0), 0, 100),
            hue=(_bounded("the hue at point %d" % index, entry["hue"], 0, 359)
                 if has_colour else HUE_UNKNOWN),
            sat=(_bounded("the saturation at point %d" % index,
                          entry.get("sat", 0), 0, 100) if has_colour else 0),
            seconds=_number("the age at point %d" % index, entry.get("seconds", 0)),
            reported_used=bool(entry.get("used", True)),
            index=index))
    return points


def log_lux(lux):
    """`Luminance::logLux()`: floored, then log10."""
    value = float(lux)
    if not value > LUX_FLOOR:
        value = LUX_FLOOR
    return math.log10(value)


class TransformedPoint(object):
    """A point in the coordinates the fit should be in: (log lux, log output)."""

    __slots__ = ("point", "x", "z", "assumed_colour", "hue", "sat")

    def __init__(self, point, x, z, hue, sat, assumed_colour=False):
        self.point = point
        self.x = x
        self.z = z
        self.hue = hue
        self.sat = sat
        # True when the colour came from the caller rather than from the clock.
        # Carried so a report can say which numbers rest on an assumption.
        self.assumed_colour = assumed_colour

    def __repr__(self):
        return "TransformedPoint(x=%.4f, z=%.4f, %s)" % (
            self.x, self.z, "assumed" if self.assumed_colour else "measured")


def transform_point(point, weights=PHOTOPIC_WEIGHTS, drive=None, assume=None):
    """`(lux, percent, hue, sat)` to `(log10 lux, log10 relative output)`.

    `assume=(hue, sat)` transforms a point whose colour was never stored, for
    the case where the provenance is known from outside the record - a clock
    that has only ever run white, say. It is an argument rather than a default
    because the assumption belongs to whoever knows it, and it is remembered on
    the result so a report can mark it.
    """
    if not point.colour_known:
        if assume is None:
            raise UnknownColourError(
                "point at %.4f lx was taught before the colour was kept; pass "
                "assume=(hue, sat) if the provenance is known from elsewhere"
                % point.lux)
        hue, sat = int(assume[0]), int(assume[1])
        assumed = True
    else:
        hue, sat, assumed = point.hue, point.sat, False

    output = relative_photopic_output(hue, sat, point.percent, weights, drive)
    return TransformedPoint(point, log_lux(point.lux), log10_output(output),
                            hue, sat, assumed_colour=assumed)


# ==========================================================================
# The fit - src/Luminance.cpp fit()
# ==========================================================================

# Decades of spread the points must cover before a slope is fitted at all.
LUM_FIT_MIN_DECADES = 0.6
# The regulated range a clock defaults to, LUM_MIN_PERCENT / LUM_MAX_PERCENT.
LUM_MIN_PERCENT = 20
LUM_MAX_PERCENT = 100


class NoSlopeError(ValueError):
    """The points cannot carry a slope and none was supplied.

    Kept apart from the other refusals so `compare()` can turn it into "this
    fit is unavailable, and here is why" rather than into a line whose slope
    came from nowhere.
    """


class Line(object):
    """`y = slope * x + offset`, and whether the slope was fitted or kept."""

    __slots__ = ("slope", "offset", "fitted", "spread", "count", "prior_slope")

    def __init__(self, slope, offset, fitted, spread=0.0, count=0,
                 prior_slope=False):
        self.slope = float(slope)
        self.offset = float(offset)
        # False means the slope is the one that was handed in: either the
        # points did not span enough light to say anything about steepness, or
        # what they did say was zero or negative.
        self.fitted = bool(fitted)
        # True when the slope came in from outside rather than out of these
        # points - the clock's own stored slope, or one given on the command
        # line. Reported, because a line with a borrowed slope is a different
        # claim from one the data supports.
        self.prior_slope = bool(prior_slope)
        self.spread = float(spread)
        self.count = int(count)

    def at(self, x):
        return self.slope * x + self.offset

    def __repr__(self):
        return "Line(slope=%.4f, offset=%.4f, fitted=%s, spread=%.3f decades, n=%d)" % (
            self.slope, self.offset, self.fitted, self.spread, self.count)


def censor(points, min_percent=LUM_MIN_PERCENT, max_percent=LUM_MAX_PERCENT):
    """Which points belong in a fit: everything below the ceiling.

    A point at the top of the range is *censored* - the slider had nothing
    above the maximum to offer, so "100 %" means "at least 100 %", and least
    squares reading it as an equality flattens the slope. The floor is
    deliberately not treated the same way: it is a number the owner chose, so a
    point sitting on it is a preference being met.

    Unless leaving them out leaves fewer than two, where a poor line beats no
    line at all.
    """
    used = [point.percent < max_percent for point in points]
    if sum(1 for flag in used if flag) < 2:
        used = [True] * len(points)
    return used


def fit_line(xs, ys, keep_slope=None, min_decades=LUM_FIT_MIN_DECADES):
    """Least squares through the points, with the firmware's three guards.

    Every point weighted the same, and age deliberately not a weight: somebody
    setting the brightness by eye is guessing, and guessing differently each
    time, so ten statements about a room are worth more than the last one. The
    cost - that a correction does not land exactly where it was asked for - is
    the trade this project has already made once in each direction and settled.

    `keep_slope` is what stands when the points cannot support a slope. On the
    clock that is the line already in use; offline it has to be supplied by
    whoever knows one, in the units of *this* fit. There is no default: a slope
    nobody can name the source of is not a fallback, it is an invention.
    """
    count = len(xs)
    if count != len(ys):
        raise ValueError("as many x as y, please")
    if count == 0:
        raise ValueError("nothing to fit")

    mean_x = sum(xs) / float(count)
    mean_y = sum(ys) / float(count)
    spread = max(xs) - min(xs)

    slope = keep_slope
    fitted = spread >= min_decades
    if fitted:
        top = sum((x - mean_x) * (y - mean_y) for x, y in zip(xs, ys))
        bottom = sum((x - mean_x) ** 2 for x in xs)
        candidate = (top / bottom) if bottom > 0.0 else 0.0
        # A slope of zero or less is refused: darker room, brighter clock is
        # not a thing anybody wants, and one careless nudge in daylight
        # produces it.
        if candidate > 0.0:
            slope = candidate
        else:
            fitted = False
    if slope is None:
        raise NoSlopeError(
            "the points spread %.2f decades, %.1f are needed to fit a slope, "
            "and no prior slope was supplied" % (spread, min_decades))
    if not fitted:
        if not math.isfinite(slope) or slope <= 0.0:
            raise ValueError("the prior slope is %r; it has to be finite and "
                             "above zero" % (slope,))

    # Through the centroid, whatever the slope turned out to be. With a fitted
    # slope that is the least-squares line; with a kept one it is the same line
    # slid up or down to sit among the points.
    return Line(slope, mean_y - slope * mean_x, fitted, spread, count,
                prior_slope=not fitted)


# ==========================================================================
# The offline comparison
# ==========================================================================

# How many lux samples a predicted curve is drawn at, spread evenly in log
# light over whatever range the points cover.
CURVE_STEPS = 13


def _fit_or_none(xs, ys, prior):
    """The line, or None and the reason there is not one."""
    try:
        return fit_line(xs, ys, keep_slope=prior), None
    except NoSlopeError as problem:
        return None, str(problem)


def _group_key(moved):
    """White is one colour whatever hue is stored beside it.

    At zero saturation the wheel answers (255, 255, 255) for every hue, so
    grouping white by hue would split one lighting condition into four.
    """
    return (0, 0) if moved.sat == 0 else (moved.hue, moved.sat)


def compare(snapshot, drive=None, weights=PHOTOPIC_WEIGHTS, colours=None,
            stack_id=STACK_ID_DEFAULT, assume=None, min_percent=None,
            max_percent=None, output_prior_slope=None):
    """Fit both models to the same points and say what each makes of them.

    The question this answers is narrow and worth keeping narrow: *on the
    points this clock already has*, does re-expressing the dependent variable
    as log relative output leave smaller residuals than fitting raw percent,
    and does each colour need one offset or something more? It does not
    identify a hue/saturation surface, and nothing here pretends it can.

    **Either fit can come back unavailable**, and that is a result rather than
    a failure. Points made in one room at one time of day carry no slope, and
    the two models differ in what there is to fall back on: the percent line
    can keep the clock's own stored slope, which is a real number in the right
    units, while the output line has nothing - the stored slope is percent per
    decade and this one is decades of light per decade of light. Supply
    `output_prior_slope` if a slope is known from somewhere; otherwise the
    output fit and everything computed from it are reported as unavailable.
    """
    if drive is None:
        drive = DriveTable.proportional()

    if output_prior_slope is not None:
        output_prior_slope = _number("the output prior slope", output_prior_slope)
        if output_prior_slope <= 0.0:
            raise ValueError("the output prior slope is %r; it has to be finite "
                             "and above zero - a darker room asking for a "
                             "brighter clock is not a thing anybody wants"
                             % output_prior_slope)

    points = points_from_luminance(snapshot)
    if not points:
        raise ValueError("this snapshot holds no points")

    weights = _check_weights(weights)
    low = _bounded("minPercent", min_percent if min_percent is not None
                   else snapshot.get("minPercent", LUM_MIN_PERCENT), 0, 100)
    high = _bounded("maxPercent", max_percent if max_percent is not None
                    else snapshot.get("maxPercent", LUM_MAX_PERCENT), 0, 100)
    if low > high:
        raise ValueError("the regulated range is empty: %d %% up to %d %%"
                         % (low, high))

    warnings = []
    if drive.assumed:
        warnings.append(
            "the drive response is assumed proportional, not measured: run "
            "`scripts/lab.py <clock> calibrate` and pass --coupling, or every "
            "number below is wrong by up to a factor of three at low settings")

    # The transformation comes first, because **which points can be censored
    # depends on which points are in the fit at all**. Censoring counted over
    # every point lets a point that the output fit cannot even use - one with
    # no colour stored - make a ceiling point look dispensable, and the output
    # fit is then left with one point, or with none. The two fits have
    # different populations, so each gets its own censoring pass over its own.
    transformed = []
    for point in points:
        try:
            transformed.append(transform_point(point, weights, drive, assume))
        except UnknownColourError:
            transformed.append(None)
            warnings.append(
                "point %d (%.4f lx at %d %%) has an unknown colour and is left "
                "out of the output fit; it is not assumed to be red"
                % (point.index, point.lux, point.percent))

    used_percent = censor(points, low, high)
    known = [p for p, m in zip(points, transformed) if m is not None]
    used_output = dict(zip([p.index for p in known], censor(known, low, high)))

    for point in points:
        if point.at_ceiling(high):
            kept_anyway = [name for name, membership in
                           (("percent", used_percent[point.index]),
                            ("output", used_output.get(point.index, False)))
                           if membership]
            warnings.append(
                "point %d (%.4f lx at %d %%) is censored: it sits at the "
                "ceiling, so it says \"at least %d %%\" and is not an equality%s"
                % (point.index, point.lux, point.percent, high,
                   "" if not kept_anyway else
                   " - kept in the %s fit all the same, because censoring it "
                   "would leave fewer than two points there"
                   % " and ".join(kept_anyway)))

    # --- the model the clock runs today: percent against log lux -----------
    percent_xs = [log_lux(p.lux) for p in points if used_percent[p.index]]
    percent_ys = [float(p.percent) for p in points if used_percent[p.index]]
    # The clock's own slope is the only legitimate fallback here: it is a real
    # number, in percent per decade, and it is what the firmware itself keeps
    # when the points cannot support a new one. A snapshot without it leaves
    # nothing to fall back on.
    stored_slope = snapshot.get("slope")
    percent_prior = None
    if isinstance(stored_slope, (int, float)) and math.isfinite(stored_slope) \
            and stored_slope > 0.0:
        percent_prior = float(stored_slope)
    percent_line, percent_reason = _fit_or_none(percent_xs, percent_ys, percent_prior)

    # --- the proposed one: log relative output against log lux -------------
    moved = [(p, used_output.get(p.index, False), m)
             for p, m in zip(points, transformed)]
    output_xs = [m.x for _, keep, m in moved if keep and m is not None]
    output_zs = [m.z for _, keep, m in moved if keep and m is not None]
    if len(output_xs) < 2:
        raise ValueError("fewer than two usable points with a known colour: "
                         "nothing to fit in output space")
    # And nothing is borrowed from the percent line. Its slope is percent per
    # decade; this one is decades of light per decade of light. Dividing one by
    # a hundred is arithmetic, not a conversion, and the number it produces has
    # no source - so without a prior somebody can name, there is no line.
    output_line, output_reason = _fit_or_none(output_xs, output_zs, output_prior_slope)

    for line, reason, name in ((percent_line, percent_reason, "percent"),
                               (output_line, output_reason, "output")):
        if line is None:
            warnings.append(
                "the %s fit is unavailable: %s. Corrections made in one room at "
                "one time of day say nothing about steepness"
                % (name, reason))
        elif line.prior_slope:
            warnings.append(
                "the %s fit kept a slope it was given rather than fitting one: "
                "%.2f decades of spread, and %.1f are needed. Only the level "
                "comes from these points" % (name, line.spread, LUM_FIT_MIN_DECADES))

    # --- what each model makes of each point -------------------------------
    rows = []
    for (point, keep, transformed) in moved:
        x = log_lux(point.lux)
        row = {
            "index": point.index,
            "lux": point.lux,
            "logLux": x,
            "percent": point.percent,
            "hue": None if not point.colour_known else point.hue,
            "sat": None if not point.colour_known else point.sat,
            # Membership, worked out here rather than read off the snapshot,
            # and separately per fit because the two have different
            # populations.
            "usedInPercentFit": bool(used_percent[point.index]),
            "usedInOutputFit": bool(keep and transformed is not None),
            "censored": point.at_ceiling(high),
            "clockUsed": point.reported_used,
            "percentCurve": percent_line.at(x) if percent_line else None,
            "percentResidual": (point.percent - percent_line.at(x)
                                if percent_line else None),
            "logOutput": None,
            "outputCurve": None,
            "outputResidual": None,
            "predictedPercent": None,
            "predictedLimited": None,
            "assumedColour": bool(transformed.assumed_colour) if transformed else False,
        }
        if transformed is not None:
            # What this point emitted is a fact about the point and is shown
            # whether or not there is a line to compare it with.
            row["logOutput"] = transformed.z
        if transformed is not None and output_line is not None:
            target = output_line.at(x)
            found = percent_for_output(target, transformed.hue, transformed.sat,
                                       weights, drive, low, high)
            row["outputCurve"] = target
            row["outputResidual"] = transformed.z - target
            # The residual in decades is the honest unit, but nobody sets a
            # clock in decades: this is the same disagreement in the unit on
            # the slider.
            row["predictedPercent"] = found.percent
            row["predictedLimited"] = found.limited
        rows.append(row)

    # --- one offset per colour, which is all two points can carry ----------
    groups = {}
    for (_, keep, transformed) in moved:
        if transformed is None or not keep:
            continue
        key = _group_key(transformed)
        groups.setdefault(key, []).append(transformed)

    colour_rows = []
    for (hue, sat), members in sorted(groups.items()):
        # An offset is an offset *from a line*. With no line there is none, and
        # the count and the spread are still worth having.
        residuals = ([m.z - output_line.at(m.x) for m in members]
                     if output_line is not None else [])
        xs = [m.x for m in members]
        colour_rows.append({
            "hue": hue,
            "sat": sat,
            "count": len(members),
            "meanResidual": (sum(residuals) / len(residuals)) if residuals else None,
            "spreadDecades": (max(xs) - min(xs)) if len(xs) > 1 else 0.0,
            # Whether the residual holds still across light levels, which is
            # the difference between "this colour needs one offset" and "the
            # model is wrong". One point cannot say.
            "residualSpread": ((max(residuals) - min(residuals))
                               if len(residuals) > 1 else None),
        })

    # --- predicted percent curves ------------------------------------------
    lo_x, hi_x = min(output_xs), max(output_xs)
    if hi_x <= lo_x:
        hi_x = lo_x + 1.0
    curve_xs = [lo_x + (hi_x - lo_x) * i / float(CURVE_STEPS - 1)
                for i in range(CURVE_STEPS)]

    if colours is None:
        colours = sorted({(g["hue"], g["sat"]) for g in colour_rows})
    curves = []
    if output_line is None:
        # A predicted percentage is the inverse of a target, and there is no
        # target. Silence beats a curve drawn from a slope nobody chose.
        colours = []
    for hue, sat in colours:
        found = [percent_for_output(output_line.at(x), hue, sat, weights, drive, low, high)
                 for x in curve_xs]
        curves.append({
            "hue": hue,
            "sat": sat,
            "rgb": list(display_rgb(hue, sat)),
            "lux": [10.0 ** x for x in curve_xs],
            "logLux": curve_xs,
            "percent": [f.percent for f in found],
            "limited": [f.limited for f in found],
        })
        if any(f.limited == "ceiling" for f in found):
            warnings.append(
                "hue %d at %d %% saturation cannot reach the reference output "
                "at the bright end: even %d %% is short. That is the gamut, not "
                "a fault - deep blue emits about a tenth of what white does"
                % (hue, sat, high))

    return {
        "stackId": stack_id,
        "weights": list(weights),
        "driveAssumed": bool(drive.assumed),
        "driveSource": drive.source,
        "minPercent": low,
        "maxPercent": high,
        "percentFit": _fit_report(
            percent_line, percent_reason, len(percent_xs),
            _rms([r["percentResidual"] for r in rows
                  if r["usedInPercentFit"] and r["percentResidual"] is not None])),
        "outputFit": _fit_report(
            output_line, output_reason, len(output_xs),
            _rms([r["outputResidual"] for r in rows
                  if r["usedInOutputFit"] and r["outputResidual"] is not None])),
        "points": rows,
        "colours": colour_rows,
        "curves": curves,
        "warnings": warnings,
    }


def _fit_report(line, reason, count, rms):
    """One shape for a line whether there is one or not.

    `available` is the field to branch on. The others are present either way -
    as None when there is no line - so a reader that forgets to branch gets
    nothing rather than something wrong.
    """
    if line is None:
        return {"available": False, "reason": reason, "slope": None,
                "offset": None, "fitted": False, "priorSlope": False,
                "count": count, "spread": None, "rms": None}
    return {"available": True, "reason": None, "slope": line.slope,
            "offset": line.offset, "fitted": line.fitted,
            "priorSlope": line.prior_slope, "count": line.count,
            "spread": line.spread, "rms": rms}


def _rms(values):
    values = [v for v in values if v is not None]
    if not values:
        return None
    return (sum(v * v for v in values) / float(len(values))) ** 0.5


# ==========================================================================
# Output: a table to read, a CSV to plot, and the command that makes both
# ==========================================================================

CSV_COLUMNS = ["kind", "stackId", "index", "lux", "logLux", "hue", "sat",
               "percent", "usedInPercentFit", "usedInOutputFit", "clockUsed",
               "censored", "percentCurve", "percentResidual",
               "logOutput", "outputCurve", "outputResidual", "predictedPercent",
               "limited"]


def report_to_csv(report):
    """The report as rows, for plotting somewhere that has a plotter.

    No plotting dependency here on purpose - this project ships stdlib scripts
    and one of them producing a chart would be the first thing to break on a
    machine that has not been prepared. Every row repeats `stackId`, because a
    CSV is separated from the report that explains it the moment it is opened
    in anything.
    """
    import csv
    import io as _io

    buffer = _io.StringIO()
    writer = csv.DictWriter(buffer, fieldnames=CSV_COLUMNS, lineterminator="\n")
    writer.writeheader()

    for row in report["points"]:
        writer.writerow({
            "kind": "point", "stackId": report["stackId"], "index": row["index"],
            "lux": row["lux"], "logLux": row["logLux"], "hue": row["hue"],
            "sat": row["sat"], "percent": row["percent"],
            "usedInPercentFit": int(row["usedInPercentFit"]),
            "usedInOutputFit": int(row["usedInOutputFit"]),
            "clockUsed": int(row["clockUsed"]),
            "censored": int(row["censored"]), "percentCurve": row["percentCurve"],
            "percentResidual": row["percentResidual"], "logOutput": row["logOutput"],
            "outputCurve": row["outputCurve"], "outputResidual": row["outputResidual"],
            "predictedPercent": row["predictedPercent"],
            "limited": row["predictedLimited"] or "",
        })

    for curve in report["curves"]:
        for i, x in enumerate(curve["logLux"]):
            writer.writerow({
                "kind": "curve", "stackId": report["stackId"], "index": i,
                "lux": curve["lux"][i], "logLux": x, "hue": curve["hue"],
                "sat": curve["sat"], "percent": curve["percent"][i],
                "usedInPercentFit": "", "usedInOutputFit": "", "clockUsed": "",
                "censored": "", "percentCurve": "", "percentResidual": "",
                "logOutput": "", "outputCurve": "", "outputResidual": "",
                "predictedPercent": curve["percent"][i],
                "limited": curve["limited"][i] or "",
            })
    return buffer.getvalue()


def _format(value, spec="%8.4f", empty="       -"):
    return empty if value is None else spec % value


def report_to_text(report):
    """The same thing for a person, which is a different shape."""
    lines = []
    lines.append("Stack:   %s" % report["stackId"])
    lines.append("Weights: %.4f / %.4f / %.4f  (a prior, not a measurement of "
                 "these LEDs)" % tuple(report["weights"]))
    lines.append("Drive:   %s" % ("assumed proportional - no coupling record given"
                                  if report["driveAssumed"] else
                                  "measured (%s)" % (report["driveSource"] or "supplied")))
    lines.append("Range:   %d..%d %%" % (report["minPercent"], report["maxPercent"]))
    lines.append("")

    for fit, label, variable, unit, spec in (
            (report["percentFit"], "Current model ", "percent   ", "%", "%.2f"),
            (report["outputFit"], "Proposed model", "log output", "decades", "%.4f")):
        if not fit["available"]:
            # The reason, not a blank: a line that cannot be fitted is a result,
            # and the reader has to know which of the two is missing and why.
            lines.append("%s  %s = unavailable, n=%d - %s"
                         % (label, variable, fit["count"], fit["reason"]))
            continue
        lines.append("%s  %s = %8.4f * log10(lux) + %8.4f  "
                     "[%s, n=%d, %.2f decades]  RMS %s %s"
                     % (label, variable, fit["slope"], fit["offset"],
                        "slope supplied" if fit["priorSlope"] else "fitted",
                        fit["count"], fit["spread"],
                        _format(fit["rms"], spec, "-"), unit))
    lines.append("")

    lines.append("  #        lux   H/S    set   curve   resid   logOut  outCurve"
                 "  resid   pred  note")
    for row in report["points"]:
        colour = "  -  " if row["hue"] is None else "%3d/%-3d" % (row["hue"], row["sat"])
        note = []
        if row["censored"]:
            note.append("censored"
                        + (" (kept anyway)" if row["usedInPercentFit"]
                           or row["usedInOutputFit"] else ""))
        if row["clockUsed"] != (row["usedInPercentFit"] or row["usedInOutputFit"]):
            # The clock's own verdict and this one disagreeing is a fact about
            # the range, and worth saying out loud rather than resolving quietly.
            note.append("clock said %s" % ("used" if row["clockUsed"] else "unused"))
        if row["hue"] is None:
            note.append("unknown colour")
        if row["assumedColour"]:
            note.append("assumed colour")
        if row["predictedLimited"]:
            note.append(row["predictedLimited"])
        lines.append("%3d %10.4f %7s %5d %7s %7s %8s %9s %7s %6s  %s"
                     % (row["index"], row["lux"], colour, row["percent"],
                        _format(row["percentCurve"], "%7.1f", "      -"),
                        _format(row["percentResidual"], "%7.1f", "      -"),
                        _format(row["logOutput"], "%8.4f", "       -"),
                        _format(row["outputCurve"], "%9.4f", "        -"),
                        _format(row["outputResidual"], "%7.4f", "      -"),
                        "-" if row["predictedPercent"] is None else "%d" % row["predictedPercent"],
                        ", ".join(note)))
    lines.append("")

    lines.append("Per colour, the offset the data would want (decades of output):")
    for group in report["colours"]:
        lines.append("  hue %3d sat %3d  n=%d  mean %s  over %.2f decades of "
                     "light  %s"
                     % (group["hue"], group["sat"], group["count"],
                        # No line, no offset: an offset is measured from one.
                        _format(group["meanResidual"], "%+.4f", "unavailable"),
                        group["spreadDecades"],
                        "" if group["residualSpread"] is None
                        else "(residual varies by %.4f)" % group["residualSpread"]))
    lines.append("")

    for curve in report["curves"]:
        limited = sum(1 for flag in curve["limited"] if flag)
        lines.append("Predicted percent, hue %3d sat %3d -> RGB %s%s"
                     % (curve["hue"], curve["sat"], tuple(curve["rgb"]),
                        "  (%d of %d points limited)" % (limited, len(curve["percent"]))
                        if limited else ""))
        lines.append("    lux     " + " ".join("%7.3f" % v for v in curve["lux"]))
        lines.append("    percent " + " ".join("%7d" % v for v in curve["percent"]))
    lines.append("")

    if report["warnings"]:
        lines.append("Warnings:")
        for warning in report["warnings"]:
            lines.append("  - %s" % warning)
    return "\n".join(lines)


def load_json(where):
    """A file, or a URL if somebody has a clock in front of them.

    The URL form exists because `GET /luminance` is where this data comes
    from; nothing in this module reaches for it on its own, and every example
    and every test uses a saved file.
    """
    if where.startswith("http://") or where.startswith("https://"):
        import urllib.request
        with urllib.request.urlopen(where, timeout=10) as response:
            return json.loads(response.read().decode("utf-8"))
    try:
        with io.open(where, "r", encoding="utf-8") as handle:
            return json.load(handle)
    except json.JSONDecodeError as problem:
        # The file name is the useful half: "line 1 column 3" on its own does
        # not say which of the two files was the broken one.
        raise ValueError("%s is not valid JSON: %s" % (where, problem))
    except OSError as problem:
        raise ValueError("cannot read %s: %s" % (where, problem.strerror or problem))


def _parse_numbers(text, option):
    try:
        return [float(piece) for piece in text.split(",")]
    except ValueError:
        raise ValueError("%s wants numbers separated by commas, not %r"
                         % (option, text))


def _parse_colours(text, option="--colours"):
    colours = []
    for piece in text.split(","):
        piece = piece.strip()
        if not piece:
            continue
        hue, _, sat = piece.partition("/")
        try:
            pair = (int(hue), int(sat or 0))
        except ValueError:
            raise ValueError("%s wants hue/saturation pairs such as 225/100, "
                             "not %r" % (option, piece))
        # Checked here rather than at the far end, so the message names the
        # option that was mistyped instead of a function nobody called.
        _bounded("%s hue" % option, pair[0], 0, 359)
        _bounded("%s saturation" % option, pair[1], 0, 100)
        colours.append(pair)
    if not colours:
        raise ValueError("%s was given nothing to read" % option)
    return colours


def main(argv=None):
    import argparse

    parser = argparse.ArgumentParser(
        prog="colour_luminance",
        description="Compare the current percent-space brightness curve with "
                    "the proposed log-output one, on a saved GET /luminance "
                    "snapshot. Reads files; changes nothing.")
    sub = parser.add_subparsers(dest="command")

    compare_parser = sub.add_parser("compare", help="fit both models and report")
    compare_parser.add_argument("--luminance", required=True,
                                help="GET /luminance as a saved file (or a URL)")
    compare_parser.add_argument("--coupling",
                                help="the coupling record from `lab.py calibrate`, "
                                     "for the measured drive response")
    compare_parser.add_argument("--weights",
                                help="photopic RGB weights, default %s"
                                     % (",".join("%g" % w for w in PHOTOPIC_WEIGHTS)))
    compare_parser.add_argument("--colours",
                                help="hue/sat pairs to predict, e.g. 140/0,225/100")
    compare_parser.add_argument("--assume-colour",
                                help="hue/sat to assume for points stored before "
                                     "the colour was kept; without it they are "
                                     "left out rather than guessed")
    compare_parser.add_argument("--stack-id", default=STACK_ID_DEFAULT,
                                help="the optical stack these numbers belong to "
                                     "(default: %s)" % STACK_ID_DEFAULT)
    compare_parser.add_argument("--csv", help="write the rows here as well")
    compare_parser.add_argument("--json", action="store_true",
                                help="print the report as JSON instead of a table")

    args = parser.parse_args(argv)
    if args.command != "compare":
        parser.print_help()
        return 2

    # Everything from here on reads files somebody typed the names of and
    # numbers somebody typed the values of. A traceback would be this script
    # blaming the user for its own lack of an error path, so each of these
    # comes back as the same kind of message argparse itself produces - one
    # line, exit code 2, nothing on stdout.
    try:
        snapshot = load_json(args.luminance)
        drive = (DriveTable.from_record(load_json(args.coupling), source=args.coupling)
                 if args.coupling else DriveTable.proportional())
        weights = (_check_weights(_parse_numbers(args.weights, "--weights"))
                   if args.weights else PHOTOPIC_WEIGHTS)
        colours = _parse_colours(args.colours, "--colours") if args.colours else None
        assume = (_parse_colours(args.assume_colour, "--assume-colour")[0]
                  if args.assume_colour else None)

        report = compare(snapshot, drive=drive, weights=weights, colours=colours,
                         stack_id=args.stack_id, assume=assume)

        if args.csv:
            with io.open(args.csv, "w", encoding="utf-8") as handle:
                handle.write(report_to_csv(report))
    except (OSError, ValueError) as problem:
        # json.JSONDecodeError is a ValueError, and every refusal this module
        # raises is one too.
        parser.error(str(problem))

    print(json.dumps(report, indent=1, sort_keys=True) if args.json
          else report_to_text(report))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
