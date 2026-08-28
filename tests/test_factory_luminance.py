# -*- coding: utf-8 -*-
"""The factory colour profile: what it is built from, and what it refuses.

The fixtures are committed copies (`tests/fixtures/`, with their provenance in
the README beside them) rather than the working paths the measurements came
from. A test that reads a mutable path measures whatever happens to be there
today, which is the one thing a checksum test must not do.
"""
import contextlib
import copy
import csv
import io
import json
import os
import shutil
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "scripts"))

import colour_luminance as cl
import factory_luminance as fl

FIXTURES = os.path.join(HERE, "fixtures")
MEASUREMENTS_PATH = os.path.join(
    FIXTURES, "2026-08-26-manual-colour-brightness-points.json")
COUPLING_PATH = os.path.join(
    FIXTURES, "2026-08-26-current-diffuser-before-mask-coupling.json")


def measurements_doc():
    with open(MEASUREMENTS_PATH, "r") as handle:
        return json.load(handle)


def coupling_doc():
    with open(COUPLING_PATH, "r") as handle:
        return json.load(handle)


def drive():
    return cl.DriveTable.from_record(coupling_doc(), source="fixture")


def parsed():
    return fl.load_measurements(measurements_doc(), drive=drive())


class ParseTest(unittest.TestCase):
    """Shape before arithmetic, and order preserved."""

    def test_the_fixture_parses_into_six_rounds_in_ascending_light(self):
        self.assertEqual([round_.lux for round_ in parsed().rounds],
                         [0.02, 0.07, 0.15, 0.5, 1.9, 10.0])

    def test_every_round_has_the_six_knots_once_and_at_least_one_anchor(self):
        for round_ in parsed().rounds:
            self.assertEqual(sorted(o.hue for o in round_.colours),
                             sorted(fl.HUE_KNOTS), round_.round_id)
            self.assertTrue(round_.anchors, round_.round_id)
            for colour in round_.colours:
                self.assertEqual(colour.sat, fl.COLOUR_SAT)

    def test_the_undeclared_round_at_0_15_lux_is_recovered(self):
        # It carries no `roundId` and no `sequence`: the only thing its points
        # state about belonging together is their exact ambient level, and the
        # only thing they state about order is the order they are written in.
        recovered = [r for r in parsed().rounds if not r.declared]
        self.assertEqual(len(recovered), 1)
        self.assertEqual(recovered[0].lux, 0.15)
        self.assertEqual([o.index for o in recovered[0].observations],
                         [8, 9, 10, 11, 12, 13, 14, 15])

    def test_order_within_a_round_follows_sequence_where_there_is_one(self):
        by_id = {r.round_id: r for r in parsed().rounds}
        round_ = by_id["dark-0.07-lux-drift-controlled"]
        self.assertEqual([o.sequence for o in round_.observations],
                         list(range(10)))

    def test_the_uncontrolled_two_lux_points_are_excluded_with_a_reason(self):
        # Eight points at eight different levels between 2.0 and 2.8 lx, taken
        # without interleaved anchors, one of them at sat=80 on hue 140. They
        # are superseded by the drift-controlled 1.9 lx round.
        excluded = parsed().excluded
        self.assertEqual([e["index"] for e in excluded], [0, 1, 2, 3, 4, 5, 6, 7])
        for entry in excluded:
            self.assertTrue(entry["reason"])
        self.assertNotIn(2.0, [r.lux for r in parsed().rounds])

    def test_the_ten_lux_round_is_the_only_censored_one(self):
        censored = [r.round_id for r in parsed().rounds if r.censored]
        self.assertEqual(censored,
                         ["bright-10-lux-drift-controlled-ceiling-limited"])

    def test_stored_output_is_recomputed_and_has_to_agree(self):
        doc = measurements_doc()
        doc["points"][20]["relativePhotopicOutput"] = 0.5
        with self.assertRaises(fl.MeasurementError) as caught:
            fl.load_measurements(doc, drive=drive())
        self.assertIn("point 20", str(caught.exception))

    def test_a_proportional_drive_table_cannot_read_this_record(self):
        # The record was produced with a measured drive response. Reading it
        # with the assumed proportional one is not a small error - it is a
        # different lamp - so it is refused rather than warned about.
        with self.assertRaises(fl.MeasurementError):
            fl.load_measurements(measurements_doc())


class RefusalTest(unittest.TestCase):
    """Malformed shapes, non-finite numbers, and nothing inferred."""

    def refuse(self, mutate, fragment):
        doc = measurements_doc()
        mutate(doc)
        with self.assertRaises(fl.MeasurementError) as caught:
            fl.load_measurements(doc, drive=drive())
        self.assertIn(fragment, str(caught.exception))

    def test_a_list_where_the_record_belongs(self):
        with self.assertRaises(fl.MeasurementError):
            fl.load_measurements([], drive=drive())

    def test_points_that_are_not_an_array(self):
        self.refuse(lambda d: d.__setitem__("points", {"a": 1}), "points")

    def test_a_point_that_is_not_an_object(self):
        self.refuse(lambda d: d["points"].__setitem__(20, [1, 2]), "point 20")

    def test_no_points_at_all(self):
        self.refuse(lambda d: d.__setitem__("points", []), "no points")

    def test_a_missing_hue_is_not_inferred(self):
        self.refuse(lambda d: d["points"][20].pop("hue"), "'hue'")

    def test_a_missing_saturation_is_not_inferred(self):
        self.refuse(lambda d: d["points"][20].pop("sat"), "'sat'")

    def test_a_null_hue_is_not_inferred_either(self):
        self.refuse(lambda d: d["points"][20].__setitem__("hue", None), "'hue'")

    def test_a_boolean_is_not_a_number(self):
        # `True == 1` in Python, so a boolean sails through every numeric
        # comparison and lands as a percentage of one.
        self.refuse(lambda d: d["points"][20].__setitem__("percent", True),
                    "a boolean")

    def test_a_non_finite_lux(self):
        self.refuse(lambda d: d["points"][20].__setitem__("lux", float("inf")),
                    "finite")

    def test_a_nan_output(self):
        self.refuse(
            lambda d: d["points"][20].__setitem__("relativePhotopicOutput",
                                                  float("nan")),
            "finite")

    def test_a_negative_lux(self):
        self.refuse(lambda d: d["points"][20].__setitem__("lux", -1.0),
                    "positive")

    def test_a_hue_outside_the_circle(self):
        self.refuse(lambda d: d["points"][20].__setitem__("hue", 400), "0..359")

    def test_a_percent_above_a_hundred(self):
        self.refuse(lambda d: d["points"][20].__setitem__("percent", 120),
                    "0..100")

    def test_a_censored_flag_that_is_a_number(self):
        self.refuse(lambda d: d["points"][20].__setitem__("censored", 1),
                    "true or false")

    def test_a_record_with_no_stack_identity(self):
        self.refuse(lambda d: d.pop("stackId"), "stackId")

    def test_a_declared_count_that_disagrees_with_the_points(self):
        # The round is dropped rather than repaired, and dropping every round
        # is what makes this visible.
        doc = measurements_doc()
        for entry in doc["rounds"]:
            entry["count"] = 99
        result = fl.load_measurements(doc, drive=drive())
        self.assertEqual([r.lux for r in result.rounds], [0.15])
        self.assertIn("declared as 99",
                      " ".join(e["reason"] for e in result.excluded))

    def test_two_rounds_declared_under_one_id(self):
        self.refuse(lambda d: d["rounds"].append(dict(d["rounds"][0])),
                    "declared twice")

    def test_a_repeated_hue_in_a_round_breaks_it(self):
        doc = measurements_doc()
        doc["points"][18]["hue"] = 0            # 180 -> a second hue 0
        doc["points"][18].pop("relativePhotopicOutput")
        result = fl.load_measurements(doc, drive=drive())
        self.assertNotIn("dark-0.07-lux-drift-controlled",
                         [r.round_id for r in result.rounds])
        self.assertIn("repeated 0",
                      " ".join(e["reason"] for e in result.excluded))

    def test_a_round_spread_over_two_ambient_levels_breaks_it(self):
        doc = measurements_doc()
        doc["points"][20]["lux"] = 0.071
        result = fl.load_measurements(doc, drive=drive())
        self.assertNotIn("dark-0.07-lux-drift-controlled",
                         [r.round_id for r in result.rounds])
        self.assertIn("not one ambient level",
                      " ".join(e["reason"] for e in result.excluded))

    def test_a_round_with_no_white_anchor_breaks_it(self):
        doc = measurements_doc()
        doc["points"] = [p for p in doc["points"]
                         if not (p.get("roundId") == "mid-0.5-lux-drift-controlled"
                                 and p["sat"] == 0)]
        doc["rounds"] = [r for r in doc["rounds"]
                         if r["id"] != "mid-0.5-lux-drift-controlled"]
        result = fl.load_measurements(doc, drive=drive())
        self.assertIn("no white anchor",
                      " ".join(e["reason"] for e in result.excluded))

    def test_a_record_whose_every_round_is_broken_is_refused(self):
        doc = measurements_doc()
        for point in doc["points"]:
            point["sat"] = 100
            point.pop("relativePhotopicOutput", None)
        with self.assertRaises(fl.MeasurementError) as caught:
            fl.load_measurements(doc, drive=drive())
        self.assertIn("no complete round", str(caught.exception))


