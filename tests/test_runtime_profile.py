# -*- coding: utf-8 -*-
"""The compact runtime profile: what the clock is actually given.

The reviewed profile in `artifacts/` is 43 KB of provenance - every
observation, every replicate, every note about how a round was taken. None of
that is arithmetic, and a firmware that parses it is a firmware carrying a
laboratory notebook into a 3.5 MB partition it shares with a web UI.

So the generator emits a second, derived document: the same model with only
the numbers the evaluator reads. Three things make it safe to do that:

* it is **derived deterministically** - two runs of the generator over the same
  reviewed profile produce the same bytes, so the file can be committed and a
  diff only ever shows a value that moved;
* it **verifies against itself** - the file carries a checksum over a payload
  whose byte layout is fixed, so the clock can refuse an edited one; and
* it **answers the same as the full profile** - the parity tests below run both
  evaluators over the knots, between them, across the hue seam and outside the
  measured range, and require the same integer percentage from each.

What is deliberately *not* dropped is the profile's own status. The reviewed
HUE240 repeat does not meet its acceptance goal; that is provenance, not a
reason to withhold the model, and a file that quietly omitted it would be
claiming an accuracy nobody measured.
"""
import json
import math
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "scripts"))

import colour_luminance as cl
import factory_luminance as fl

ARTIFACTS = os.path.join(HERE, "..", "artifacts")
REVIEWED = os.path.join(ARTIFACTS, "2026-08-27-hue240-repeat-profile.json")
EVALUATION = os.path.join(ARTIFACTS, "2026-08-27-hue240-repeat-evaluation.json")

# Where the build puts it. `web/public` is copied into the LittleFS image by
# Vite the same way `zones.json` is - generated, committed, and never fetched
# at runtime, which is also what keeps `emptyOutDir` from eating it.
SHIPPED = os.path.join(HERE, "..", "web", "public", "factory-luminance.json")


def reviewed():
    with open(REVIEWED, "r") as handle:
        return json.load(handle)


def evaluation():
    with open(EVALUATION, "r") as handle:
        return json.load(handle)


class RuntimeProfileShape(unittest.TestCase):
    """What is in it, and - just as much - what is not."""

    def setUp(self):
        self.profile = reviewed()
        self.payload = fl.runtime_profile(self.profile)

    def test_only_the_keys_the_evaluator_reads(self):
        self.assertEqual(sorted(self.payload), sorted([
            "drive", "hueKnots", "huePeriod", "levels", "modelId",
            "percentRange", "profileId", "runtimeSchema", "satFade",
            "schemaVersion", "sourceChecksum", "stackId", "status", "weights",
        ]))

    def test_the_notebook_does_not_travel(self):
        text = json.dumps(self.payload)
        for gone in ("observations", "replicates", "whiteAnchors",
                     "diagnostics", "measurementNote", "targeted",
                     "logOutputResidualObserved", "excluded"):
            self.assertNotIn(gone, text)

    def test_a_level_is_five_numbers_and_two_flags(self):
        level = self.payload["levels"][0]
        self.assertEqual(sorted(level), ["bounds", "censored", "logLux",
                                         "residuals", "white"])
        self.assertEqual(len(level["residuals"]), len(fl.HUE_KNOTS))
        self.assertEqual(len(level["bounds"]), len(fl.HUE_KNOTS))

    def test_the_hue_knots_and_the_period_travel(self):
        self.assertEqual(self.payload["hueKnots"], list(fl.HUE_KNOTS))
        self.assertEqual(self.payload["huePeriod"], fl.HUE_PERIOD)

    def test_the_hardware_model_travels_with_it(self):
        self.assertEqual(self.payload["drive"]["levels"],
                         self.profile["source"]["driveLevels"])
        self.assertEqual(self.payload["drive"]["response"],
                         self.profile["source"]["driveResponse"])
        self.assertEqual(self.payload["weights"],
                         self.profile["source"]["photopicWeights"])

    def test_the_identity_of_what_it_came_from(self):
        self.assertEqual(self.payload["sourceChecksum"],
                         self.profile["checksum"]["value"])
        self.assertEqual(self.payload["profileId"], self.profile["profileId"])
        self.assertEqual(self.payload["stackId"], self.profile["stackId"])
        self.assertEqual(self.payload["modelId"], self.profile["modelId"])

    def test_the_bounds_survive_as_flags(self):
        # "lower" in the reviewed profile: the observation was at the ceiling,
        # so the residual is "at least this much". A flag rather than the word,
        # because the firmware has one kind and a string would invite a second.
        flags = [flag for level in self.payload["levels"]
                 for flag in level["bounds"]]
        self.assertEqual(sorted(set(flags)), [0, 1])

    def test_the_censored_level_is_still_marked(self):
        censored = [level["censored"] for level in self.payload["levels"]]
        self.assertEqual(censored,
                         [level["censored"] for level in self.profile["levels"]])

    def test_the_status_is_not_hidden(self):
        status = self.payload["status"]
        # The reviewed profile misses its own acceptance goal at hue 240. That
        # travels: a model shipped without its limitation reads as one that
        # has none.
        self.assertIn("monotone", status)
        self.assertFalse(status["monotone"])
        self.assertIn("supersededBy", status)

    def test_the_status_can_carry_the_evaluation(self):
        payload = fl.runtime_profile(self.profile, evaluation=evaluation())
        status = payload["status"]
        self.assertFalse(status["acceptanceMet"])
        self.assertAlmostEqual(status["rms"], 6.8664509135, places=6)
        self.assertEqual(status["maxError"], 15)
        self.assertEqual(status["worstHue"], 240)

    def test_an_evaluation_of_another_profile_is_refused(self):
        other = evaluation()
        other["profileChecksum"] = "0" * 64
        with self.assertRaises(fl.MeasurementError):
            fl.runtime_profile(self.profile, evaluation=other)


