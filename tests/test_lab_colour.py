# -*- coding: utf-8 -*-
"""The colour-output sweep in scripts/lab.py, run against a clock that is not there.

Every test here drives a `FakeLab`: the sweep is a sequence of frames and a
sequence of calls, and both are worth pinning down exactly, because on a real
clock a wrong frame does not fail - it measures something else and says nothing
about it. Nothing in this file opens a socket.

What the sweep measures is the **broadband** response of a TSL2591 to the
clock's own LEDs: channel mixing and drive linearity. It is not a measurement
of perceived brightness, and the tests check that the files it writes say so.
"""
import json
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "scripts"))

import colour_luminance as cl
import lab


def driver_rgb(hue, sat, percent):
    """What the LED driver would write for this colour at this setting.

    Built here out of colour_luminance the same way the sweep must build it -
    FastLED's rainbow wheel, the clock's gamma, the driver's per-channel
    scaling. If the sweep ever reaches for an ordinary HSV conversion, these
    numbers stop matching.
    """
    scaled = cl.gamma_scale(percent)
    return [cl.channel_drive(c, scaled) for c in cl.display_rgb(hue, sat)]


class GridTest(unittest.TestCase):
    """The representative grid, and the frames it turns into."""

    def test_the_grid_is_the_planned_representative_set(self):
        grid = lab.colour_grid()
        # White first: one hue only, because at zero saturation the wheel
        # answers the same white whatever the hue.
        self.assertEqual(grid[0], (0, 0))
        # Six saturated hues around the wheel.
        for hue in (0, 60, 120, 180, 240, 300):
            self.assertIn((hue, 100), grid)
        # And half saturation at three widely spaced ones.
        for hue in (0, 120, 240):
            self.assertIn((hue, 50), grid)
        self.assertEqual(len(grid), 10)
        self.assertEqual(len(set(grid)), len(grid), "no colour measured twice")

    def test_the_percentages_are_the_planned_ones(self):
        self.assertEqual(lab.COLOUR_PERCENTS, [20, 35, 50, 70, 90, 100])

    def test_every_frame_is_exactly_what_the_driver_would_write(self):
        cells = [(8, 5), (8, 6)]
        grid = [(0, 0), (240, 100)]
        percents = [20, 100]
        frames = lab.colour_frames(cells, grid, percents)

        # Colour by colour, percentage by percentage, in that order - so a row
        # of the result reads as one colour swept over the slider.
        wanted = [(0, 0, 20), (0, 0, 100), (240, 100, 20), (240, 100, 100)]
        self.assertEqual(len(frames), len(wanted))
        for frame, (hue, sat, percent) in zip(frames, wanted):
            rgb = driver_rgb(hue, sat, percent)
            self.assertEqual(frame, {"clear": True,
                                     "set": [{"cell": [8, 5], "rgb": rgb},
                                             {"cell": [8, 6], "rgb": rgb}]},
                             "hue %d sat %d at %d %%" % (hue, sat, percent))

    def test_the_frames_are_not_ordinary_hsv(self):
        # The one check that would pass with a geometric conversion is no check
        # at all: FastLED's wheel puts full-saturation "yellow" at (171, 170, 0)
        # where an ordinary HSV would give (255, 255, 0). 90 degrees is the
        # hue that lands on FastLED's byte 64 through the clock's own rounding.
        frame = lab.colour_frames([(8, 5)], [(90, 100)], [100])[0]
        self.assertEqual(frame["set"][0]["rgb"], [171, 170, 0])