class DriftTest(unittest.TestCase):
    """The observer drifts over a round, and the anchors are how that is seen."""

    def round_by_id(self, round_id):
        return {r.round_id: r for r in parsed().rounds}[round_id]

    def test_white_between_two_equal_anchors_is_that_value(self):
        round_ = self.round_by_id("dark-room-0.02-lux-drift-controlled")
        # All four anchors sit at 30 %, so nothing was drifting and every
        # colour in the round is measured against the same white.
        for sequence in range(10):
            self.assertAlmostEqual(fl.white_at(round_, sequence),
                                   cl.log10_output(0.030022499999999997), 12)

    def test_white_is_interpolated_across_a_drifting_pair(self):
        # 0.07 lx: anchors at sequences 0, 3, 6, 9 reading 35, 35, 38, 35 %.
        # The colour at sequence 4 sits one third of the way from the 35 %
        # anchor to the 38 % one, in log output.
        round_ = self.round_by_id("dark-0.07-lux-drift-controlled")
        low = cl.log10_output(0.05234874999999999)     # 35 %
        high = cl.log10_output(0.07334249999999999)    # 38 %
        self.assertAlmostEqual(fl.white_at(round_, 4), low + (high - low) / 3.0, 12)
        self.assertAlmostEqual(fl.white_at(round_, 5),
                               low + (high - low) * 2.0 / 3.0, 12)
        self.assertAlmostEqual(fl.white_at(round_, 8),
                               high + (low - high) * 2.0 / 3.0, 12)

    def test_outside_the_anchors_the_nearest_one_stands(self):
        # The 0.15 lx round has anchors at its two ends only, so nothing is
        # outside them; 0.07 has one at each end too. Held flat rather than
        # extrapolated: an observer's drift before the first anchor was never
        # observed, and a line through two points run backwards is a guess.
        round_ = self.round_by_id("dark-0.07-lux-drift-controlled")
        self.assertEqual(fl.white_at(round_, -5), fl.white_at(round_, 0))
        self.assertEqual(fl.white_at(round_, 50), fl.white_at(round_, 9))

    def test_a_residual_is_the_colour_against_the_white_beside_it(self):
        round_ = self.round_by_id("dark-0.07-lux-drift-controlled")
        blue = [o for o in round_.colours if o.hue == 240][0]
        self.assertEqual(blue.sequence, 5)
        self.assertAlmostEqual(fl.residual_of(round_, blue),
                               blue.log_output - fl.white_at(round_, 5), 12)
        # Blue emits far less than white at the same setting, and the person
        # did not make up all of it: the residual is negative.
        self.assertLess(fl.residual_of(round_, blue), 0.0)

    def test_the_undeclared_round_is_normalised_by_written_order(self):
        round_ = self.round_by_id(
            [r.round_id for r in parsed().rounds if not r.declared][0])
        first, last = round_.anchors[0], round_.anchors[-1]
        self.assertEqual((first.percent, last.percent), (40, 45))
        self.assertEqual(fl.white_at(round_, first.index), first.log_output)
        self.assertEqual(fl.white_at(round_, last.index), last.log_output)


def profile():
    return fl.build_profile(parsed(), drive=drive())


class ProfileTest(unittest.TestCase):
    """The document: what it identifies itself as, and what it separates."""

    def test_it_names_its_schema_its_model_and_its_stack(self):
        built = profile()
        self.assertEqual(built["schemaVersion"], fl.SCHEMA_VERSION)
        self.assertEqual(built["modelId"], fl.MODEL_ID)
        self.assertEqual(built["stackId"], "current-diffuser-before-mask")
        self.assertTrue(built["profileId"])

    def test_the_ambient_axis_is_the_measured_levels_in_log_lux(self):
        built = profile()
        self.assertEqual([level["lux"] for level in built["levels"]],
                         [0.02, 0.07, 0.15, 0.5, 1.9, 10.0])
        xs = [level["logLux"] for level in built["levels"]]
        self.assertEqual(xs, sorted(xs))
        self.assertAlmostEqual(built["logLuxRange"]["min"], xs[0], 12)
        self.assertAlmostEqual(built["logLuxRange"]["max"], xs[-1], 12)

    def test_every_level_carries_a_white_baseline_and_its_anchors(self):
        for level in profile()["levels"]:
            self.assertIn("whiteLogOutput", level)
            self.assertTrue(level["whiteAnchors"])
            for anchor in level["whiteAnchors"]:
                self.assertEqual(anchor["sat"], 0)

    def test_the_hue_axis_is_the_six_knots_and_says_it_is_cyclic(self):
        built = profile()
        self.assertEqual(built["hueKnots"], list(fl.HUE_KNOTS))
        self.assertTrue(built["hueCyclic"])
        self.assertEqual(built["huePeriodDegrees"], 360)
        for level in built["levels"]:
            self.assertEqual([r["hue"] for r in level["residuals"]],
                             list(fl.HUE_KNOTS))

    def test_censored_residuals_are_lower_bounds_and_exact_ones_are_not(self):
        built = profile()
        bright = [l for l in built["levels"] if l["lux"] == 10.0][0]
        bound = {r["hue"]: r["bound"] for r in bright["residuals"]}
        # Four hues ran out of slider at 10 lx; two did not.
        self.assertEqual(bound, {0: None, 60: "lower", 120: None,
                                 180: "lower", 240: "lower", 300: "lower"})
        for level in built["levels"]:
            if level["lux"] == 10.0:
                continue
            for residual in level["residuals"]:
                self.assertIsNone(residual["bound"], level["lux"])

    def test_a_residual_carries_the_observation_behind_it(self):
        level = [l for l in profile()["levels"] if l["lux"] == 0.07][0]
        blue = [r for r in level["residuals"] if r["hue"] == 240][0]
        self.assertEqual(blue["percent"], 70)
        self.assertEqual(blue["sequence"], 5)
        self.assertEqual(blue["index"], 21)

    def test_the_saturation_fade_is_declared_rather_than_implied(self):
        fade = profile()["satFade"]
        self.assertEqual(fade["kind"], "linear")
        self.assertEqual(fade["fullAtSat"], 100)
        self.assertEqual(fade["zeroAtSat"], 0)
        # It is a starting point, not a measurement: every colour point in the
        # record is at sat=100, so nothing here identifies the shape between.
        self.assertTrue(fade["note"])
        self.assertFalse(fade["measured"])

    def test_the_percent_range_travels_with_the_profile(self):
        self.assertEqual(profile()["percentRange"], {"min": 20, "max": 100})
        narrow = fl.build_profile(parsed(), drive=drive(), percent_range=(30, 90))
        self.assertEqual(narrow["percentRange"], {"min": 30, "max": 90})

    def test_the_excluded_round_says_what_supersedes_it(self):
        built = profile()
        self.assertEqual([e["index"] for e in built["source"]["excluded"]],
                         [0, 1, 2, 3, 4, 5, 6, 7])
        self.assertIn("mid-1.9-lux-drift-controlled", built["source"]["supersededBy"])
        self.assertTrue(built["source"]["exclusionNote"])

    def test_the_provenance_names_both_inputs_and_the_hardware_model(self):
        source = profile()["source"]
        self.assertEqual(source["stackId"], "current-diffuser-before-mask")
        self.assertTrue(source["measurementSource"])
        self.assertEqual(source["photopicWeights"], list(cl.PHOTOPIC_WEIGHTS))
        self.assertFalse(source["driveAssumed"])
        self.assertEqual([r["roundId"] for r in source["rounds"]],
                         [l["roundId"] for l in profile()["levels"]])


class ChecksumTest(unittest.TestCase):
    """A profile that cannot say whether it is the one that was measured."""

    def test_it_is_a_sha256_over_the_payload_without_itself(self):
        built = profile()
        self.assertEqual(built["checksum"]["algorithm"], "sha256")
        self.assertEqual(len(built["checksum"]["value"]), 64)
        self.assertEqual(fl.profile_checksum(built), built["checksum"]["value"])

    def test_two_builds_of_the_same_inputs_agree_byte_for_byte(self):
        self.assertEqual(fl.canonical_bytes(profile()),
                         fl.canonical_bytes(profile()))
        self.assertEqual(json.dumps(profile(), sort_keys=True),
                         json.dumps(profile(), sort_keys=True))

    def test_moving_one_measured_percent_moves_the_checksum(self):
        doc = measurements_doc()
        doc["points"][21]["percent"] = 71
        doc["points"][21].pop("relativePhotopicOutput")
        moved = fl.build_profile(fl.load_measurements(doc, drive=drive()),
                                 drive=drive())
        self.assertNotEqual(moved["checksum"]["value"],
                            profile()["checksum"]["value"])

    def test_the_checksum_covers_the_range_and_the_fade_too(self):
        # Not only the grid: two profiles with the same numbers regulated over
        # different ranges are different profiles.
        self.assertNotEqual(
            fl.build_profile(parsed(), drive=drive(),
                             percent_range=(30, 90))["checksum"]["value"],
            profile()["checksum"]["value"])

    def test_a_tampered_profile_no_longer_verifies(self):
        built = profile()
        self.assertTrue(fl.verify_profile(built))
        built["levels"][0]["whiteLogOutput"] += 0.001
        self.assertFalse(fl.verify_profile(built))

    def test_the_checksum_survives_a_reordering_of_the_json(self):
        # Canonical means sorted keys, so a file rewritten by another tool
        # still verifies. Only the values are the profile.
        built = profile()
        round_tripped = json.loads(json.dumps(built, sort_keys=True))
        self.assertTrue(fl.verify_profile(round_tripped))