class RuntimeDocument(unittest.TestCase):
    """The file: fixed layout, so a small machine can check it."""

    def setUp(self):
        self.profile = reviewed()
        self.text = fl.runtime_text(self.profile)

    def test_deterministic(self):
        self.assertEqual(self.text, fl.runtime_text(reviewed()))

    def test_small_enough_to_ship(self):
        # A 3.5 MB partition holding a web UI. The reviewed profile is 43 KB;
        # this has to be a rounding error beside the JS bundle, or the argument
        # for deriving it at all does not hold.
        self.assertLess(len(self.text.encode("utf-8")), 6144)

    def test_the_payload_sits_at_a_fixed_offset(self):
        # The firmware does not sort JSON keys, so it cannot re-canonicalise a
        # parsed document to check a checksum. It hashes a substring instead,
        # which is only safe if the substring is where it is said to be.
        self.assertTrue(self.text.startswith(fl.RUNTIME_HEAD))
        self.assertEqual(self.text[fl.RUNTIME_CHECKSUM_AT:
                                   fl.RUNTIME_CHECKSUM_AT + 64],
                         json.loads(self.text)["checksum"]["value"])
        self.assertEqual(self.text[fl.RUNTIME_PAYLOAD_AT - len(fl.RUNTIME_MARK):
                                   fl.RUNTIME_PAYLOAD_AT], fl.RUNTIME_MARK)
        self.assertTrue(self.text.endswith("}"))

    def test_the_checksum_is_over_exactly_that_substring(self):
        import hashlib
        body = self.text[fl.RUNTIME_PAYLOAD_AT:-1]
        digest = hashlib.sha256(body.encode("utf-8")).hexdigest()
        self.assertEqual(digest, json.loads(self.text)["checksum"]["value"])

    def test_it_verifies(self):
        self.assertTrue(fl.verify_runtime(self.text))

    def test_a_moved_digit_does_not(self):
        spoilt = self.text.replace('"huePeriod":360', '"huePeriod":359')
        self.assertNotEqual(spoilt, self.text)
        self.assertFalse(fl.verify_runtime(spoilt))

    def test_a_trailing_byte_does_not(self):
        self.assertFalse(fl.verify_runtime(self.text + " "))

    def test_the_shipped_file_is_what_the_generator_makes(self):
        # Generated, committed and shipped - the same rule as zones.json and
        # the icons. Regenerate with `python3 scripts/factory_luminance.py
        # runtime`; a run with nothing changed leaves the file byte-identical.
        self.assertTrue(os.path.exists(SHIPPED),
                        "web/public/factory-luminance.json is missing")
        with open(SHIPPED, "r") as handle:
            self.assertEqual(handle.read(),
                             fl.runtime_text(self.profile, evaluation=evaluation()))