class PowerTest(unittest.TestCase):
    """The budget, worked out the way the clock works it out.

    FastLED's `calculate_unscaled_power_mW`: the channels are summed over the
    whole strip, each sum multiplied by its milliwatts and shifted down by
    eight, plus a dark milliwatt per pixel. The clock refuses an over-budget
    frame outright - a frame it dimmed would not be the frame that was asked
    for - and the first whole-face white frame browned it out, so this is
    worth getting right on this side rather than discovering on the wall.
    """

    def test_a_dark_frame_is_the_strip_idling(self):
        # gDark_mW is 1 mA at 5 V, and there are 114 pixels.
        self.assertEqual(lab.estimated_draw_mw({"clear": True, "set": []}), 5 * 114)

    def test_one_white_pixel_is_the_three_channels_added(self):
        frame = {"clear": True, "set": [{"cell": [8, 5], "rgb": [255, 255, 255]}]}
        expected = ((255 * 80) >> 8) + ((255 * 55) >> 8) + ((255 * 75) >> 8) + 5 * 114
        self.assertEqual(lab.estimated_draw_mw(frame), expected)

    def test_the_channels_are_summed_before_they_are_scaled(self):
        # The shift happens once, over the sum - not per pixel. Two pixels at
        # 127 are not the same as one at 254 if it is done per pixel.
        frame = {"clear": True, "set": [{"cell": [8, 5], "rgb": [127, 0, 0]},
                                        {"cell": [8, 6], "rgb": [127, 0, 0]}]}
        self.assertEqual(lab.estimated_draw_mw(frame), ((254 * 80) >> 8) + 5 * 114)

    def test_the_default_sweep_stays_far_below_the_cap(self):
        # Three cells at full white, against the clock's 7.5 W.
        worst = max(lab.estimated_draw_mw(frame) for frame in
                    lab.colour_frames(lab.COLOUR_CELLS, lab.colour_grid(),
                                      lab.COLOUR_PERCENTS))
        self.assertLess(worst, lab.LAB_MAX_DRAW_MW / 2)


# "no override", as distinct from "the clock answered null" - which is a thing
# a JSON body can be, and therefore a thing the sweep has to survive.
UNSET = object()


class FakeLab(object):
    """The clock as this experiment uses it, with nothing behind it.

    Records every call in order, because *when* the strip is taken and given
    back is as much a part of this run as the frames are: a script that keeps
    the strip after it fails leaves a clock dark on the wall with no sign of
    why.
    """

    def __init__(self, on=False, present=True, sweep_error=None,
                 release_error=None, max_frames=128, max_draw_mw=7500,
                 take_error=None, sweep_answer=UNSET):
        self.calls = []
        self.on = on
        self.present = present
        self.sweep_error = sweep_error
        self.release_error = release_error
        self.take_error = take_error
        # What the clock answers with, when a test needs it to be something
        # other than a complete, well-formed sweep.
        self.sweep_answer = sweep_answer
        self.max_frames = max_frames
        self.max_draw_mw = max_draw_mw
        self.swept = None

    def state(self):
        self.calls.append(("state",))
        return {"on": self.on, "mode": 7, "pixels": 114, "rows": 10, "columns": 11,
                "maxFrames": self.max_frames, "drawMw": 570,
                "maxDrawMw": self.max_draw_mw,
                "sensor": {"name": "TSL2591", "present": self.present,
                           "rungs": 8, "rung": 4, "ms": 200}}

    def _call(self, path, body=None):
        self.calls.append(("call", path, body))
        if path == "/currentState":
            return {"hue": 140, "sat": 80, "lum": 60, "mode": 0, "automaticLum": 1}
        raise AssertionError("the sweep asked for %s" % path)

    def take(self, on=True):
        self.calls.append(("take", on))
        if on and self.take_error is not None:
            # An acquisition that fails is *ambiguous*: the clock may well have
            # entered lab mode before the answer went missing.
            raise self.take_error
        if not on and self.release_error is not None:
            raise self.release_error
        self.on = on
        return {"on": on}

    def sweep(self, frames, settle_ms=80, dark=False, rung=None):
        self.calls.append(("sweep", len(frames), settle_ms, dark, rung))
        self.swept = frames
        if self.sweep_error is not None:
            raise self.sweep_error
        if self.sweep_answer is not UNSET:
            return self.sweep_answer
        # One reading per frame, plus the dark companion beside it. The numbers
        # are made up and only their shape matters here.
        return {"frames": [{"lit": {"lux": 1.0 + i, "ch0": 1000 + i, "ch1": 100,
                                    "ms": 200, "rung": rung},
                            "dark": {"lux": 0.5, "ch0": 400, "ch1": 40,
                                     "ms": 200, "rung": rung}}
                           for i in range(len(frames))]}


def run_sweep(fake, **kwargs):
    kwargs.setdefault("json_path", None)
    return lab.colour_sweep(fake, **kwargs)