class EvaluationTest(unittest.TestCase):
    """Between the knots, and at the exact edges where the rules are stated."""

    def setUp(self):
        self.profile = profile()

    def at(self, lux, hue, sat):
        return fl.evaluate(self.profile, lux, hue, sat)

    def level(self, lux):
        return [l for l in self.profile["levels"] if l["lux"] == lux][0]

    def residual(self, lux, hue):
        return [r for r in self.level(lux)["residuals"]
                if r["hue"] == hue][0]["logOutputResidual"]

    # --- saturation, at both ends exactly ---------------------------------

    def test_at_sat_zero_the_colour_residual_is_exactly_zero(self):
        for hue in (0, 37, 120, 240, 299, 359):
            found = self.at(0.5, hue, 0)
            self.assertEqual(found.residual, 0.0, hue)
            self.assertEqual(found.target, self.level(0.5)["whiteLogOutput"])

    def test_at_sat_zero_every_hue_gives_the_same_answer(self):
        answers = {self.at(0.5, hue, 0).percent for hue in range(0, 360, 17)}
        self.assertEqual(len(answers), 1)

    def test_at_sat_a_hundred_the_residual_is_taken_whole(self):
        found = self.at(0.5, 240, 100)
        self.assertAlmostEqual(found.residual, self.residual(0.5, 240), 12)
        self.assertAlmostEqual(
            found.target, self.level(0.5)["whiteLogOutput"] + found.residual, 12)

    def test_the_fade_is_linear_between_the_two_ends(self):
        whole = self.at(0.5, 240, 100).residual
        for sat in (0, 25, 50, 75, 100):
            self.assertAlmostEqual(self.at(0.5, 240, sat).residual,
                                   whole * sat / 100.0, 12)

    # --- hue, including the wrap the grid has no knot for ------------------

    def test_a_knot_hue_reads_that_knot_exactly(self):
        for hue in fl.HUE_KNOTS:
            self.assertAlmostEqual(self.at(0.5, hue, 100).residual,
                                   self.residual(0.5, hue), 12)

    def test_between_two_knots_it_is_linear(self):
        low, high = self.residual(0.5, 60), self.residual(0.5, 120)
        self.assertAlmostEqual(self.at(0.5, 90, 100).residual,
                               (low + high) / 2.0, 12)
        self.assertAlmostEqual(self.at(0.5, 80, 100).residual,
                               low + (high - low) * (20 / 60.0), 12)

    def test_the_wrap_from_300_back_to_0_is_a_segment_like_any_other(self):
        # The one segment with no knot at its far end in the stored order.
        # Getting this wrong shows up as violet snapping to red at 359.
        last, first = self.residual(0.5, 300), self.residual(0.5, 0)
        self.assertAlmostEqual(self.at(0.5, 330, 100).residual,
                               (last + first) / 2.0, 12)
        self.assertAlmostEqual(self.at(0.5, 359, 100).residual,
                               last + (first - last) * (59 / 60.0), 12)

    def test_the_wrap_is_continuous_at_the_seam(self):
        just_under = self.at(0.5, 359, 100).residual
        at_zero = self.at(0.5, 0, 100).residual
        self.assertLess(abs(just_under - at_zero),
                        abs(self.residual(0.5, 300) - at_zero) / 10.0)

    def test_a_hue_outside_the_circle_is_refused(self):
        with self.assertRaises(fl.MeasurementError):
            self.at(0.5, 360, 100)
        with self.assertRaises(fl.MeasurementError):
            self.at(0.5, -1, 100)

    # --- ambient, and the clamp -------------------------------------------

    def test_at_a_measured_level_the_white_baseline_is_that_level(self):
        for level in self.profile["levels"]:
            self.assertAlmostEqual(self.at(level["lux"], 0, 0).white,
                                   level["whiteLogOutput"], 12)

    def test_between_levels_the_white_baseline_is_linear_in_log_lux(self):
        low, high = self.level(0.5), self.level(1.9)
        middle = 10.0 ** ((low["logLux"] + high["logLux"]) / 2.0)
        self.assertAlmostEqual(
            self.at(middle, 0, 0).white,
            (low["whiteLogOutput"] + high["whiteLogOutput"]) / 2.0, 9)

    def test_below_the_measured_range_it_clamps_and_says_so(self):
        found = self.at(0.001, 0, 0)
        self.assertEqual(found.lux_clamped, "below")
        self.assertEqual(found.percent, self.at(0.02, 0, 0).percent)
        self.assertAlmostEqual(found.white, self.level(0.02)["whiteLogOutput"], 12)

    def test_above_the_measured_range_it_clamps_and_says_so(self):
        found = self.at(1000.0, 0, 0)
        self.assertEqual(found.lux_clamped, "above")
        self.assertAlmostEqual(found.white, self.level(10.0)["whiteLogOutput"], 12)

    def test_exactly_at_the_ends_nothing_is_clamped(self):
        self.assertIsNone(self.at(0.02, 0, 0).lux_clamped)
        self.assertIsNone(self.at(10.0, 0, 0).lux_clamped)

    # --- what the answer rests on -----------------------------------------

    def test_a_prediction_leaning_on_a_censored_corner_is_marked(self):
        # The 10 lx round is where the slider ran out. Anything interpolated
        # against it inherits "at least", and says so.
        self.assertEqual(self.at(10.0, 240, 100).bound, "lower")
        self.assertEqual(self.at(5.0, 240, 100).bound, "lower")
        # 0 and 120 were exact even at 10 lx.
        self.assertIsNone(self.at(10.0, 0, 100).bound)
        # Well below the censored level, nothing is borrowed from it.
        self.assertIsNone(self.at(0.5, 240, 100).bound)

    def test_saturation_zero_never_inherits_a_colour_bound(self):
        # The residual contributes nothing, so a censored colour corner is not
        # part of the answer and must not be reported as if it were.
        self.assertIsNone(self.at(10.0, 240, 0).bound)

    def test_a_measured_corner_is_attainable_by_construction(self):
        # Its target *is* what that colour emitted at the percentage somebody
        # chose, so the inverse finds it again. Blue at 10 lx is a lower bound
        # and still not "limited": the two say different things and a caller
        # has to be able to tell them apart.
        found = self.at(10.0, 240, 100)
        self.assertIsNone(found.limited)
        self.assertTrue(found.attainable)
        self.assertEqual(found.bound, "lower")

    def test_an_unattainable_target_reports_the_ceiling(self):
        # Hue 225 sits three quarters of the way from the 180 knot to the 240
        # one, so its target is blended from two colours that are not it - and
        # at 5 lx that target is more light than hue 225 emits at any
        # percentage in the range. That is the gamut, not a fault.
        found = self.at(5.0, 225, 100)
        self.assertEqual(found.limited, "ceiling")
        self.assertFalse(found.attainable)
        self.assertEqual(found.percent, self.profile["percentRange"]["max"])
        self.assertLess(found.log_output, found.target)

    def test_a_reachable_target_is_not_reported_as_limited(self):
        found = self.at(0.07, 0, 100)
        self.assertIsNone(found.limited)
        self.assertTrue(found.attainable)

    def test_the_answer_stays_inside_the_profile_range(self):
        for lux in (0.001, 0.02, 0.3, 2.0, 10.0, 500.0):
            for hue in range(0, 360, 30):
                for sat in (0, 50, 100):
                    found = self.at(lux, hue, sat)
                    self.assertGreaterEqual(found.percent,
                                            self.profile["percentRange"]["min"])
                    self.assertLessEqual(found.percent,
                                         self.profile["percentRange"]["max"])

    def test_a_brighter_room_never_asks_for_a_dimmer_clock(self):
        # Every hue, not a chosen few: the knots, the midpoints between them,
        # and the wrap. A brighter room asking for a dimmer clock is the one
        # thing this curve may never do, and `Luminance.cpp` already refuses a
        # fitted slope of zero or less for the same reason.
        hues = sorted(set(list(fl.HUE_KNOTS) + list(range(0, 360, 7))))
        for hue in hues:
            for sat in (0, 30, 60, 100):
                previous = 0
                for lux in (0.001, 0.01, 0.02, 0.07, 0.15, 0.3, 0.5, 1.0,
                            1.9, 5.0, 10.0, 100.0):
                    percent = self.at(lux, hue, sat).percent
                    self.assertGreaterEqual(percent, previous, (hue, sat, lux))
                    previous = percent

    def test_the_target_itself_is_non_decreasing_in_ambient_light(self):
        # The percentage is quantised, so it can be flat where the target
        # moves. The target is the coordinate the model is actually in, and it
        # is the one the guarantee has to hold in.
        for hue in range(0, 360, 11):
            for sat in (0, 45, 100):
                previous = float("-inf")
                for lux in (0.001, 0.02, 0.09, 0.15, 0.4, 0.5, 1.9, 6.0,
                            10.0, 80.0):
                    target = self.at(lux, hue, sat).target
                    self.assertGreaterEqual(target + 1e-12, previous,
                                            (hue, sat, lux))
                    previous = target

    def test_the_measured_knots_are_non_decreasing_across_the_grid(self):
        # Read straight off the stored grid rather than through the evaluator,
        # so a monotone answer cannot come from the interpolation smoothing
        # over a grid that is not.
        levels = self.profile["levels"]
        whites = [level["whiteLogOutput"] for level in levels]
        self.assertEqual(whites, sorted(whites))
        for position, hue in enumerate(fl.HUE_KNOTS):
            targets = [level["residuals"][position]["targetLogOutput"]
                       for level in levels]
            self.assertEqual(targets, sorted(targets), hue)

    def test_it_reads_the_hardware_model_out_of_the_profile(self):
        # No coupling file passed: the drive table and the weights travel in
        # the profile, so a profile is enough to evaluate one.
        self.assertEqual(fl.evaluate(self.profile, 0.5, 240, 100).percent,
                         fl.evaluate(self.profile, 0.5, 240, 100,
                                     drive=drive()).percent)

    def test_a_profile_that_does_not_verify_is_refused(self):
        broken = copy.deepcopy(self.profile)
        broken["levels"][0]["whiteLogOutput"] += 1.0
        with self.assertRaises(fl.MeasurementError) as caught:
            fl.evaluate(broken, 0.5, 0, 100)
        self.assertIn("checksum", str(caught.exception))

    def test_a_profile_from_another_schema_is_refused(self):
        broken = copy.deepcopy(self.profile)
        broken["schemaVersion"] = fl.SCHEMA_VERSION + 1
        broken["checksum"]["value"] = fl.profile_checksum(broken)
        with self.assertRaises(fl.MeasurementError) as caught:
            fl.evaluate(broken, 0.5, 0, 100)
        self.assertIn("schema", str(caught.exception))

    def test_a_profile_from_another_model_is_refused(self):
        broken = copy.deepcopy(self.profile)
        broken["modelId"] = "something-else"
        broken["checksum"]["value"] = fl.profile_checksum(broken)
        with self.assertRaises(fl.MeasurementError):
            fl.evaluate(broken, 0.5, 0, 100)