class RuntimeRefusals(unittest.TestCase):
    """A profile the evaluator may not act on, and what it says about it."""

    def loaded(self, mangle):
        text = fl.runtime_text(reviewed())
        document = json.loads(text)
        mangle(document["payload"])
        return fl.load_runtime(fl.reseal_runtime(document["payload"]))

    def test_a_good_one_loads(self):
        self.assertIsNotNone(fl.load_runtime(fl.runtime_text(reviewed())))

    def test_an_unchecksummed_one_is_refused(self):
        text = fl.runtime_text(reviewed())
        with self.assertRaises(fl.MeasurementError):
            fl.load_runtime(text.replace('"huePeriod":360', '"huePeriod":359'))

    def test_a_later_runtime_schema_is_refused(self):
        with self.assertRaises(fl.MeasurementError):
            self.loaded(lambda p: p.update(runtimeSchema=2))

    def test_another_model_is_refused(self):
        with self.assertRaises(fl.MeasurementError):
            self.loaded(lambda p: p.update(modelId="something-else"))

    def test_one_level_is_refused(self):
        with self.assertRaises(fl.MeasurementError):
            self.loaded(lambda p: p.update(levels=p["levels"][:1]))

    def test_levels_out_of_order_are_refused(self):
        def swap(p):
            p["levels"] = list(reversed(p["levels"]))
        with self.assertRaises(fl.MeasurementError):
            self.loaded(swap)

    def test_a_short_residual_row_is_refused(self):
        def clip(p):
            p["levels"][0]["residuals"] = p["levels"][0]["residuals"][:3]
        with self.assertRaises(fl.MeasurementError):
            self.loaded(clip)

    def test_a_nonfinite_number_is_refused(self):
        # Not expressible in JSON, so it arrives as a string or as null - both
        # of which used to reach arithmetic and come back as a percentage.
        def spoil(p):
            p["levels"][0]["white"] = "nan"
        with self.assertRaises(fl.MeasurementError):
            self.loaded(spoil)

    def test_a_fade_this_code_does_not_know_is_refused(self):
        with self.assertRaises(fl.MeasurementError):
            self.loaded(lambda p: p["satFade"].update(kind="cosine"))