class OwnershipTest(unittest.TestCase):
    """The strip is taken for the sweep and for nothing else."""

    def test_it_looks_before_it_takes_and_gives_back_afterwards(self):
        fake = FakeLab()
        run_sweep(fake)
        kinds = [call[0] for call in fake.calls]
        # State first, the clock's own settings next, and only then the strip.
        self.assertEqual(kinds[0], "state")
        self.assertEqual(kinds[1], "call")
        self.assertEqual(fake.calls[1][1], "/currentState")
        self.assertEqual(kinds[2], "take")
        self.assertEqual(fake.calls[2], ("take", True))
        self.assertEqual(kinds[3], "sweep")
        self.assertEqual(fake.calls[-1], ("take", False))
        # Taken once, given back once.
        self.assertEqual(kinds.count("take"), 2)

    def test_it_gives_the_strip_back_when_the_sweep_fails(self):
        for problem in (ValueError("no"),
                        SystemExit("/lab/sweep -> HTTP 413 labTooBright"),
                        KeyboardInterrupt()):
            fake = FakeLab(sweep_error=problem)
            with self.assertRaises(type(problem), msg=repr(problem)):
                run_sweep(fake)
            self.assertEqual(fake.calls[-1], ("take", False), repr(problem))

    def test_a_failure_to_give_the_strip_back_does_not_hide_the_real_error(self):
        # The release runs in a `finally`, so an exception out of it would
        # replace the one that says what actually went wrong.
        fake = FakeLab(sweep_error=ValueError("the sweep failed"),
                       release_error=SystemExit("/lab/mode -> HTTP 500"))
        with self.assertRaises(ValueError) as caught:
            run_sweep(fake)
        self.assertIn("the sweep failed", str(caught.exception))

    def test_it_refuses_a_clock_whose_strip_is_already_taken(self):
        # `on` means somebody else - another script, or a calibration - owns
        # the strip. Taking it would not fail; it would quietly ruin their run.
        fake = FakeLab(on=True)
        with self.assertRaises(SystemExit):
            run_sweep(fake)
        self.assertNotIn("take", [call[0] for call in fake.calls])

    def test_it_refuses_a_clock_with_no_sensor(self):
        fake = FakeLab(present=False)
        with self.assertRaises(SystemExit):
            run_sweep(fake)
        self.assertNotIn("take", [call[0] for call in fake.calls])