class IsotonicTest(unittest.TestCase):
    """The projection itself: least squares onto non-decreasing, and no more."""

    def test_an_already_monotone_series_is_returned_unchanged(self):
        values = [-2.0, -1.5, -1.5, -0.4, 0.9]
        self.assertEqual(fl.isotonic(values, [1.0] * len(values)), values)

    def test_one_dip_is_pooled_into_the_weighted_mean(self):
        # The least-squares monotone answer to "these two disagree" is that
        # they are the same, at the mean - not that one of them wins.
        self.assertEqual(fl.isotonic([0.0, 1.0, 0.0, 2.0], [1.0] * 4),
                         [0.0, 0.5, 0.5, 2.0])

    def test_weights_decide_where_a_pooled_block_lands(self):
        # Four observations against two: the block sits nearer the four.
        pooled = fl.isotonic([1.0, 0.0], [2.0, 4.0])
        self.assertAlmostEqual(pooled[0], (2.0 * 1.0 + 4.0 * 0.0) / 6.0, 12)
        self.assertEqual(pooled[0], pooled[1])

    def test_a_long_run_of_violations_pools_into_one_block(self):
        self.assertEqual(fl.isotonic([3.0, 2.0, 1.0], [1.0] * 3),
                         [2.0, 2.0, 2.0])

    def test_it_is_the_nearest_monotone_series_there_is(self):
        # The defining property, checked against a search rather than
        # restated: no monotone series built from the same values is closer.
        values = [0.0, 1.0, 0.2, 0.9, 0.4]
        weights = [1.0, 1.0, 1.0, 1.0, 1.0]
        projected = fl.isotonic(values, weights)
        best = sum(w * (p - v) ** 2
                   for v, w, p in zip(values, weights, projected))
        step = 0.05
        for a in range(0, 21):
            for b in range(a, 21):
                for c in range(b, 21):
                    for e in range(c, 21):
                        for f in range(e, 21):
                            candidate = [a * step, b * step, c * step,
                                         e * step, f * step]
                            cost = sum(w * (p - v) ** 2 for v, w, p
                                       in zip(values, weights, candidate))
                            self.assertGreaterEqual(cost, best - 1e-9)

    def test_it_refuses_a_series_and_weights_of_different_lengths(self):
        with self.assertRaises(fl.MeasurementError):
            fl.isotonic([1.0, 2.0], [1.0])


class MonotonicProjectionTest(unittest.TestCase):
    """What the projection does to this record, and what it leaves alone."""

    def setUp(self):
        self.profile = profile()

    def level(self, lux):
        return [l for l in self.profile["levels"] if l["lux"] == lux][0]

    def test_the_profile_declares_how_it_was_made_monotone(self):
        how = self.profile["monotonic"]
        self.assertEqual(how["method"], "weighted-isotonic-pava")
        self.assertEqual(how["coordinate"], "targetLogOutput")
        self.assertEqual(how["censoredLoss"], "one-sided-lower-bound")
        self.assertTrue(how["note"])

    def test_the_observed_values_are_kept_beside_the_projected_ones(self):
        # The measurement is not overwritten. A reader can recover exactly
        # what was seen, and exactly what the projection did to it.
        for level in self.profile["levels"]:
            self.assertIn("whiteLogOutputObserved", level)
            for residual in level["residuals"]:
                self.assertIn("targetLogOutputObserved", residual)
                self.assertIn("logOutputResidualObserved", residual)
                self.assertAlmostEqual(
                    residual["adjustmentDecades"],
                    residual["targetLogOutput"]
                    - residual["targetLogOutputObserved"], 9)

    def test_the_untouched_hues_really_are_untouched(self):
        # 0, 120 and 300 were already non-decreasing, so the least-squares
        # projection is the identity on them. A construction that moved them
        # would be doing something other than enforcing monotonicity.
        for position, hue in enumerate(fl.HUE_KNOTS):
            for level in self.profile["levels"]:
                residual = level["residuals"][position]
                if hue in (0, 120, 300):
                    self.assertEqual(residual["adjustmentDecades"], 0.0,
                                     (hue, level["lux"]))
                    self.assertFalse(residual["adjusted"])

    def test_only_the_contradicting_segment_moved(self):
        moved = set()
        for level in self.profile["levels"]:
            if level["whiteLogOutput"] != level["whiteLogOutputObserved"]:
                moved.add(level["lux"])
            for residual in level["residuals"]:
                if residual["adjusted"]:
                    moved.add(level["lux"])
        # 0.15 and 0.5 contradict each other; blue drags 1.9 into its block.
        self.assertEqual(moved, {0.15, 0.5, 1.9})

    def test_the_white_block_lands_on_the_anchor_weighted_mean(self):
        # 0.15 lx rests on two anchors and 0.5 lx on four, so the pooled value
        # sits twice as near the steady round as the drifting one.
        low, high = self.level(0.15), self.level(0.5)
        self.assertEqual(low["whiteLogOutput"], high["whiteLogOutput"])
        expected = (2 * low["whiteLogOutputObserved"]
                    + 4 * high["whiteLogOutputObserved"]) / 6.0
        self.assertAlmostEqual(low["whiteLogOutput"], expected, 9)

    def test_nothing_moved_by_more_than_a_tenth_of_a_decade(self):
        biggest = max(abs(r["adjustmentDecades"])
                      for l in self.profile["levels"] for r in l["residuals"])
        self.assertLess(biggest, 0.1)

    def test_no_censored_observation_is_pulled_below_its_lower_bound(self):
        # A censored point says "at least this much". A projection that
        # lowered one would be asserting the observer wanted less than they
        # demonstrably asked for.
        for level in self.profile["levels"]:
            for residual in level["residuals"]:
                if residual["bound"] == "lower":
                    self.assertGreaterEqual(
                        residual["targetLogOutput"] + 1e-12,
                        residual["targetLogOutputObserved"], level["lux"])

    def test_the_censored_flags_survive_the_projection(self):
        bright = self.level(10.0)
        self.assertEqual(
            {r["hue"]: r["bound"] for r in bright["residuals"]},
            {0: None, 60: "lower", 120: None, 180: "lower",
             240: "lower", 300: "lower"})

    def test_the_source_contradiction_is_still_reported(self):
        # The projection must not make the record look clean. The diagnostics
        # are computed from what was observed, not from what was stored.
        diagnostics = self.profile["diagnostics"]
        self.assertFalse(diagnostics["monotone"])
        self.assertEqual([(d["fromLux"], d["toLux"])
                          for d in diagnostics["nonMonotoneWhite"]],
                         [(0.15, 0.5)])
        self.assertEqual(sorted({d["hue"] for d in diagnostics["nonMonotoneHues"]}),
                         [60, 180, 240])
        for entry in diagnostics["nonMonotoneHues"]:
            self.assertEqual((entry["fromLux"], entry["toLux"]), (0.15, 0.5))
        self.assertTrue(diagnostics["note"])

    def test_pooling_only_ever_lowers_a_point_that_a_later_one_undercuts(self):
        # Which is why no censored point in this record is at risk: all four
        # are at the brightest level, and nothing comes after them.
        self.assertEqual(fl.isotonic([0.0, 1.0, 0.5], [1, 1, 1]),
                         [0.0, 0.75, 0.75])          # the dip is raised
        self.assertEqual(fl.isotonic([0.0, 1.0, 0.2], [1, 1, 1]),
                         [0.0, 0.6, 0.6])            # the peak is lowered

    def test_a_record_whose_censored_point_cannot_be_honoured_is_refused(self):
        # A lower bound at an *interior* level, sitting above the brighter
        # level after it: blue pinned at the ceiling in a 0.5 lx room while
        # the 1.9 lx round asks for 70 %. Making that non-decreasing means
        # pulling the bound down, and a bound cannot be averaged away.
        doc = measurements_doc()
        for point in doc["points"]:
            if (point.get("roundId") == "mid-0.5-lux-drift-controlled"
                    and point["hue"] == 240 and point["sat"] == 100):
                point["percent"] = 100
                point["censored"] = True
                point.pop("relativePhotopicOutput", None)
        with self.assertRaises(fl.MeasurementError) as caught:
            fl.build_profile(fl.load_measurements(doc, drive=drive()),
                             drive=drive())
        message = str(caught.exception)
        self.assertIn("hue 240", message)
        self.assertIn("0.5 lx", message)
        self.assertIn("censored", message)

    def test_the_same_record_without_the_bound_is_merely_pooled(self):
        # The identical numbers, not flagged as a bound and not at the
        # ceiling, are an ordinary contradiction and get projected. What makes
        # the case above a refusal is the censoring, not the shape.
        doc = measurements_doc()
        for point in doc["points"]:
            if (point.get("roundId") == "mid-0.5-lux-drift-controlled"
                    and point["hue"] == 240 and point["sat"] == 100):
                point["percent"] = 99
                point.pop("relativePhotopicOutput", None)
        built = fl.build_profile(fl.load_measurements(doc, drive=drive()),
                                 drive=drive())
        blue = [[r for r in l["residuals"] if r["hue"] == 240][0]
                for l in built["levels"]]
        targets = [r["targetLogOutput"] for r in blue]
        self.assertEqual(targets, sorted(targets))