class ResealedButMalformed(unittest.TestCase):
    """The checksum says "nobody edited this since it was written".

    It does not say the thing that was written is a profile. Anyone holding
    `reseal_runtime` - this module, the generator, and whatever wrote the file
    in `web/public/` - can produce a document that verifies perfectly and
    carries nonsense, and a checksum is exactly the reassurance that stops the
    next reader looking. So every field the evaluator or the read-out touches
    is checked *after* the checksum, on the resealed document, and each of the
    cases below is one that used to sail through.

    The firmware's counterpart is FactoryLuminance::begin(), which reads the
    same fields in the same order through ArduinoJson. Where the two could
    differ they are made to agree deliberately: a JSON `20.0` is refused here
    because `is<int>()` refuses it there, and a bound is required to be `true`
    or `false` rather than 1 or 0 for the same reason.
    """

    def loaded(self, mangle):
        document = json.loads(fl.runtime_text(reviewed()))
        mangle(document["payload"])
        return fl.load_runtime(fl.reseal_runtime(document["payload"]))

    def refuses(self, mangle):
        with self.assertRaises(fl.MeasurementError):
            self.loaded(mangle)

    def test_the_resealed_document_really_does_verify(self):
        # Or every refusal below would be passing for the wrong reason: a
        # checksum complaint reads exactly like a shape complaint from here.
        document = json.loads(fl.runtime_text(reviewed()))
        document["payload"]["profileId"] = "something-else-entirely"
        text = fl.reseal_runtime(document["payload"])
        self.assertTrue(fl.verify_runtime(text))
        self.assertIsNotNone(fl.load_runtime(text))

    # --- the identity -----------------------------------------------------
    #
    # Both of these key something. `profileId` is what the read-out names when
    # somebody asks which measurement their clock is running; `sourceChecksum`
    # is what the stored colour corrections are filed under, so an empty one
    # makes every correction ever made look as though it belonged to this
    # profile, whatever it was really learned on.

    def test_a_profile_with_no_id_is_refused(self):
        for value in ("", None, 0, [], {}, False):
            self.refuses(lambda p, v=value: p.update(profileId=v))

    def test_an_id_that_is_not_a_string_is_refused(self):
        self.refuses(lambda p: p.update(profileId=17))

    def test_a_profile_with_no_source_checksum_is_refused(self):
        for value in ("", None, 0, [], {}, False):
            self.refuses(lambda p, v=value: p.update(sourceChecksum=v))

    def test_a_source_checksum_that_is_not_a_string_is_refused(self):
        self.refuses(lambda p: p.update(sourceChecksum=["0" * 64]))

    # --- the saturation fade ----------------------------------------------

    def test_a_fade_that_runs_backwards_is_refused(self):
        # Not merely different from its other end. Reversed, the colour
        # residual would be whole on a white face - where there is no colour to
        # correct - and absent at full saturation, where the whole correction
        # lives. That is the model meaning the opposite of itself, and it is
        # refused rather than quietly flipped.
        self.refuses(lambda p: p["satFade"].update(zeroAtSat=100, fullAtSat=0))

    def test_a_fade_edge_that_is_not_a_whole_number_is_refused(self):
        for value in (0.5, "0", None, True, [0]):
            self.refuses(lambda p, v=value: p["satFade"].update(zeroAtSat=v))
            self.refuses(lambda p, v=value: p["satFade"].update(fullAtSat=v))

    def test_a_fade_edge_written_as_a_float_is_refused(self):
        # 100.0 is 100 in Python and is not an integer to ArduinoJson's
        # `is<int>()`. A value this side accepted and that side refused would
        # be a profile that passes the generator's own tests and will not load
        # on a clock.
        self.refuses(lambda p: p["satFade"].update(fullAtSat=100.0))

    def test_a_fade_edge_outside_a_saturation_is_refused(self):
        self.refuses(lambda p: p["satFade"].update(zeroAtSat=-1))
        self.refuses(lambda p: p["satFade"].update(fullAtSat=101))

    # --- the flags --------------------------------------------------------

    def test_a_bound_that_is_not_a_boolean_is_refused(self):
        # A bound says "at least this much" about one corner of the grid, and
        # every answer that touches the corner has to admit it. Read as
        # "anything truthy", the string "no" is a bound and so is 0.0's
        # neighbour; read as a number, a 2 is a bound with no meaning attached.
        for value in (1, 0, "lower", "", None, 0.0, [True]):
            self.refuses(
                lambda p, v=value: p["levels"][0]["bounds"].__setitem__(0, v))

    def test_every_bound_is_looked_at_not_just_the_first(self):
        self.refuses(
            lambda p: p["levels"][-1]["bounds"].__setitem__(-1, 1))

    def test_the_censored_flag_that_is_not_a_boolean_is_refused(self):
        for value in (1, 0, "yes", "", None, [False]):
            self.refuses(lambda p, v=value: p["levels"][0].update(censored=v))

    def test_a_level_that_does_not_say_whether_it_is_censored_is_refused(self):
        self.refuses(lambda p: p["levels"][0].pop("censored"))

    # --- and the good one still loads -------------------------------------

    def test_the_generator_writes_what_this_accepts(self):
        # The check that keeps the two halves of this from drifting apart: a
        # rule tightened here without the generator following it would make the
        # shipped file unloadable, which is a failure worth having land in a
        # test run rather than on a clock.
        payload = fl.load_runtime(fl.runtime_text(reviewed()))
        for level in payload["levels"]:
            self.assertIsInstance(level["censored"], bool)
            for flag in level["bounds"]:
                self.assertIsInstance(flag, bool)
        self.assertIsInstance(payload["satFade"]["zeroAtSat"], int)
        self.assertNotIsInstance(payload["satFade"]["zeroAtSat"], bool)
        self.assertLess(payload["satFade"]["zeroAtSat"],
                        payload["satFade"]["fullAtSat"])
        self.assertTrue(payload["profileId"])
        self.assertTrue(payload["sourceChecksum"])