class ReportTest(unittest.TestCase):
    """What the run writes down, and what it refuses to claim."""

    def report(self, **kwargs):
        return run_sweep(FakeLab(), **kwargs)

    def test_it_names_the_optical_stack_it_was_measured_on(self):
        # A diffuser behind the mask and one in front of it are different
        # devices photometrically. A dataset that does not say which one it
        # came from cannot be compared with the next one.
        self.assertEqual(lab.COLOUR_STACK_ID, "current-diffuser-before-mask")
        self.assertEqual(self.report()["stackId"], "current-diffuser-before-mask")
        self.assertEqual(self.report(stack_id="mask-then-wh73")["stackId"],
                         "mask-then-wh73")

    def test_it_records_what_the_run_was(self):
        report = self.report()
        self.assertEqual(report["cells"], [[8, 5], [8, 6], [8, 7]])
        self.assertEqual(report["percents"], lab.COLOUR_PERCENTS)
        self.assertEqual(report["grid"], [list(c) for c in lab.colour_grid()])
        self.assertEqual(report["rung"], lab.COLOUR_RUNG)
        self.assertEqual(report["settleMs"], lab.COLOUR_SETTLE_MS)
        # The clock as it was found, so the run can be read months later.
        self.assertEqual(report["clock"]["hue"], 140)
        self.assertEqual(report["clock"]["sat"], 80)
        self.assertEqual(report["sensor"]["name"], "TSL2591")

    def test_it_says_the_sensor_is_broadband_and_not_the_eye(self):
        report = self.report()
        # Not a claim about perceived brightness, and the file has to say so:
        # the TSL2591 weights the channels roughly 35/40/25 where the eye
        # weights them 21/72/7.
        self.assertFalse(report["sensor"]["photopic"])
        self.assertIn("broadband", report["sensor"]["note"])
        self.assertIn("not", report["sensor"]["note"].lower())
        self.assertIn("perceiv", report["measures"].lower())

    def test_every_frame_carries_its_colour_and_its_reading(self):
        report = self.report()
        self.assertEqual(len(report["frames"]),
                         len(lab.colour_grid()) * len(lab.COLOUR_PERCENTS))
        first = report["frames"][0]
        self.assertEqual((first["hue"], first["sat"], first["percent"]),
                         (0, 0, lab.COLOUR_PERCENTS[0]))
        self.assertEqual(first["rgb"], driver_rgb(0, 0, lab.COLOUR_PERCENTS[0]))
        self.assertAlmostEqual(first["lit"], 1.0)
        self.assertAlmostEqual(first["dark"], 0.5)
        # The frame's own contribution: what it added to the room beside it.
        self.assertAlmostEqual(first["signal"], 0.5)
        self.assertEqual(first["ch0"], 1000)
        self.assertFalse(first["saturated"])

    def test_a_reading_against_the_stop_is_flagged_rather_than_used(self):
        fake = FakeLab()
        fake.max_frames = 128
        report = run_sweep(fake)
        # The fake's readings climb; none of them is near full scale.
        self.assertFalse(any(frame["saturated"] for frame in report["frames"]))

        class Saturating(FakeLab):
            def sweep(self, frames, settle_ms=80, dark=False, rung=None):
                answer = FakeLab.sweep(self, frames, settle_ms, dark, rung)
                answer["frames"][3]["lit"]["ch0"] = 36000     # 98 % at 100 ms
                answer["frames"][3]["lit"]["ms"] = 100
                return answer

        report = run_sweep(Saturating())
        self.assertTrue(report["frames"][3]["saturated"])
        self.assertFalse(report["frames"][2]["saturated"])

    def test_the_json_is_deterministic(self):
        # Same run, same bytes: a file that differs on every run cannot be
        # diffed, and a timestamp is the usual reason.
        first = lab.colour_json(self.report())
        second = lab.colour_json(self.report())
        self.assertEqual(first, second)
        # And nothing in it is a clock reading: a field named for a date or a
        # time is the usual reason a record stops being diffable. Checked by
        # field name rather than by scanning the text, which trips over the
        # word "times" in the sensor note.
        def field_names(node):
            if isinstance(node, dict):
                for key, value in node.items():
                    yield key
                    for name in field_names(value):
                        yield name
            elif isinstance(node, list):
                for value in node:
                    for name in field_names(value):
                        yield name

        for name in field_names(json.loads(first)):
            self.assertNotIn("date", name.lower())
            self.assertNotIn("stamp", name.lower())
            self.assertNotIn("when", name.lower())

    def test_the_csv_is_plain_rows_and_carries_the_stack(self):
        import csv
        import io as string_io

        rows = list(csv.reader(string_io.StringIO(lab.colour_csv(self.report()))))
        header = rows[0]
        self.assertEqual(header[0], "stackId")
        for column in ("hue", "sat", "percent", "r", "g", "b", "lit", "dark",
                       "signal", "ch0", "ch1", "saturated"):
            self.assertIn(column, header)
        self.assertEqual(len(rows) - 1,
                         len(lab.colour_grid()) * len(lab.COLOUR_PERCENTS))
        for row in rows[1:]:
            self.assertEqual(row[0], lab.COLOUR_STACK_ID)

    def test_it_writes_the_files_it_is_asked_for_and_no_others(self):
        import shutil
        import tempfile
        folder = tempfile.mkdtemp()
        try:
            wanted = os.path.join(folder, "sweep.json")
            run_sweep(FakeLab(), json_path=wanted)
            self.assertEqual(sorted(os.listdir(folder)), ["sweep.json"])
            with open(wanted) as handle:
                self.assertEqual(json.load(handle)["stackId"], lab.COLOUR_STACK_ID)

            comma = os.path.join(folder, "sweep.csv")
            run_sweep(FakeLab(), json_path=wanted, csv_path=comma)
            self.assertEqual(sorted(os.listdir(folder)), ["sweep.csv", "sweep.json"])
        finally:
            shutil.rmtree(folder, ignore_errors=True)