def crossvalidation():
    return fl.cross_validate(parsed(), drive=drive())


class CrossValidationTest(unittest.TestCase):
    """Leave one clean interior level out, and say what that costs."""

    def setUp(self):
        self.report = crossvalidation()

    def scored(self):
        return [f for f in self.report["folds"] if f["scored"]]

    def test_the_folds_are_the_clean_interior_levels(self):
        # 0.02 lx is the darkest and 10 lx the brightest, so holding either
        # out leaves every prediction clamped against the range - which
        # measures the clamp, not the model. 10 lx is censored as well.
        self.assertEqual([f["lux"] for f in self.scored()],
                         [0.07, 0.15, 0.5, 1.9])
        unscored = [f for f in self.report["folds"] if not f["scored"]]
        self.assertEqual([f["lux"] for f in unscored], [0.02, 10.0])
        for fold in unscored:
            self.assertTrue(fold["reason"])

    def test_the_unscored_endpoint_folds_are_still_reported(self):
        # Reported and labelled, not dropped: their large errors are the
        # evidence for why clamping is what the profile does outside its range.
        bright = [f for f in self.report["folds"] if f["lux"] == 10.0][0]
        self.assertTrue(bright["rows"])
        for row in bright["rows"]:
            self.assertEqual(row["luxClamped"], "above")

    def test_a_held_out_level_is_not_in_the_profile_that_predicts_it(self):
        for fold in self.scored():
            self.assertNotIn(fold["lux"], fold["trainedOnLux"])
            self.assertEqual(len(fold["trainedOnLux"]), 5)

    def test_every_scored_row_is_an_uncensored_observation(self):
        for fold in self.scored():
            for row in fold["rows"]:
                if row["scored"]:
                    self.assertFalse(row["censored"])

    def test_censored_observations_are_checked_as_inequalities(self):
        # "at least 100 %" is honoured by predicting 100 or more, and is not
        # an equality anybody may take a residual from.
        check = self.report["inSample"]["censored"]
        self.assertEqual(check["count"], 4)
        self.assertEqual(check["honoured"], 4)
        self.assertEqual(check["violations"], [])
        for entry in check["rows"]:
            self.assertGreaterEqual(entry["predicted"], entry["atLeast"])

    def test_the_acceptance_block_states_both_goals_and_both_verdicts(self):
        acceptance = self.report["acceptance"]
        self.assertEqual(acceptance["goalRms"], 6.0)
        self.assertEqual(acceptance["goalMax"], 10.0)
        self.assertEqual(acceptance["met"],
                         acceptance["rmsMet"] and acceptance["maxMet"])
        self.assertEqual(acceptance["count"], 24)
        self.assertEqual(acceptance["folds"], 4)

    def test_the_rms_goal_is_met_on_this_record(self):
        acceptance = self.report["acceptance"]
        self.assertLessEqual(acceptance["rms"], 6.0)
        self.assertTrue(acceptance["rmsMet"])

    def test_the_maximum_goal_is_not_met_and_the_report_says_so(self):
        # Recorded rather than argued away. If another sitting fixes the blue
        # contradiction this test is what will notice.
        acceptance = self.report["acceptance"]
        self.assertGreater(acceptance["max"], 10.0)
        self.assertFalse(acceptance["maxMet"])
        self.assertFalse(acceptance["met"])
        self.assertTrue(acceptance["note"])

    def test_the_report_names_the_hue_responsible(self):
        worst = max(self.report["byHue"], key=lambda h: h["max"])
        self.assertEqual(worst["hue"], 240)
        without = self.report["worstHueExcluded"]
        self.assertEqual(without["hue"], 240)
        # Every other hue meets both goals; the failure is one colour's
        # observations contradicting each other, not the model's shape.
        self.assertLessEqual(without["rms"], 6.0)
        self.assertLessEqual(without["max"], 10.0)

    def test_the_numbers_on_the_current_fixture(self):
        # Pinned so a change in the data or the construction is visible rather
        # than silently absorbed.
        acceptance = self.report["acceptance"]
        self.assertAlmostEqual(acceptance["rms"], 5.4083, 3)
        self.assertEqual(acceptance["max"], 15)
        self.assertAlmostEqual(self.report["inSample"]["rms"], 1.8371, 3)
        self.assertEqual(self.report["inSample"]["max"], 7)

    def test_it_is_deterministic(self):
        self.assertEqual(json.dumps(crossvalidation(), sort_keys=True),
                         json.dumps(crossvalidation(), sort_keys=True))

    def test_the_limitations_are_carried_in_the_report(self):
        self.assertTrue(self.report["limitations"])
        for entry in self.report["limitations"]:
            self.assertTrue(entry.strip())