class RuntimeParity(unittest.TestCase):
    """The compact evaluator answers what the full one answers."""

    @classmethod
    def setUpClass(cls):
        cls.profile = reviewed()
        cls.runtime = fl.load_runtime(fl.runtime_text(cls.profile))

    def both(self, lux, hue, sat):
        full = fl.evaluate(self.profile, lux, hue, sat)
        compact = fl.evaluate_runtime(self.runtime, lux, hue, sat)
        return full, compact

    def same(self, lux, hue, sat):
        full, compact = self.both(lux, hue, sat)
        where = "%g lx, hue %d, sat %d" % (lux, hue, sat)
        self.assertEqual(compact.percent, full.percent, where)
        self.assertAlmostEqual(compact.target, full.target, places=9, msg=where)
        self.assertEqual(compact.limited, full.limited, where)
        self.assertEqual(compact.bound, full.bound, where)
        self.assertEqual(compact.lux_clamped, full.lux_clamped, where)

    def test_at_every_knot(self):
        for level in self.profile["levels"]:
            for hue in fl.HUE_KNOTS:
                self.same(level["lux"], hue, 100)

    def test_between_the_knots(self):
        for lux in (0.03, 0.11, 0.4, 1.9, 4.5, 8.0):
            for hue in (15, 45, 91, 137, 210, 259, 331):
                self.same(lux, hue, 100)

    def test_across_the_seam(self):
        # 300 -> 0 is the segment with no knot at its far end in the stored
        # order, which is exactly the one that gets written wrong.
        for hue in (300, 305, 315, 330, 345, 355, 359, 0):
            self.same(0.5, hue, 100)

    def test_white_is_the_same_answer_at_every_hue(self):
        answers = {fl.evaluate_runtime(self.runtime, 0.5, hue, 0).percent
                   for hue in range(0, 360, 17)}
        self.assertEqual(len(answers), 1)

    def test_down_the_saturation_fade(self):
        for sat in (0, 1, 17, 33, 50, 66, 99, 100):
            self.same(0.5, 240, sat)

    def test_outside_the_measured_range(self):
        for lux in (0.001, 0.005, 0.019, 10.5, 90.0, 5000.0):
            self.same(lux, 240, 100)
            self.assertIsNotNone(
                fl.evaluate_runtime(self.runtime, lux, 240, 100).lux_clamped)

    def test_the_bound_travels(self):
        # The censored level is the top one; a colour read off it says "at
        # least this much" and the answer has to admit it.
        top = self.profile["levels"][-1]
        bounded = [knot for knot, residual in zip(fl.HUE_KNOTS, top["residuals"])
                   if residual["bound"]]
        self.assertTrue(bounded, "the reviewed profile has no bounded corner")
        for hue in bounded:
            self.assertEqual(
                fl.evaluate_runtime(self.runtime, top["lux"], hue, 100).bound,
                "lower")

    def test_a_colour_that_runs_out_of_slider_says_so(self):
        # At the top level this profile asks some colours for more light than
        # they can emit at any percentage in the range. That is the gamut, not
        # a fault, and it has to be said rather than shown as a quiet 100 %.
        # Which colours is a fact about this measurement: on the reviewed
        # profile it is 180 and 300, while 240 reaches its target exactly.
        for hue in (180, 300):
            found = fl.evaluate_runtime(self.runtime, 10.0, hue, 100)
            self.assertEqual(found.limited, "ceiling", "hue %d" % hue)
            self.assertEqual(found.percent, self.runtime["percentRange"]["max"])
        self.assertIsNone(fl.evaluate_runtime(self.runtime, 10.0, 240, 100).limited)

    def test_light_has_to_be_positive(self):
        with self.assertRaises(fl.MeasurementError):
            fl.evaluate_runtime(self.runtime, 0.0, 0, 100)