class LimitsTest(unittest.TestCase):
    """The refusals, all of them made before the strip is taken."""

    def test_more_cells_than_the_default_need_the_flag(self):
        cells = [(8, c) for c in range(6)]
        fake = FakeLab()
        with self.assertRaises(SystemExit):
            run_sweep(fake, cells=cells)
        self.assertEqual(fake.calls, [], "nothing was asked of the clock")
        # With the flag it goes ahead - and still inside the power cap.
        report = run_sweep(FakeLab(), cells=cells, full=True)
        self.assertEqual(len(report["cells"]), 6)

    def test_an_over_budget_frame_is_refused_before_the_strip_is_taken(self):
        cells = [(r, c) for r in range(6) for c in range(11)]   # 66 cells of white
        fake = FakeLab()
        with self.assertRaises(SystemExit):
            run_sweep(fake, cells=cells, full=True)
        self.assertNotIn("take", [call[0] for call in fake.calls])

    def test_the_clock_gets_the_last_word_on_the_budget(self):
        # If a clock reports a smaller cap than this script knows about, that
        # is the number that counts.
        fake = FakeLab(max_draw_mw=600)
        with self.assertRaises(SystemExit):
            run_sweep(fake)
        self.assertNotIn("take", [call[0] for call in fake.calls])

    def test_more_frames_than_the_clock_accepts_are_refused(self):
        fake = FakeLab(max_frames=10)
        with self.assertRaises(SystemExit):
            run_sweep(fake)
        self.assertNotIn("take", [call[0] for call in fake.calls])

    def test_the_sweep_is_one_request_with_dark_readings_on_a_pinned_rung(self):
        fake = FakeLab()
        run_sweep(fake, rung=3, settle_ms=250)
        sweeps = [call for call in fake.calls if call[0] == "sweep"]
        self.assertEqual(len(sweeps), 1, "one request, or network jitter gets in")
        _, count, settle_ms, dark, rung = sweeps[0]
        self.assertEqual(count, len(lab.colour_grid()) * len(lab.COLOUR_PERCENTS))
        self.assertEqual((settle_ms, dark, rung), (250, True, 3))