class CommandLineTest(unittest.TestCase):
    """Two files in, three deterministic files out, and no traceback."""

    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.dir)
        self.profile_path = os.path.join(self.dir, "profile.json")
        self.report_path = os.path.join(self.dir, "report.json")
        self.csv_path = os.path.join(self.dir, "report.csv")

    def build(self, *extra):
        argv = ["build",
                "--measurements", MEASUREMENTS_PATH,
                "--coupling", COUPLING_PATH,
                "--profile", self.profile_path,
                "--report", self.report_path,
                "--csv", self.csv_path] + list(extra)
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = fl.main(argv)
        return code, out.getvalue(), err.getvalue()

    def refused(self, argv):
        """argparse's own failure: exit 2, one line on stderr, no traceback."""
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            with self.assertRaises(SystemExit) as caught:
                fl.main(argv)
        self.assertEqual(caught.exception.code, 2)
        self.assertNotIn("Traceback", err.getvalue())
        self.assertEqual(out.getvalue(), "")
        return err.getvalue()

    def read(self, path):
        with open(path, "r") as handle:
            return handle.read()

    def test_a_build_writes_a_profile_a_report_and_a_csv(self):
        code, out, err = self.build()
        self.assertEqual(code, 0)
        for path in (self.profile_path, self.report_path, self.csv_path):
            self.assertTrue(os.path.exists(path), path)
        self.assertIn("RMS", out)

    def test_the_profile_it_writes_verifies_when_read_back(self):
        self.build()
        written = json.loads(self.read(self.profile_path))
        self.assertTrue(fl.verify_profile(written))
        self.assertEqual(written["modelId"], fl.MODEL_ID)

    def test_the_profile_it_writes_can_be_evaluated_without_the_coupling(self):
        self.build()
        written = json.loads(self.read(self.profile_path))
        self.assertEqual(fl.evaluate(written, 0.5, 240, 100).percent,
                         fl.evaluate(profile(), 0.5, 240, 100).percent)

    def test_two_runs_produce_byte_identical_files(self):
        self.build()
        first = [self.read(p) for p in (self.profile_path, self.report_path,
                                        self.csv_path)]
        os.remove(self.profile_path)
        self.build()
        second = [self.read(p) for p in (self.profile_path, self.report_path,
                                         self.csv_path)]
        self.assertEqual(first, second)

    def test_the_csv_has_a_header_and_one_row_per_observation(self):
        self.build()
        rows = list(csv.DictReader(io.StringIO(self.read(self.csv_path))))
        self.assertEqual(list(rows[0].keys()), fl.CSV_COLUMNS)
        # Six folds of six colours, plus the in-sample pass over the same 36.
        self.assertEqual(len(rows), 72)
        self.assertEqual({row["kind"] for row in rows}, {"fold", "inSample"})

    def test_the_csv_reproduces_the_headline_count_in_one_column(self):
        # 36 held-out rows: 4 are censored bounds and 12 sit in the two
        # unscored endpoint folds, leaving the 24 the acceptance figure is
        # taken over. All three facts are separate columns, because needing to
        # AND two of them is how that number gets miscounted.
        self.build()
        rows = list(csv.DictReader(io.StringIO(self.read(self.csv_path))))
        folds = [r for r in rows if r["kind"] == "fold"]
        self.assertEqual(len(folds), 36)
        counted = [r for r in folds if r["countedInAcceptance"] == "1"]
        self.assertEqual(len(counted), 24)
        self.assertEqual(len(counted),
                         json.loads(self.read(self.report_path))
                         ["acceptance"]["count"])
        self.assertEqual(sum(1 for r in folds if r["censored"] == "1"), 4)
        self.assertEqual(sum(1 for r in folds if r["foldScored"] == "1"), 24)

    def test_the_unscored_endpoint_rows_are_still_in_the_csv(self):
        self.build()
        rows = list(csv.DictReader(io.StringIO(self.read(self.csv_path))))
        unscored = [r for r in rows
                    if r["kind"] == "fold" and r["foldScored"] == "0"]
        self.assertEqual(len(unscored), 12)
        self.assertEqual({r["lux"] for r in unscored}, {"0.02", "10.0"})
        for row in unscored:
            self.assertEqual(row["countedInAcceptance"], "0")
            self.assertTrue(row["luxClamped"])

    def test_no_in_sample_row_counts_towards_acceptance(self):
        self.build()
        rows = list(csv.DictReader(io.StringIO(self.read(self.csv_path))))
        for row in rows:
            if row["kind"] == "inSample":
                self.assertEqual(row["countedInAcceptance"], "0")
                self.assertEqual(row["foldScored"], "0")

    def test_a_narrower_range_changes_the_profile_it_writes(self):
        self.build()
        wide = self.read(self.profile_path)
        self.build("--percent-range", "30,90")
        self.assertNotEqual(wide, self.read(self.profile_path))
        self.assertEqual(json.loads(self.read(self.profile_path))["percentRange"],
                         {"min": 30, "max": 90})

    # --- what it refuses ---------------------------------------------------

    def test_a_missing_measurement_file(self):
        message = self.refused(["build", "--measurements", "/no/such/file",
                                "--coupling", COUPLING_PATH,
                                "--profile", self.profile_path])
        self.assertIn("/no/such/file", message)

    def test_a_file_that_is_not_json(self):
        broken = os.path.join(self.dir, "broken.json")
        with open(broken, "w") as handle:
            handle.write("{not json")
        message = self.refused(["build", "--measurements", broken,
                                "--coupling", COUPLING_PATH,
                                "--profile", self.profile_path])
        self.assertIn("not valid JSON", message)

    def test_a_measurement_record_that_will_not_parse(self):
        doc = measurements_doc()
        doc["points"][20].pop("hue")
        path = os.path.join(self.dir, "nohue.json")
        with open(path, "w") as handle:
            json.dump(doc, handle)
        message = self.refused(["build", "--measurements", path,
                                "--coupling", COUPLING_PATH,
                                "--profile", self.profile_path])
        self.assertIn("point 20", message)

    def test_the_coupling_record_is_required(self):
        # Without a measured drive response every number would be wrong by up
        # to a factor of three at the settings this clock runs at, so it is
        # not optional and not defaulted.
        self.refused(["build", "--measurements", MEASUREMENTS_PATH,
                      "--profile", self.profile_path])

    def test_a_malformed_percent_range(self):
        message = self.refused(["build", "--measurements", MEASUREMENTS_PATH,
                                "--coupling", COUPLING_PATH,
                                "--profile", self.profile_path,
                                "--percent-range", "wide"])
        self.assertIn("--percent-range", message)

    def test_an_inverted_percent_range(self):
        self.refused(["build", "--measurements", MEASUREMENTS_PATH,
                      "--coupling", COUPLING_PATH,
                      "--profile", self.profile_path,
                      "--percent-range", "90,30"])

    def test_an_unwritable_output_path(self):
        message = self.refused(["build", "--measurements", MEASUREMENTS_PATH,
                                "--coupling", COUPLING_PATH,
                                "--profile",
                                os.path.join(self.dir, "no", "dir", "p.json")])
        self.assertTrue(message.strip())

    def test_no_subcommand_prints_help_rather_than_failing(self):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = fl.main([])
        self.assertEqual(code, 2)
        self.assertIn("build", out.getvalue())


class EvaluateCommandTest(unittest.TestCase):
    """Reading a written profile back and asking it something."""

    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.dir)
        self.path = os.path.join(self.dir, "profile.json")
        with open(self.path, "w") as handle:
            json.dump(profile(), handle)

    def run_it(self, *extra):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = fl.main(["evaluate", "--profile", self.path] + list(extra))
        return code, out.getvalue()

    def test_it_answers_with_the_percentage_and_what_it_rests_on(self):
        code, out = self.run_it("--lux", "0.5", "--hue", "240", "--sat", "100")
        self.assertEqual(code, 0)
        answer = json.loads(out)
        self.assertEqual(answer["percent"],
                         fl.evaluate(profile(), 0.5, 240, 100).percent)
        self.assertIn("targetLogOutput", answer)
        self.assertIn("bound", answer)
        self.assertIn("luxClamped", answer)

    def test_it_refuses_a_profile_that_has_been_edited(self):
        with open(self.path, "r") as handle:
            edited = json.load(handle)
        edited["levels"][0]["whiteLogOutput"] += 0.5
        with open(self.path, "w") as handle:
            json.dump(edited, handle)
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            with self.assertRaises(SystemExit) as caught:
                fl.main(["evaluate", "--profile", self.path, "--lux", "1",
                         "--hue", "0", "--sat", "0"])
        self.assertEqual(caught.exception.code, 2)
        self.assertIn("checksum", err.getvalue())
        self.assertNotIn("Traceback", err.getvalue())


class ReportBindsToProfileTest(unittest.TestCase):
    """The checksum in the report has one job: naming the profile it judged."""

    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.dir)
        self.profile_path = os.path.join(self.dir, "profile.json")
        self.report_path = os.path.join(self.dir, "report.json")

    def build(self, *extra):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = fl.main(["build",
                            "--measurements", MEASUREMENTS_PATH,
                            "--coupling", COUPLING_PATH,
                            "--profile", self.profile_path,
                            "--report", self.report_path] + list(extra))
        self.assertEqual(code, 0, err.getvalue())
        with open(self.profile_path) as handle:
            built = json.load(handle)
        with open(self.report_path) as handle:
            report = json.load(handle)
        return built, report

    def test_the_written_report_names_the_written_profile(self):
        built, report = self.build()
        self.assertEqual(report["profileChecksum"], built["checksum"]["value"])
        self.assertEqual(report["profileId"], built["profileId"])

    def test_it_still_names_it_when_the_id_is_overridden(self):
        # `profileId` and `generator` are inside the checksummed payload, so a
        # report that rebuilt the profile with different ones would quietly
        # describe a document nobody has.
        built, report = self.build("--profile-id", "bench-2026-08-27")
        self.assertEqual(built["profileId"], "bench-2026-08-27")
        self.assertEqual(report["profileId"], "bench-2026-08-27")
        self.assertEqual(report["profileChecksum"], built["checksum"]["value"])

    def test_the_report_is_of_the_range_it_was_asked_for(self):
        built, report = self.build("--percent-range", "30,90")
        self.assertEqual(report["percentRange"], {"min": 30, "max": 90})
        self.assertEqual(report["profileChecksum"], built["checksum"]["value"])

    def test_a_profile_handed_in_with_another_range_is_refused(self):
        other = fl.build_profile(parsed(), drive=drive(), percent_range=(30, 90))
        with self.assertRaises(fl.MeasurementError) as caught:
            fl.cross_validate(parsed(), drive=drive(), percent_range=(20, 100),
                              profile=other)
        self.assertIn("range", str(caught.exception))


# ==========================================================================
# The targeted hue-240 repeat: W-H240 pairs, not six-hue rounds
# ==========================================================================

REPEAT_PATH = os.path.join(
    FIXTURES, "2026-08-27-manual-colour-brightness-points-with-hue240-repeat.json")


def repeat_doc():
    with open(REPEAT_PATH, "r") as handle:
        return json.load(handle)


def repeat_parsed():
    return fl.load_measurements(repeat_doc(), drive=drive())