class GridMonotonicity(unittest.TestCase):
    """Whether the shipped grid rises with light, measured on the grid itself.

    `status.monotone` in the profile is a *finding about the observations*, and
    the clock must not take it as a statement about the numbers it was handed:
    the two are written by different code at different times, and a status
    field is exactly the thing an edit would leave behind while moving a value.

    So the dip is computed from the installed grid, in the coordinate that
    matters - the target log output, white plus residual, at every hue knot.

    **And the reviewed profile does not dip at all**, which is the finding
    worth writing down: `status.monotone` on it is `false`, and that is about
    the *observations*, where hue 240 falls a quarter of a decade. The isotonic
    step then pools the levels those disagreements sit between, and what comes
    out rises everywhere. Reading the status field as a statement about the
    grid would have the clock reporting a fault it does not have.
    """

    def setUp(self):
        self.payload = fl.load_runtime(fl.runtime_text(reviewed()))

    def test_the_shipped_grid_does_not_dip(self):
        self.assertEqual(fl.grid_dip(self.payload), 0.0)

    def test_although_the_observations_behind_it_do(self):
        stated = reviewed()["diagnostics"]
        self.assertFalse(stated["monotone"])
        self.assertTrue(stated["nonMonotoneHues"])
        # A quarter of a decade at hue 240, in the observed values - and none
        # of it left in the grid above. The two must not be confused.
        worst = max(one["fromTarget"] - one["toTarget"]
                    for one in stated["nonMonotoneHues"])
        self.assertGreater(worst, 0.2)

    def test_the_isotonic_step_is_what_removed_it(self):
        # Levels three and four carry the same white output: that is the pool
        # the disagreement was resolved into, and it is visible in the shipped
        # numbers rather than only in a note about them.
        levels = self.payload["levels"]
        self.assertEqual(levels[2]["white"], levels[3]["white"])

    def test_a_grid_that_rises_everywhere_has_no_dip(self):
        payload = json.loads(json.dumps(self.payload))
        for index, level in enumerate(payload["levels"]):
            level["white"] = -3.0 + index
            level["residuals"] = [0.0] * len(payload["hueKnots"])
        self.assertEqual(fl.grid_dip(payload), 0.0)

    def test_a_grid_that_falls_off_a_cliff_is_a_fault(self):
        payload = json.loads(json.dumps(self.payload))
        payload["levels"][-1]["white"] -= 3.0
        self.assertGreater(fl.grid_dip(payload), fl.GRID_MAX_DIP)
        with self.assertRaises(fl.MeasurementError):
            fl.load_runtime(fl.reseal_runtime(payload))

    def test_the_white_line_is_looked_at_too(self):
        # A dip can be in the white baseline with every residual flat, which is
        # a different bug from one in a single hue and would be missed by a
        # check that only walked the colour rows.
        payload = json.loads(json.dumps(self.payload))
        for level in payload["levels"]:
            level["residuals"] = [0.0] * len(payload["hueKnots"])
        payload["levels"][2]["white"] = payload["levels"][1]["white"] - 0.02
        self.assertAlmostEqual(fl.grid_dip(payload), 0.02, places=9)

    def test_the_number_travels_to_the_firmware(self):
        # The host harness compares its own reading of the grid against this
        # one. A dip found by one implementation and not the other is the two
        # walking different grids, which is what the fixture exists to catch.
        flat = fl.runtime_flat(self.payload)
        self.assertIn("%.17g" % fl.grid_dip(self.payload), flat)


class GoldenVectors(unittest.TestCase):
    """The vectors the firmware evaluator is checked against.

    Committed, because the C++ side has no Python and the Python side has no
    C++ compiler in every environment this runs in. The file is the contract
    between them; this test is what keeps it honest.
    """

    PATH = os.path.join(HERE, "golden", "factory_luminance_vectors.json")

    def test_the_committed_vectors_are_what_the_model_says(self):
        self.assertTrue(os.path.exists(self.PATH),
                        "run scripts/factory_luminance.py vectors")
        with open(self.PATH, "r") as handle:
            golden = json.load(handle)

        runtime = fl.load_runtime(fl.runtime_text(reviewed(),
                                                  evaluation=evaluation()))
        self.assertEqual(golden["sourceChecksum"], runtime["sourceChecksum"])
        self.assertTrue(golden["cases"])
        for case in golden["cases"]:
            found = fl.evaluate_runtime(runtime, case["lux"], case["hue"],
                                        case["sat"])
            where = "%g lx, hue %d, sat %d" % (case["lux"], case["hue"],
                                               case["sat"])
            self.assertEqual(found.percent, case["percent"], where)
            self.assertAlmostEqual(found.target, case["target"], places=9,
                                   msg=where)
            self.assertEqual(found.limited or "", case["limited"], where)
            self.assertEqual(found.bound or "", case["bound"], where)
            self.assertEqual(found.lux_clamped or "", case["clamped"], where)

    def test_they_cover_the_places_that_break(self):
        with open(self.PATH, "r") as handle:
            cases = json.load(handle)["cases"]
        hues = {case["hue"] for case in cases}
        sats = {case["sat"] for case in cases}
        self.assertTrue({0, 300, 330, 359} <= hues, "the seam is not covered")
        self.assertTrue({0, 50, 100} <= sats, "the fade is not covered")
        self.assertTrue(any(case["clamped"] for case in cases))
        self.assertTrue(any(case["limited"] for case in cases))
        self.assertTrue(any(case["bound"] for case in cases))


if __name__ == "__main__":
    unittest.main()