class CleanupTest(unittest.TestCase):
    """Taking the strip and giving it back, including when that goes wrong."""

    def setUp(self):
        import shutil
        import tempfile
        self.dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.dir, ignore_errors=True)
        self.json_path = os.path.join(self.dir, "sweep.json")

    def run_it(self, fake):
        return run_sweep(fake, json_path=self.json_path,
                         csv_path=os.path.join(self.dir, "sweep.csv"))

    def test_an_acquisition_that_fails_is_still_cleaned_up_after(self):
        # Taking the strip can fail *ambiguously*: a timeout on the way back
        # says nothing about whether the clock entered lab mode. Leaving it
        # there because the call reported failure is how a clock ends up dark
        # on the wall with nobody knowing why.
        fake = FakeLab(take_error=SystemExit("/lab/mode -> HTTP 504"))
        with self.assertRaises(SystemExit):
            self.run_it(fake)
        self.assertEqual(fake.calls[-1], ("take", False), "no release was tried")
        self.assertEqual(os.listdir(self.dir), [])

    def test_an_acquisition_failure_survives_a_failed_release(self):
        fake = FakeLab(take_error=ValueError("could not take the strip"),
                       release_error=SystemExit("/lab/mode -> HTTP 500"))
        with self.assertRaises(ValueError) as caught:
            self.run_it(fake)
        self.assertIn("could not take", str(caught.exception))

    def test_a_sweep_failure_survives_a_failed_release(self):
        fake = FakeLab(sweep_error=ValueError("the sweep failed"),
                       release_error=SystemExit("/lab/mode -> HTTP 500"))
        with self.assertRaises(ValueError) as caught:
            self.run_it(fake)
        self.assertIn("the sweep failed", str(caught.exception))
        self.assertEqual(os.listdir(self.dir), [])

    def test_an_interrupted_run_gives_the_strip_back_and_writes_nothing(self):
        fake = FakeLab(sweep_error=KeyboardInterrupt())
        with self.assertRaises(KeyboardInterrupt):
            self.run_it(fake)
        self.assertEqual(fake.calls[-1], ("take", False))
        self.assertEqual(os.listdir(self.dir), [])

    def test_an_original_error_survives_an_interrupt_during_the_release(self):
        # Ctrl-C while the strip is being given back. Whatever the release
        # does, the error that says why the run failed is the one to keep.
        fake = FakeLab(sweep_error=ValueError("the sweep failed"),
                       release_error=KeyboardInterrupt())
        with self.assertRaises(ValueError) as caught:
            self.run_it(fake)
        self.assertIn("the sweep failed", str(caught.exception))
        self.assertEqual(os.listdir(self.dir), [])

    def test_an_acquisition_error_survives_an_interrupt_during_the_release(self):
        fake = FakeLab(take_error=ValueError("could not take the strip"),
                       release_error=KeyboardInterrupt())
        with self.assertRaises(ValueError) as caught:
            self.run_it(fake)
        self.assertIn("could not take", str(caught.exception))

    def test_an_interrupt_during_the_release_stays_an_interrupt(self):
        # Nothing went wrong before it, so this is the error - and it is not
        # dressed up as something else: Ctrl-C means Ctrl-C, and turning it
        # into an ordinary failure would take that away from whoever pressed
        # it. Still fatal, still nothing written.
        fake = FakeLab(release_error=KeyboardInterrupt())
        with self.assertRaises(KeyboardInterrupt):
            self.run_it(fake)
        self.assertEqual(os.listdir(self.dir), [], "a file was written anyway")

    def test_an_ordinary_failure_to_release_is_still_reported_as_one(self):
        fake = FakeLab(release_error=RuntimeError("the socket went away"))
        with self.assertRaises(SystemExit) as caught:
            self.run_it(fake)
        self.assertIn("lab", str(caught.exception).lower())
        self.assertEqual(os.listdir(self.dir), [])

    def test_a_failed_release_after_a_good_sweep_is_fatal(self):
        # The measurement is fine, but the clock is still in lab mode: dark,
        # not showing the time, and waiting for somebody who thinks the run
        # finished. Reporting success on top of that would bury the one thing
        # the person needs to act on - so no report, and no files.
        fake = FakeLab(release_error=SystemExit("/lab/mode -> HTTP 500"))
        with self.assertRaises(SystemExit) as caught:
            self.run_it(fake)
        message = str(caught.exception)
        self.assertIn("lab", message.lower())
        self.assertEqual(os.listdir(self.dir), [], "a file was written anyway")