class TargetedPairParseTest(unittest.TestCase):
    """A pair is two observations in one sitting, and never half a round."""

    def setUp(self):
        self.parsed = repeat_parsed()

    def test_the_six_hue_rounds_are_unchanged_by_the_addition(self):
        self.assertEqual([r.lux for r in self.parsed.rounds],
                         [0.02, 0.07, 0.15, 0.5, 1.9, 10.0])
        for round_ in self.parsed.rounds:
            self.assertEqual(sorted(o.hue for o in round_.colours),
                             sorted(fl.HUE_KNOTS))

    def test_the_pairs_are_parsed_as_their_own_kind(self):
        self.assertEqual([p.lux for p in self.parsed.pairs],
                         [0.02, 0.15, 0.5, 1.9])
        for pair in self.parsed.pairs:
            self.assertEqual(pair.hue, 240)
            self.assertEqual(pair.colour.sat, 100)
            self.assertEqual(pair.anchor.sat, 0)
            self.assertEqual(pair.family, "hue240-targeted-repeat-2026-08-27")

    def test_a_pair_is_not_a_round_and_never_becomes_one(self):
        # The whole failure this guards against is a two-point sitting being
        # read as a round with four hues missing, or worse, filled in.
        for pair in self.parsed.pairs:
            self.assertNotIn(pair.round_id,
                             [r.round_id for r in self.parsed.rounds])
        self.assertEqual(len(self.parsed.rounds), 6)

    def test_the_family_declaration_is_read_and_checked(self):
        family = self.parsed.families["hue240-targeted-repeat-2026-08-27"]
        self.assertEqual(family["hue"], 240)
        self.assertEqual(family["levels"], [0.02, 0.15, 0.5, 1.9])
        self.assertEqual(family["kind"], "targeted-repeat")
        self.assertTrue(family["note"])

    def test_no_colour_is_inferred_for_the_five_hues_not_measured(self):
        # A targeted level contributes one hue. The other five at that level
        # come from the six-hue round and from nowhere else.
        for pair in self.parsed.pairs:
            self.assertEqual(len(pair.observations), 2)
            self.assertEqual({o.hue for o in pair.observations if o.sat == 100},
                             {240})

    def test_the_pair_residual_is_its_own_white_and_nothing_elses(self):
        # Same sitting, adjacent: the difference is the measurement. It does
        # not borrow the six-hue round's white, which is what makes it a
        # replicate of the residual rather than of the level.
        for pair in self.parsed.pairs:
            self.assertAlmostEqual(
                pair.residual,
                pair.colour.log_output - pair.anchor.log_output, 12)

    def test_a_declared_level_with_no_points_is_refused(self):
        doc = repeat_doc()
        doc["points"] = [p for p in doc["points"]
                         if p.get("roundId") != "hue240-targeted-repeat-2026-08-27-0.5"]
        with self.assertRaises(fl.MeasurementError) as caught:
            fl.load_measurements(doc, drive=drive())
        self.assertIn("0.5", str(caught.exception))

    def test_a_pair_carrying_the_wrong_hue_is_refused(self):
        doc = repeat_doc()
        for point in doc["points"]:
            if point.get("roundId", "").startswith("hue240-targeted-repeat") \
                    and point["sat"] == 100 and point["lux"] == 0.15:
                point["hue"] = 120
                point.pop("relativePhotopicOutput", None)
        with self.assertRaises(fl.MeasurementError) as caught:
            fl.load_measurements(doc, drive=drive())
        self.assertIn("240", str(caught.exception))

    def test_a_pair_with_no_white_anchor_is_refused(self):
        doc = repeat_doc()
        doc["points"] = [p for p in doc["points"]
                         if not (p.get("roundId", "").startswith(
                             "hue240-targeted-repeat") and p["sat"] == 0
                             and p["lux"] == 0.02)]
        with self.assertRaises(fl.MeasurementError) as caught:
            fl.load_measurements(doc, drive=drive())
        self.assertIn("anchor", str(caught.exception))

    def test_a_family_declaring_no_hue_is_refused(self):
        doc = repeat_doc()
        for entry in doc["rounds"]:
            if entry.get("kind") == "targeted-repeat":
                entry.pop("hue")
        with self.assertRaises(fl.MeasurementError) as caught:
            fl.load_measurements(doc, drive=drive())
        self.assertIn("hue", str(caught.exception))

    def test_the_old_fixture_has_no_pairs_and_no_families(self):
        old = parsed()
        self.assertEqual(old.pairs, [])
        self.assertEqual(old.families, {})


def repeat_profile(**kwargs):
    return fl.build_profile(repeat_parsed(), drive=drive(), **kwargs)


class TargetedWeightingTest(unittest.TestCase):
    """What a replicate may move, and what it may not."""

    def setUp(self):
        self.old = profile()
        self.new = repeat_profile()

    def level(self, built, lux):
        return [l for l in built["levels"] if l["lux"] == lux][0]

    def residual(self, built, lux, hue):
        return [r for r in self.level(built, lux)["residuals"]
                if r["hue"] == hue][0]

    # --- the white baseline is the cautious part ---------------------------

    def test_a_pair_does_not_move_the_white_baseline(self):
        # Their anchors sit up to 0.56 decades below the six-hue rounds' at
        # the dark levels - a different adaptation state, one anchor deep.
        # Letting that into the global white would move every hue's answer.
        for lux in (0.02, 0.07, 0.15, 0.5, 1.9, 10.0):
            self.assertEqual(
                self.level(self.new, lux)["whiteLogOutputObserved"],
                self.level(self.old, lux)["whiteLogOutputObserved"], lux)

    def test_the_anchors_stored_are_still_only_the_rounds_own(self):
        for lux in (0.02, 0.15):
            self.assertEqual(
                [a["index"] for a in self.level(self.new, lux)["whiteAnchors"]],
                [a["index"] for a in self.level(self.old, lux)["whiteAnchors"]])

    def test_the_profile_states_the_white_decision_and_shows_its_cost(self):
        decision = self.new["targeted"]["whiteBaseline"]
        self.assertEqual(decision["appliedTo"], "residualsOnly")
        self.assertTrue(decision["note"])
        # Reported, not applied: what pooling them *would* have done, so the
        # decision is evidence rather than assertion.
        moves = {entry["lux"]: entry["wouldMoveDecades"]
                 for entry in decision["ifPooled"]}
        self.assertEqual(sorted(moves), [0.02, 0.15, 0.5, 1.9])
        self.assertLess(moves[0.15], -0.1)
        self.assertLess(moves[0.02], -0.05)
        self.assertEqual(moves[1.9], 0.0)

    # --- the residual is the part a pair really measures --------------------

    def test_a_replicated_residual_is_the_mean_of_its_observations(self):
        for pair in repeat_parsed().pairs:
            stored = self.residual(self.new, pair.lux, 240)
            self.assertEqual(stored["replicates"], 2)
            contributions = [c["logOutputResidual"] for c in stored["observations"]]
            self.assertAlmostEqual(
                stored["logOutputResidualObserved"],
                sum(contributions) / 2.0, 9)

    def test_every_contribution_keeps_its_provenance(self):
        stored = self.residual(self.new, 0.15, 240)
        kinds = [c["kind"] for c in stored["observations"]]
        self.assertEqual(sorted(kinds), ["round", "targeted"])
        for contribution in stored["observations"]:
            self.assertIn("roundId", contribution)
            self.assertIn("index", contribution)
            self.assertIn("percent", contribution)

    def test_the_old_observation_is_never_erased(self):
        stored = self.residual(self.new, 0.15, 240)
        original = [c for c in stored["observations"] if c["kind"] == "round"][0]
        self.assertEqual(original["percent"], 80)
        self.assertAlmostEqual(
            original["logOutputResidual"],
            self.residual(self.old, 0.15, 240)["logOutputResidualObserved"], 9)

    def test_hues_without_a_replicate_are_untouched(self):
        for hue in (0, 60, 120, 180, 300):
            for lux in (0.02, 0.15, 0.5, 1.9):
                stored = self.residual(self.new, lux, hue)
                self.assertEqual(stored["replicates"], 1)
                self.assertEqual(
                    stored["logOutputResidualObserved"],
                    self.residual(self.old, lux, hue)["logOutputResidualObserved"],
                    (hue, lux))

    def test_the_level_without_a_repeat_is_untouched_even_at_hue_240(self):
        # 0.07 and 10 lx got no targeted sitting.
        for lux in (0.07, 10.0):
            self.assertEqual(self.residual(self.new, lux, 240)["replicates"], 1)

    def test_the_isotonic_weight_follows_the_replicate_count(self):
        weights = {}
        for level in self.new["levels"]:
            for stored in level["residuals"]:
                weights[(level["lux"], stored["hue"])] = stored["weight"]
        self.assertEqual(weights[(0.15, 240)], 2.0)
        self.assertEqual(weights[(0.15, 0)], 1.0)
        self.assertEqual(weights[(0.07, 240)], 1.0)

    # --- validation-only, for comparison ------------------------------------

    def test_validation_only_leaves_the_grid_exactly_as_it_was(self):
        held = repeat_profile(include_targeted=False)
        for lux in (0.02, 0.15, 0.5, 1.9):
            self.assertEqual(
                self.residual(held, lux, 240)["logOutputResidualObserved"],
                self.residual(self.old, lux, 240)["logOutputResidualObserved"],
                lux)
            self.assertEqual(self.residual(held, lux, 240)["replicates"], 1)
        self.assertEqual(held["targeted"]["appliedToResiduals"], False)
        self.assertEqual(self.new["targeted"]["appliedToResiduals"], True)

    def test_the_two_choices_are_different_profiles(self):
        self.assertNotEqual(repeat_profile(include_targeted=False)["checksum"],
                            self.new["checksum"])

    def test_the_profile_reports_each_pair_against_the_original(self):
        rows = {entry["lux"]: entry for entry in self.new["targeted"]["pairs"]}
        self.assertEqual(sorted(rows), [0.02, 0.15, 0.5, 1.9])
        for lux, entry in rows.items():
            self.assertEqual(entry["hue"], 240)
            self.assertAlmostEqual(
                entry["differenceDecades"],
                entry["targetedResidual"] - entry["roundResidual"], 9)
        # The two sessions agree closely at 0.02 and exactly at 1.9, and
        # disagree most at 0.15 - which is the level the original round could
        # least be trusted at, having drifted 40 to 45 % across it.
        self.assertLess(abs(rows[0.02]["differenceDecades"]), 0.02)
        self.assertEqual(rows[1.9]["differenceDecades"], 0.0)
        self.assertGreater(abs(rows[0.15]["differenceDecades"]), 0.1)

    def test_the_old_fixture_carries_an_empty_targeted_block(self):
        self.assertEqual(self.old["targeted"]["pairs"], [])
        self.assertEqual(self.old["targeted"]["appliedToResiduals"], True)


class TargetedCrossValidationTest(unittest.TestCase):
    """Holding out a level has to hold out everything measured at it."""

    def setUp(self):
        self.report = fl.cross_validate(repeat_parsed(), drive=drive())

    def test_a_fold_trains_on_no_observation_from_its_own_level(self):
        # The leak this guards against is subtle and total: leave the 0.15 lx
        # round out but keep the 0.15 lx targeted pair, and the model is asked
        # to predict hue 240 at a level where it has just been told the
        # answer. The fold would look excellent and mean nothing.
        for fold in self.report["folds"]:
            self.assertNotIn(fold["lux"], fold["trainedOnTargetedLux"])
            self.assertNotIn(fold["lux"], fold["trainedOnLux"])

    def test_the_targeted_pairs_are_named_in_each_fold(self):
        held = [f for f in self.report["folds"] if f["lux"] == 0.15][0]
        self.assertEqual(sorted(held["trainedOnTargetedLux"]),
                         [0.02, 0.5, 1.9])
        self.assertEqual(held["heldOutTargeted"],
                         ["hue240-targeted-repeat-2026-08-27-0.15"])

    def test_a_held_out_pair_is_scored_as_its_own_row(self):
        # It is an observation of hue 240 at that level, so it belongs in the
        # fold's error just as the round's own hue 240 does - and it is
        # labelled, so nobody has to guess which is which.
        held = [f for f in self.report["folds"] if f["lux"] == 0.5][0]
        kinds = sorted(row["kind"] for row in held["rows"])
        self.assertEqual(kinds.count("targeted"), 1)
        self.assertEqual(kinds.count("round"), 6)
        pair_row = [r for r in held["rows"] if r["kind"] == "targeted"][0]
        self.assertEqual(pair_row["hue"], 240)
        self.assertEqual(pair_row["observed"], 65)

    def test_the_report_carries_the_pair_comparison(self):
        rows = {e["lux"]: e for e in self.report["targeted"]["pairs"]}
        self.assertEqual(sorted(rows), [0.02, 0.15, 0.5, 1.9])
        self.assertLess(abs(rows[0.02]["differenceDecades"]), 0.02)

    def test_the_old_fixture_report_has_no_targeted_rows(self):
        old = fl.cross_validate(parsed(), drive=drive())
        for fold in old["folds"]:
            self.assertEqual([r for r in fold["rows"]
                              if r["kind"] == "targeted"], [])
            self.assertEqual(fold["trainedOnTargetedLux"], [])

    def test_it_is_deterministic(self):
        first = json.dumps(fl.cross_validate(repeat_parsed(), drive=drive()),
                           sort_keys=True)
        second = json.dumps(fl.cross_validate(repeat_parsed(), drive=drive()),
                            sort_keys=True)
        self.assertEqual(first, second)


class TargetedAcceptanceTest(unittest.TestCase):
    """What the repeat did to the numbers, which is not what was hoped."""

    def setUp(self):
        self.report = fl.cross_validate(repeat_parsed(), drive=drive())

    def test_the_old_fixture_metrics_are_unchanged(self):
        # The repeat is additive. Nothing about the earlier record moved.
        old = fl.cross_validate(parsed(), drive=drive())
        self.assertAlmostEqual(old["acceptance"]["rms"], 5.4083, 3)
        self.assertEqual(old["acceptance"]["max"], 15)
        self.assertEqual(old["acceptance"]["count"], 24)

    def test_the_repeat_adds_three_scored_rows(self):
        # Four pairs, but the one at 0.02 lx falls in the darkest fold, which
        # is unscored because everything in it clamps.
        self.assertEqual(self.report["acceptance"]["count"], 27)

    def test_hue_240_still_fails_the_maximum_goal(self):
        acceptance = self.report["acceptance"]
        self.assertEqual(acceptance["max"], 15)
        self.assertFalse(acceptance["maxMet"])

    def test_the_rms_goal_now_fails_as_well(self):
        # It passed at 5.41 before the repeat and fails at 6.87 after. The
        # repeat did not resolve the contradiction at hue 240; it measured
        # more of it.
        acceptance = self.report["acceptance"]
        self.assertGreater(acceptance["rms"], 6.0)
        self.assertFalse(acceptance["rmsMet"])
        self.assertFalse(acceptance["met"])

    def test_the_current_numbers_are_pinned(self):
        acceptance = self.report["acceptance"]
        self.assertAlmostEqual(acceptance["rms"], 6.8665, 3)
        self.assertEqual(acceptance["max"], 15)

    def test_every_other_hue_still_meets_both_goals(self):
        others = [h for h in self.report["byHue"] if h["hue"] != 240]
        for entry in others:
            self.assertLessEqual(entry["rms"], 6.0, entry["hue"])
            self.assertLessEqual(entry["max"], 10, entry["hue"])
        without = self.report["worstHueExcluded"]
        self.assertEqual(without["hue"], 240)
        self.assertLessEqual(without["rms"], 6.0)
        self.assertLessEqual(without["max"], 10)

    def test_the_two_sessions_disagree_most_where_the_error_is_worst(self):
        # At 0.15 lx the round says 80 % and the repeat says 55 %. No
        # single-valued model satisfies both, and the fit lands between them,
        # so both rows come back wrong by about twelve points.
        held = [f for f in self.report["folds"] if f["lux"] == 0.15][0]
        blue = sorted((r for r in held["rows"] if r["hue"] == 240),
                      key=lambda r: r["kind"])
        self.assertEqual([r["kind"] for r in blue], ["round", "targeted"])
        self.assertEqual([r["observed"] for r in blue], [80, 55])
        self.assertEqual(len({r["predicted"] for r in blue}), 1)
        self.assertGreater(abs(blue[0]["error"]), 10)
        self.assertGreater(abs(blue[1]["error"]), 10)

    def test_the_report_carries_the_validation_only_sensitivity(self):
        # Both choices are reported so the decision is evidence. Neither meets
        # the goals, so the choice cannot be a threshold being chased.
        sensitivity = self.report["sensitivity"]["targetedAsReplicates"]
        self.assertAlmostEqual(sensitivity["applied"]["rms"], 6.8665, 3)
        self.assertAlmostEqual(sensitivity["validationOnly"]["rms"], 6.6025, 3)
        self.assertEqual(sensitivity["applied"]["max"], 15)
        self.assertEqual(sensitivity["validationOnly"]["max"], 15)
        self.assertFalse(sensitivity["applied"]["met"])
        self.assertFalse(sensitivity["validationOnly"]["met"])
        self.assertTrue(sensitivity["note"])

    def test_the_limitations_name_the_disagreement(self):
        text = " ".join(self.report["limitations"])
        self.assertIn("240", text)
        self.assertIn("0.15", text)


class TargetedArtifactTest(unittest.TestCase):
    """The repeat record goes through the same command, deterministically."""

    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.dir)

    def build(self, tag):
        paths = [os.path.join(self.dir, "%s-%s" % (tag, name))
                 for name in ("profile.json", "report.json", "report.csv")]
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = fl.main(["build", "--measurements", REPEAT_PATH,
                            "--coupling", COUPLING_PATH,
                            "--profile", paths[0], "--report", paths[1],
                            "--csv", paths[2]])
        self.assertEqual(code, 0, err.getvalue())
        return [self.read(path) for path in paths]

    def read(self, path):
        with open(path, "r") as handle:
            return handle.read()

    def test_two_runs_are_byte_identical(self):
        self.assertEqual(self.build("a"), self.build("b"))

    def test_the_written_profile_verifies_and_carries_the_repeat(self):
        text = self.build("a")[0]
        built = json.loads(text)
        self.assertTrue(fl.verify_profile(built))
        self.assertEqual(len(built["targeted"]["pairs"]), 4)
        blue = [r for l in built["levels"] if l["lux"] == 0.15
                for r in l["residuals"] if r["hue"] == 240][0]
        self.assertEqual(blue["replicates"], 2)

    def test_the_report_names_the_profile_it_judged(self):
        profile_text, report_text, _ = self.build("a")
        self.assertEqual(json.loads(report_text)["profileChecksum"],
                         json.loads(profile_text)["checksum"]["value"])

    def test_the_csv_labels_targeted_rows_and_keeps_its_columns(self):
        rows = list(csv.DictReader(io.StringIO(self.build("a")[2])))
        self.assertEqual(list(rows[0].keys()), fl.CSV_COLUMNS)
        folds = [r for r in rows if r["kind"] == "fold"]
        self.assertEqual(sum(1 for r in folds if r["source"] == "targeted"), 4)
        self.assertEqual(sum(1 for r in folds
                             if r["countedInAcceptance"] == "1"), 27)

    def test_the_old_record_still_produces_its_own_numbers(self):
        # Running the earlier fixture through the same command must still give
        # the metrics that were pinned before the repeat existed.
        path = os.path.join(self.dir, "old-report.json")
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            fl.main(["build", "--measurements", MEASUREMENTS_PATH,
                     "--coupling", COUPLING_PATH,
                     "--profile", os.path.join(self.dir, "old-profile.json"),
                     "--report", path])
        acceptance = json.loads(self.read(path))["acceptance"]
        self.assertAlmostEqual(acceptance["rms"], 5.4083, 3)
        self.assertEqual(acceptance["max"], 15)