class ResultValidationTest(unittest.TestCase):
    """What comes back is checked before any of it is believed.

    The firmware **truncates**: a sweep that outruns LAB_MAX_SWEEP_MS (90 s)
    stops where it is, sets `truncated` and answers with fewer frames than were
    asked for. Zipped against the colours that were requested, a short answer
    does not fail - it labels every reading with the wrong colour from the
    point where it stopped, and writes a file that looks complete.
    """

    def setUp(self):
        import shutil
        import tempfile
        self.dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.dir, ignore_errors=True)
        self.json_path = os.path.join(self.dir, "sweep.json")
        self.csv_path = os.path.join(self.dir, "sweep.csv")

    def reading(self, lux=1.0):
        return {"lux": lux, "ch0": 1000, "ch1": 100, "ms": 200, "rung": 4}

    def answer(self, count, **extra):
        frames = [{"lit": self.reading(1.0 + i), "dark": self.reading(0.5)}
                  for i in range(count)]
        answer = {"frames": frames}
        answer.update(extra)
        return answer

    def refuse(self, sweep_answer):
        """The run must fail, and must leave nothing behind."""
        fake = FakeLab(sweep_answer=sweep_answer)
        with self.assertRaises(SystemExit) as caught:
            run_sweep(fake, json_path=self.json_path, csv_path=self.csv_path)
        self.assertEqual(os.listdir(self.dir), [], "a file was written anyway")
        # And the strip still went back.
        self.assertEqual(fake.calls[-1], ("take", False))
        return str(caught.exception)

    def wanted(self):
        return len(lab.colour_grid()) * len(lab.COLOUR_PERCENTS)

    def test_a_truncated_sweep_is_fatal(self):
        message = self.refuse(self.answer(self.wanted(), truncated=True))
        self.assertIn("truncated", message.lower())

    def test_a_truncated_sweep_is_fatal_even_at_the_full_length(self):
        # `truncated` is the clock's own word for "this run is not what you
        # asked for". Believing the frame count over the flag would be
        # believing the half of the answer that cannot say so.
        self.refuse(self.answer(self.wanted(), truncated=True))

    def test_too_few_frames_are_fatal(self):
        message = self.refuse(self.answer(self.wanted() - 1))
        self.assertIn("%d" % self.wanted(), message)

    def test_too_many_frames_are_fatal(self):
        self.refuse(self.answer(self.wanted() + 1))

    def test_an_answer_that_is_not_a_sweep_is_fatal(self):
        for bad in ([], "frames", 5, None, {}, {"frames": 5}, {"frames": {}}):
            self.refuse(bad)

    def test_a_frame_that_is_not_a_reading_pair_is_fatal(self):
        for broken in ({}, {"lit": {"lux": 1.0}}, {"dark": {"lux": 1.0}},
                       {"lit": 5, "dark": {"lux": 1.0}},
                       {"lit": {"lux": 1.0}, "dark": 5}, 7, None, "frame"):
            answer = self.answer(self.wanted())
            answer["frames"][2] = broken
            self.refuse(answer)

    def test_a_reading_the_report_cannot_use_is_fatal(self):
        # `lux` is what every row is built from; `ch0` and `ms` are what the
        # saturation check divides. A null in any of them is a reading that
        # failed, and a run with a hole in it must not be written to a file
        # that will later look complete.
        for broken in ({"ch0": 1000, "ms": 200},                 # no lux at all
                       {"lux": None, "ch0": 1000, "ms": 200},
                       {"lux": "1.0", "ch0": 1000, "ms": 200},
                       {"lux": 1.0, "ch0": None, "ms": 200},
                       {"lux": 1.0, "ch0": 1000, "ms": None}):
            answer = self.answer(self.wanted())
            answer["frames"][1]["lit"] = broken
            self.refuse(answer)
            answer = self.answer(self.wanted())
            answer["frames"][1]["dark"] = broken
            self.refuse(answer)

    def test_the_raw_counts_arrive_together_or_not_at_all(self):
        # `measureInto` in LabRoutes.cpp writes ch0 and ch1 inside one branch:
        # either `readChannels` succeeded and both are there, or neither is.
        # One without the other is an answer this script does not understand,
        # and the infrared channel is the half that would go unnoticed - the
        # report carries it, and a null in it is a hole in the data.
        for broken in ({"lux": 1.0, "ch0": 1000, "ms": 200, "rung": 4},
                       {"lux": 1.0, "ch1": 100, "ms": 200, "rung": 4},
                       {"lux": 1.0, "ch0": 1000, "ch1": None, "ms": 200, "rung": 4},
                       {"lux": 1.0, "ch0": None, "ch1": 100, "ms": 200, "rung": 4},
                       {"lux": 1.0, "ch0": 1000, "ch1": "100", "ms": 200, "rung": 4},
                       {"lux": 1.0, "ch0": 1000, "ch1": 100, "rung": 4},
                       {"lux": 1.0, "ch0": 1000, "ch1": 100, "ms": 200}):
            answer = self.answer(self.wanted())
            answer["frames"][1]["lit"] = broken
            self.refuse(answer)
            answer = self.answer(self.wanted())
            answer["frames"][1]["dark"] = broken
            self.refuse(answer)

    def test_a_reading_that_is_not_finite_is_fatal(self):
        # `json.loads` accepts the literals NaN and Infinity, and a NaN is
        # exactly what a sensor that could not answer produces on its way
        # through ArduinoJson. Neither belongs in a measurement.
        for field in ("lux", "ch0", "ch1", "ms"):
            for value in (float("nan"), float("inf"), float("-inf")):
                answer = self.answer(self.wanted())
                answer["frames"][1]["lit"][field] = value
                self.refuse(answer)

    def test_a_reading_without_counts_is_still_a_reading(self):
        # `measureInto` only adds ch0/ch1 when the channels could be read. A
        # lux on its own is thin but usable, and the saturation check already
        # says so by asking whether ch0 is there at all.
        answer = self.answer(self.wanted())
        answer["frames"][1]["lit"] = {"lux": 2.0, "rung": 4, "ms": 200}
        report = run_sweep(FakeLab(sweep_answer=answer), json_path=self.json_path)
        self.assertFalse(report["frames"][1]["saturated"])
        self.assertIsNone(report["frames"][1]["ch0"])

    def test_a_good_sweep_still_goes_through(self):
        report = run_sweep(FakeLab(sweep_answer=self.answer(self.wanted())),
                           json_path=self.json_path)
        self.assertEqual(len(report["frames"]), self.wanted())
        self.assertEqual(os.listdir(self.dir), ["sweep.json"])


class CommandTest(unittest.TestCase):
    """`lab.py <clock> colour` - one command, no questions asked."""

    def test_the_defaults_are_the_conservative_run(self):
        options = lab.colour_options([])
        self.assertEqual(options["cells"], lab.COLOUR_CELLS)
        self.assertEqual(options["rung"], lab.COLOUR_RUNG)
        self.assertEqual(options["settle_ms"], lab.COLOUR_SETTLE_MS)
        self.assertEqual(options["json_path"], lab.COLOUR_FILE)
        self.assertIsNone(options["csv_path"])
        self.assertFalse(options["full"])
        self.assertEqual(options["stack_id"], lab.COLOUR_STACK_ID)

    def test_every_option_can_be_given(self):
        options = lab.colour_options(["--full", "--rung", "3",
                                      "--cells", "8,5;8,6;7,4",
                                      "--settle-ms", "250",
                                      "--json", "out.json", "--csv", "out.csv",
                                      "--stack-id", "mask-then-wh73"])
        self.assertTrue(options["full"])
        self.assertEqual(options["rung"], 3)
        self.assertEqual(options["cells"], [(8, 5), (8, 6), (7, 4)])
        self.assertEqual(options["settle_ms"], 250)
        self.assertEqual(options["json_path"], "out.json")
        self.assertEqual(options["csv_path"], "out.csv")
        self.assertEqual(options["stack_id"], "mask-then-wh73")

    def test_a_mistyped_cell_is_refused_rather_than_guessed(self):
        for bad in ("8", "8,5;", "8,5;x,2", "8-5", "8,5,6"):
            with self.assertRaises(SystemExit, msg=bad):
                lab.colour_options(["--cells", bad])

    def test_a_cell_off_the_face_is_refused(self):
        for bad in ("10,5", "8,11", "-1,5"):
            with self.assertRaises(SystemExit, msg=bad):
                lab.colour_options(["--cells", bad])

    def test_the_command_runs_the_sweep_and_owns_nothing_around_it(self):
        # `colour` takes the strip itself, for the sweep and for nothing else,
        # so it must not go through main's generic take/release wrapper - that
        # would hold the strip across the whole command including the writing.
        fake = FakeLab()
        seen = {}

        def recorder(client, **kwargs):
            seen["client"] = client
            seen["kwargs"] = kwargs
            # The shape `colour_sweep` really returns, so the stub cannot let
            # a summary through that would fail against the actual function.
            return {"stackId": kwargs["stack_id"], "measures": lab.COLOUR_MEASURES,
                    "cells": [list(cell) for cell in kwargs["cells"]],
                    "grid": [list(colour) for colour in lab.colour_grid()],
                    "percents": lab.COLOUR_PERCENTS, "rung": kwargs["rung"],
                    "settleMs": kwargs["settle_ms"], "clock": {}, "sensor": {},
                    "frames": []}

        argv, factory, sweep = sys.argv, lab.Lab, lab.colour_sweep
        try:
            sys.argv = ["lab.py", "clock.local", "colour", "--csv", "out.csv"]
            lab.Lab = lambda host: fake
            lab.colour_sweep = recorder
            lab.main()
        finally:
            sys.argv, lab.Lab, lab.colour_sweep = argv, factory, sweep

        self.assertIs(seen["client"], fake)
        self.assertEqual(seen["kwargs"]["csv_path"], "out.csv")
        # main did not take the strip on its own account.
        self.assertEqual([call for call in fake.calls if call[0] == "take"], [])


if __name__ == "__main__":
    unittest.main()
