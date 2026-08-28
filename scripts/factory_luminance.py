# -*- coding: utf-8 -*-
"""The factory colour profile: build one from measurements, and evaluate it.

`colour_luminance.py` answers "what does this colour at this percentage emit",
exactly as the firmware would. That is a property of the hardware. This module
answers the other half - "how much light does somebody *want* at this ambient
level in this colour" - which is a property of a person in a room, and can only
come from measurements.

The shape is the one the feasibility note settled on rather than a global
analytic surface, because the data do not yet justify one:

    target(x, hue, sat) = white(x) + residual(hue, x) * fade(sat)

`x` is `log10(lux)`. `white(x)` is piecewise linear over the measured ambient
levels. `residual` is a grid, six cyclic hue knots by those same levels, in
**decades of emitted light** relative to white - the unit the one existing
measurement is constant in, where percent is neither an offset nor a factor.
Percent comes back out by inverting the hardware model, which is
`colour_luminance.percent_for_output()` and not a second implementation.

Three things are kept apart everywhere, because conflating them is how a
subjective record turns into a false measurement:

* **exact** observations - somebody chose a percentage and could have chosen a
  higher one;
* **censored** observations - the slider was at the ceiling, so the record says
  "at least this", and least squares reading it as an equality is simply wrong;
* **provenance** - which round, which sequence, which stack, and what was left
  out and why.

Nothing here talks to a clock, and nothing here writes firmware schema. It
reads two saved files and writes a profile and an evaluation report.

    python3 scripts/factory_luminance.py build \\
        --measurements tests/fixtures/2026-08-26-manual-colour-brightness-points.json \\
        --coupling tests/fixtures/2026-08-26-current-diffuser-before-mask-coupling.json \\
        --profile artifacts/factory-profile.json \\
        --report artifacts/factory-evaluation.json \\
        --csv artifacts/factory-evaluation.csv
"""

import hashlib
import io
import json
import math
import os

import colour_luminance as cl
from colour_luminance import (_bounded, _mapping, _number, _sequence,
                              _shape_of)


# ==========================================================================
# What the profile is, and what its identifiers mean
# ==========================================================================

# Bumped when a field changes meaning, never when one is added - the same rule
# `SETTINGS_SCHEMA` follows in src/Settings.cpp. A reader that finds a version
# it does not know must refuse, not guess.
SCHEMA_VERSION = 1

# The shape of the model, not the numbers in it. Two profiles with the same
# `modelId` can be compared field for field; two with different ones cannot,
# whatever their fields happen to be called.
MODEL_ID = "white-baseline-plus-cyclic-hue-loglux-residual-grid"

# The six hue knots the rounds were measured at, in degrees, cyclic over 360.
HUE_KNOTS = (0, 60, 120, 180, 240, 300)
HUE_PERIOD = 360

# Colour points are measured at full saturation; white anchors at none. Any
# other saturation in a round is a point this model has no place to put.
COLOUR_SAT = 100
WHITE_SAT = 0

# A `rounds` entry carrying this `kind` is not a round at all: it declares a
# family of two-point sittings, each a white anchor and one colour at one
# ambient level. `id` names the family and `levels` names the sittings, whose
# own ids are "<family>-<level>".
TARGETED_KIND = "targeted-repeat"

# Stored numbers are rounded to this many decimals before the checksum is
# taken. `log10` is allowed to differ by an ulp between C libraries, which at
# these magnitudes is around 1e-17; rounding here is what makes the checksum a
# statement about the *data* rather than about the libm that read it.
ROUND_DECIMALS = 10


class MeasurementError(ValueError):
    """A measurement record that cannot be read as one.

    Its own class so the command line can turn it into a line of text without
    also swallowing a genuine fault in this module.
    """


# ==========================================================================
# Reading the record
# ==========================================================================

class Observation(object):
    """One thing somebody did: a percentage chosen at a colour and a level."""

    __slots__ = ("index", "sequence", "hue", "sat", "percent", "lux",
                 "log_lux", "censored", "label", "round_id", "log_output")

    def __init__(self, index, sequence, hue, sat, percent, lux, censored,
                 label, round_id, log_output):
        self.index = index
        self.sequence = sequence
        self.hue = hue
        self.sat = sat
        self.percent = percent
        self.lux = lux
        self.log_lux = cl.log_lux(lux)
        self.censored = censored
        self.label = label
        self.round_id = round_id
        # What the *hardware* emits at this setting, recomputed through the
        # firmware's own path. The record carries the same number; see
        # `load_measurements`, which checks the two agree rather than
        # believing either on its own.
        self.log_output = log_output

    @property
    def is_white(self):
        return self.sat == WHITE_SAT

    def __repr__(self):
        return "Observation(#%d %s %.4g lx hue=%d sat=%d %d%%%s)" % (
            self.index, self.round_id, self.lux, self.hue, self.sat,
            self.percent, ", censored" if self.censored else "")


class Round(object):
    """One sitting: white anchors interleaved with the six hue knots."""

    __slots__ = ("round_id", "lux", "log_lux", "observations", "declared",
                 "protocol")

    def __init__(self, round_id, lux, observations, declared, protocol):
        self.round_id = round_id
        self.lux = lux
        self.log_lux = cl.log_lux(lux)
        self.observations = observations
        # True when the record's own `rounds` list names it. The 0.15 lx round
        # is real and is not declared; see `_group`.
        self.declared = declared
        self.protocol = protocol

    @property
    def anchors(self):
        return [o for o in self.observations if o.is_white]

    @property
    def colours(self):
        return [o for o in self.observations if not o.is_white]

    @property
    def censored(self):
        return any(o.censored for o in self.observations)

    def __repr__(self):
        return "Round(%s, %.4g lx, n=%d%s)" % (
            self.round_id, self.lux, len(self.observations),
            ", censored" if self.censored else "")


class TargetedPair(object):
    """One white anchor and one colour, taken together at one level.

    Not a round and never promoted into one. A round is a statement about six
    hues; a pair is a statement about *one*, and the five it says nothing
    about must stay unsaid. What it does measure, it measures well: the two
    observations are adjacent in the same sitting, so the difference between
    them is a residual with the observer's adaptation divided out.
    """

    __slots__ = ("round_id", "family", "hue", "lux", "log_lux",
                 "observations", "anchor", "colour")

    def __init__(self, round_id, family, hue, observations):
        self.round_id = round_id
        self.family = family
        self.hue = hue
        self.observations = observations
        self.anchor = [o for o in observations if o.is_white][0]
        self.colour = [o for o in observations if not o.is_white][0]
        self.lux = self.colour.lux
        self.log_lux = self.colour.log_lux

    @property
    def residual(self):
        """Its own white, never the six-hue round's.

        Borrowing the round's baseline would turn a replicate of the residual
        into a replicate of the *level*, which is exactly the thing this
        session cannot speak to - see `WHITE_BASELINE_NOTE`.
        """
        return self.colour.log_output - self.anchor.log_output

    @property
    def censored(self):
        return any(o.censored for o in self.observations)

    def __repr__(self):
        return "TargetedPair(%s, %.4g lx, hue=%d, residual=%+.4f)" % (
            self.round_id, self.lux, self.hue, self.residual)


class Measurements(object):
    """A parsed record: the usable rounds, and everything left out with why."""

    __slots__ = ("rounds", "excluded", "stack_id", "source", "note",
                 "pairs", "families")

    def __init__(self, rounds, excluded, stack_id, source, note,
                 pairs=None, families=None):
        self.rounds = rounds
        self.excluded = excluded
        self.stack_id = stack_id
        self.source = source
        self.note = note
        # Targeted W-to-colour pairs, and the family declarations that claim
        # them. Empty on a record that has none, which is every record written
        # before this existed.
        self.pairs = list(pairs or [])
        self.families = dict(families or {})


def _flag(name, value, default=False):
    """A JSON boolean. A number is not one, however much Python agrees."""
    if value is None:
        return default
    if not isinstance(value, bool):
        raise MeasurementError("%s is %s; true or false was expected"
                               % (name, _shape_of(value)))
    return value


def _text(name, value, default=""):
    if value is None:
        return default
    if not isinstance(value, str):
        raise MeasurementError("%s is %s; a string was expected"
                               % (name, _shape_of(value)))
    return value


def _refuse(problem):
    """Every shape complaint from `colour_luminance` re-raised as ours.

    Those helpers raise plain `ValueError`, which is right for them and wrong
    at this boundary: the caller wants to know it was the measurement file.
    """
    return MeasurementError(str(problem))


def load_measurements(doc, drive=None, weights=cl.PHOTOPIC_WEIGHTS,
                      check_stored_output=True):
    """The record as rounds, in ascending light, or a refusal saying why.

    Fail-closed throughout: a missing hue is not assumed, a missing saturation
    is not assumed, a round that is not a complete `W-…-W` sitting at one
    ambient level is not repaired into one. What cannot be read as a round is
    listed as excluded, with the reason, rather than dropped.

    `check_stored_output` recomputes every point through the firmware's path
    and compares it with the `relativePhotopicOutput` the record carries. They
    are the same quantity computed twice; if they disagree, one of the two is
    describing different hardware and neither may be used.
    """
    if drive is None:
        drive = cl.DriveTable.proportional()
    try:
        weights = cl._check_weights(weights)
    except ValueError as problem:
        raise _refuse(problem)

    doc = _mapping_or_refuse("the measurement record", doc)
    stack_id = _text("stackId", doc.get("stackId"))
    if not stack_id:
        raise MeasurementError(
            "the measurement record names no stackId. The optical stack is "
            "part of the photometry, so a profile built from an unnamed one "
            "could be applied to a clock it does not describe")

    raw_points = doc.get("points")
    if raw_points is None:
        raise MeasurementError("the measurement record has no 'points'")
    raw_points = _sequence_or_refuse("points", raw_points)
    if not raw_points:
        raise MeasurementError("the measurement record holds no points")

    declared, families = _declared_rounds(doc)

    observations = []
    for index, entry in enumerate(raw_points):
        observations.append(_observation(index, entry, drive, weights,
                                         check_stored_output))

    groups, pairs, excluded = _group(observations, declared, families)
    if not groups:
        raise MeasurementError(
            "no complete round in this record: a round needs at least one "
            "white anchor at sat=%d and the six hue knots %s once each at "
            "sat=%d, all at one ambient level"
            % (WHITE_SAT, ", ".join(str(h) for h in HUE_KNOTS), COLOUR_SAT))

    return Measurements(groups, excluded, stack_id,
                        _text("source", doc.get("source")),
                        _text("note", doc.get("note")),
                        pairs=pairs, families=families)


def _mapping_or_refuse(name, value):
    try:
        return _mapping(name, value)
    except ValueError as problem:
        raise _refuse(problem)


def _sequence_or_refuse(name, value):
    try:
        return _sequence(name, value)
    except ValueError as problem:
        raise _refuse(problem)


def _declared_rounds(doc):
    """The record's own `rounds` list, by id.

    It is metadata about groups that are also identifiable from the points
    themselves, so it is checked against them rather than trusted: a declared
    count that disagrees with the points is a record that has been edited in
    one place and not the other.
    """
    declared, families = {}, {}
    raw = doc.get("rounds")
    if raw is None:
        return declared, families
    for index, entry in enumerate(_sequence_or_refuse("rounds", raw)):
        entry = _mapping_or_refuse("round %d" % index, entry)
        round_id = _text("the id of round %d" % index, entry.get("id"))
        if not round_id:
            raise MeasurementError("round %d has no id" % index)
        if round_id in declared or round_id in families:
            raise MeasurementError("round %r is declared twice" % round_id)

        kind = _text("the kind of round %r" % round_id, entry.get("kind"))
        if kind == TARGETED_KIND:
            families[round_id] = _family(round_id, entry)
            continue
        if kind:
            raise MeasurementError(
                "round %r declares kind %r, and this code knows only %r or no "
                "kind at all (a six-hue round). Guessing what an unknown kind "
                "means is how a two-point sitting becomes a round"
                % (round_id, kind, TARGETED_KIND))
        try:
            lux = _number("the light of round %r" % round_id, entry.get("lux"))
        except ValueError as problem:
            raise _refuse(problem)
        if lux <= 0.0:
            raise MeasurementError("round %r is at %r lx; light is positive"
                                   % (round_id, lux))
        count = entry.get("count")
        if count is not None:
            count = _whole("the count of round %r" % round_id, count, 1, 1000)
        declared[round_id] = {
            "lux": lux,
            "count": count,
            "protocol": _text("the protocol of round %r" % round_id,
                              entry.get("protocol")),
            "ceilingLimited": _flag("the ceilingLimited flag of round %r"
                                    % round_id, entry.get("ceilingLimited")),
        }
    return declared, families


def _family(family_id, entry):
    """A targeted-repeat declaration: which hue, and at which levels.

    Both are required and neither is inferred. Without the hue there is
    nothing to check the pair's colour against, and a pair whose colour is
    taken on trust is a pair that can quietly land on the wrong knot.
    """
    if entry.get("hue") is None:
        raise MeasurementError(
            "the targeted family %r declares no hue. It is not inferred from "
            "its points: the declaration is what the points are checked "
            "against, and a family that agrees with whatever arrives checks "
            "nothing" % family_id)
    hue = _whole("the hue of the targeted family %r" % family_id,
                 entry["hue"], 0, HUE_PERIOD - 1)
    if hue not in HUE_KNOTS:
        raise MeasurementError(
            "the targeted family %r is at hue %d, which is not one of the "
            "grid knots %s. A replicate has to replicate something"
            % (family_id, hue, ", ".join(str(h) for h in HUE_KNOTS)))

    if entry.get("levels") is None:
        raise MeasurementError(
            "the targeted family %r declares no levels, so nothing says which "
            "sittings belong to it" % family_id)
    levels = []
    for position, value in enumerate(
            _sequence_or_refuse("the levels of family %r" % family_id,
                                entry["levels"])):
        level = _number("level %d of family %r" % (position, family_id), value)
        if level <= 0.0:
            raise MeasurementError("family %r has a level at %r lx; light is "
                                   "positive" % (family_id, level))
        levels.append(level)
    if not levels:
        raise MeasurementError("the targeted family %r declares no levels"
                               % family_id)
    if len(set(levels)) != len(levels):
        raise MeasurementError("the targeted family %r declares a level twice"
                               % family_id)

    count = entry.get("count")
    if count is not None:
        count = _whole("the count of family %r" % family_id, count, 1, 1000)
        if count != 2 * len(levels):
            raise MeasurementError(
                "family %r declares %d points over %d levels; a pair is two "
                "points, so %d were expected"
                % (family_id, count, len(levels), 2 * len(levels)))

    return {
        "id": family_id,
        "kind": TARGETED_KIND,
        "hue": hue,
        "levels": levels,
        "count": count,
        "protocol": _text("the protocol of family %r" % family_id,
                          entry.get("protocol")),
        "note": _text("the note of family %r" % family_id, entry.get("note")),
        # The sitting ids this family claims. Written out rather than matched
        # by prefix: a prefix would also swallow an unrelated id that happens
        # to start with the same characters.
        "roundIds": {_targeted_id(family_id, level): level for level in levels},
    }


def _targeted_id(family_id, level):
    """The sitting id a family claims for one of its levels."""
    return "%s-%g" % (family_id, level)


def _whole(name, value, low, high):
    try:
        return _bounded(name, value, low, high)
    except ValueError as problem:
        raise _refuse(problem)


def _observation(index, entry, drive, weights, check_stored_output):
    """One point, with nothing inferred.

    `hue` and `sat` are required rather than defaulted. A point that does not
    say what colour it was taken in cannot be placed on a hue axis, and the
    default that suggests itself - hue 0 - is red.
    """
    where = "point %d" % index
    entry = _mapping_or_refuse(where, entry)

    for field in ("hue", "sat", "percent", "lux"):
        if entry.get(field) is None:
            raise MeasurementError(
                "%s has no %r. It is not inferred: a point whose colour or "
                "level is unknown is not a point on this grid" % (where, field))

    hue = _whole("the hue at %s" % where, entry["hue"], 0, HUE_PERIOD - 1)
    sat = _whole("the saturation at %s" % where, entry["sat"], 0, 100)
    percent = _whole("the percent at %s" % where, entry["percent"], 0, 100)
    try:
        lux = _number("the light at %s" % where, entry["lux"])
    except ValueError as problem:
        raise _refuse(problem)
    if lux <= 0.0:
        raise MeasurementError("%s is at %r lx; light is positive"
                               % (where, lux))

    sequence = entry.get("sequence")
    if sequence is not None:
        sequence = _whole("the sequence at %s" % where, sequence, 0, 1000)

    log_output = cl.log10_output(
        cl.relative_photopic_output(hue, sat, percent, weights, drive))

    if check_stored_output and entry.get("relativePhotopicOutput") is not None:
        try:
            stored = _number("relativePhotopicOutput at %s" % where,
                             entry["relativePhotopicOutput"])
        except ValueError as problem:
            raise _refuse(problem)
        recomputed = cl.relative_photopic_output(hue, sat, percent, weights,
                                                 drive)
        if not _close(stored, recomputed):
            raise MeasurementError(
                "%s says it emits %r, and the same hue, saturation and "
                "percent through the firmware's own path give %r. One of the "
                "two describes different hardware - check the coupling record "
                "and the photopic weights before using either"
                % (where, stored, recomputed))

    return Observation(
        index=index,
        sequence=sequence,
        hue=hue,
        sat=sat,
        percent=percent,
        lux=lux,
        censored=_flag("the censored flag at %s" % where, entry.get("censored")),
        label=_text("the label at %s" % where, entry.get("label")),
        round_id=_text("the roundId at %s" % where, entry.get("roundId")),
        log_output=log_output)


def _close(a, b, tolerance=1e-9):
    return abs(float(a) - float(b)) <= tolerance * max(1.0, abs(float(b)))


def _group(observations, declared, families=None):
    """Points into rounds, in the order they appear, then by ascending light.

    A point carrying a `roundId` belongs to that round. A point without one is
    grouped by its **exact** ambient level, which is the only thing such a
    point states about which sitting it belongs to. Either way the group is
    then held to the same standard, so a real round recovered from bare points
    is a round and a scatter of points at nearby levels is not.
    """
    families = families or {}
    claimed = {}
    for family in families.values():
        for round_id, level in family["roundIds"].items():
            claimed[round_id] = (family, level)

    order = []
    groups = {}
    for observation in observations:
        key = observation.round_id or _undeclared_key(observation)
        if key not in groups:
            groups[key] = []
            order.append(key)
        groups[key].append(observation)

    # A declared sitting with no points is a record edited in one place and
    # not the other, and it is the half that would go unnoticed: the family
    # would simply contribute fewer replicates than it claims.
    for round_id, (family, level) in sorted(claimed.items()):
        if round_id not in groups:
            raise MeasurementError(
                "the targeted family %r declares a sitting at %g lx (%r) and "
                "no point carries that roundId"
                % (family["id"], level, round_id))

    rounds = []
    pairs = []
    excluded = []
    for key in order:
        members = groups[key]
        if key in claimed:
            family, level = claimed[key]
            problem = _pair_problem(key, members, family, level)
            if problem is None:
                pairs.append(TargetedPair(key, family["id"], family["hue"],
                                          sorted(members, key=_sequence_of)))
                continue
            # A malformed pair is refused outright rather than excluded. A
            # round can be dropped and the grid still stands on the others; a
            # family that declared this sitting cannot quietly lose it.
            raise MeasurementError("the targeted sitting %r is not a pair: %s"
                                   % (key, problem))
        problem = _round_problem(key, members, declared)
        if problem is not None:
            for observation in members:
                excluded.append({
                    "index": observation.index,
                    "lux": observation.lux,
                    "hue": observation.hue,
                    "sat": observation.sat,
                    "percent": observation.percent,
                    "group": key,
                    "reason": problem,
                })
            continue
        members = sorted(members, key=_sequence_of)
        rounds.append(Round(
            round_id=key,
            lux=members[0].lux,
            observations=members,
            declared=key in declared,
            protocol=declared.get(key, {}).get("protocol", "")))

    pairs.sort(key=lambda pair: (pair.log_lux, pair.hue))
    rounds.sort(key=lambda round_: round_.log_lux)
    for lower, higher in zip(rounds, rounds[1:]):
        if not higher.log_lux > lower.log_lux:
            raise MeasurementError(
                "rounds %r and %r are both at %r lx; two sittings at one level "
                "cannot both be a grid knot"
                % (lower.round_id, higher.round_id, lower.lux))
    return rounds, pairs, excluded


def _pair_problem(key, members, family, level):
    """None if this sitting is a usable pair, else the reason it is not."""
    if len(members) != 2:
        return ("%d points, and a pair is exactly two - one white anchor and "
                "one colour" % len(members))

    anchors = [o for o in members if o.is_white]
    colours = [o for o in members if not o.is_white]
    if len(anchors) != 1:
        return ("%d white anchors at sat=%d, and a pair has exactly one - it "
                "is what the colour beside it is measured against"
                % (len(anchors), WHITE_SAT))
    if len(colours) != 1:
        return "%d colour observations, and a pair has exactly one" % len(colours)

    colour = colours[0]
    if colour.hue != family["hue"]:
        return ("its colour is hue %d and the family %r declares hue %d. The "
                "declaration decides; a sitting that names its own hue could "
                "land on any knot"
                % (colour.hue, family["id"], family["hue"]))
    if colour.sat != COLOUR_SAT:
        return ("its colour is at saturation %d and the grid is measured at "
                "%d" % (colour.sat, COLOUR_SAT))

    if len({o.log_lux for o in members}) != 1:
        return "its two points are at different ambient levels"
    if not _close(level, colour.lux):
        return ("declared at %g lx and its points are at %r lx"
                % (level, colour.lux))
    if _sequence_of(anchors[0]) == _sequence_of(colour):
        return "its two points share a sequence number, so their order is unknown"
    return None


def _undeclared_key(observation):
    """The group name a point with no `roundId` falls into.

    Its exact stored level, formatted rather than rounded: two points a
    thousandth of a decade apart were not taken at one ambient level, and
    binning them together is exactly the repair this refuses to make.
    """
    return "undeclared@%.12f-log10lux" % observation.log_lux


def _sequence_of(observation):
    """Where a point sits in its round.

    `sequence` where the record gives one; otherwise the order it was written
    in, which is what the 0.15 lx round has and all it has. Both describe the
    same thing - how far through the sitting the observer was - which is what
    the white anchors are interpolated against.
    """
    return (observation.sequence if observation.sequence is not None
            else observation.index)


def _round_problem(key, members, declared):
    """None if this group is a usable round, else the reason it is not."""
    levels = sorted({observation.log_lux for observation in members})
    if len(levels) != 1:
        return ("not one ambient level: %d different ones between %.4g and "
                "%.4g lx, so these points are not one sitting"
                % (len(levels), 10.0 ** levels[0], 10.0 ** levels[-1]))

    anchors = [o for o in members if o.is_white]
    if not anchors:
        return ("no white anchor at sat=%d: without one there is nothing to "
                "measure a colour residual against" % WHITE_SAT)

    colours = [o for o in members if not o.is_white]
    off_grid = sorted({o.sat for o in colours if o.sat != COLOUR_SAT})
    if off_grid:
        return ("saturation %s is neither the white anchor (%d) nor the "
                "measured colour saturation (%d), and this model has nowhere "
                "to put it"
                % (", ".join(str(s) for s in off_grid), WHITE_SAT, COLOUR_SAT))

    hues = [o.hue for o in colours]
    if sorted(hues) != sorted(HUE_KNOTS):
        missing = sorted(set(HUE_KNOTS) - set(hues))
        extra = sorted(set(hues) - set(HUE_KNOTS))
        repeated = sorted({h for h in hues if hues.count(h) > 1})
        return ("not the six hue knots once each%s%s%s"
                % ("; missing %s" % ", ".join(str(h) for h in missing) if missing else "",
                   "; unexpected %s" % ", ".join(str(h) for h in extra) if extra else "",
                   "; repeated %s" % ", ".join(str(h) for h in repeated) if repeated else ""))

    sequences = [_sequence_of(o) for o in members]
    if len(set(sequences)) != len(sequences):
        return "two points share a sequence number, so their order is unknown"

    if key in declared:
        count = declared[key]["count"]
        if count is not None and count != len(members):
            return ("declared as %d points and %d are present, so the record "
                    "has been edited in one place and not the other"
                    % (count, len(members)))
        if not _close(declared[key]["lux"], members[0].lux):
            return ("declared at %r lx and its points are at %r lx"
                    % (declared[key]["lux"], members[0].lux))
    return None


# ==========================================================================
# The observer drifts, and the anchors are how that is seen
# ==========================================================================
#
# A round takes minutes and the eye adapts over them, which is why the protocol
# interleaves white: `W-0-180-W-60-240-W-120-300-W`. The 0.07 lx round is the
# case that proves the point - its anchors read 35, 35, 38, 35 %, so the middle
# third of that sitting was measured against a brighter reference than the ends
# were, and a colour taken there is not comparable with one taken at the start
# unless the drift is taken out.
#
# Taken out in **log output**, not in percent: the same drift is a different
# number of percentage points at every level, and the whole reason for this
# coordinate is that it is not.

def white_at(round_, sequence):
    """The white reference beside a point at this position in the round.

    Linear between the anchors either side, and **held flat outside them**. An
    anchor is an observation; before the first and after the last there is
    none, and running a line backwards out of the two nearest is a guess where
    the honest answer is "the same as the nearest thing we saw".
    """
    anchors = sorted(((_sequence_of(o), o.log_output) for o in round_.anchors))
    if sequence <= anchors[0][0]:
        return anchors[0][1]
    if sequence >= anchors[-1][0]:
        return anchors[-1][1]
    for (low, at_low), (high, at_high) in zip(anchors, anchors[1:]):
        if low <= sequence <= high:
            if high == low:
                return at_low
            return at_low + (at_high - at_low) * (sequence - low) / float(high - low)
    return anchors[-1][1]                                   # unreachable


def residual_of(round_, observation):
    """How far this colour sits from the white beside it, in decades.

    Negative for every saturated colour on this clock: at one setting a colour
    emits less light than white does, and the observer did not make all of it
    back with the slider.
    """
    return observation.log_output - white_at(round_, _sequence_of(observation))


def white_baseline(round_):
    """The one white value this round contributes to the profile's curve.

    The mean of its anchors, in log output. The drift is already accounted for
    where it matters - between the anchors, against the colour beside them -
    and what is left for the grid is one number per level.
    """
    anchors = round_.anchors
    return sum(o.log_output for o in anchors) / float(len(anchors))


# ==========================================================================
# The profile
# ==========================================================================

# The regulated range a clock defaults to. It travels with the profile because
# which observations were at the ceiling depends on where the ceiling was, and
# a grid read against a different range is being read against a different
# experiment.
DEFAULT_PERCENT_RANGE = (cl.LUM_MIN_PERCENT, cl.LUM_MAX_PERCENT)

SAT_FADE_NOTE = (
    "The colour residual is scaled by sat/100: nothing at sat=0, all of it at "
    "sat=100. Linear because it is the simplest curve through the only two "
    "saturations this record contains, and declared here rather than left "
    "implicit because nothing in the data identifies the shape between them - "
    "every colour observation is at sat=100 and every anchor at sat=0.")

EXCLUSION_NOTE = (
    "Taken without interleaved white anchors, at eight different ambient "
    "levels between 2.0 and 2.8 lx, one of them at a saturation and a hue that "
    "are not on this grid. Nothing in them can be corrected for the observer "
    "drifting, so they are not used - superseded by the drift-controlled 1.9 "
    "lx round, which covers the same part of the range and can be.")


def _round_number(value):
    """One number, rounded where the checksum can rely on it.

    `-0.0` is folded into `0.0`: the two are equal, `json` writes them
    differently, and a profile whose checksum depends on the sign of a zero
    would be reporting a difference nobody made.
    """
    rounded = round(float(value), ROUND_DECIMALS)
    return 0.0 if rounded == 0.0 else rounded


WHITE_BASELINE_NOTE = (
    "A targeted pair's white anchor is used to normalise its own colour and "
    "nothing else. Its anchors sit up to half a decade below the six-hue "
    "rounds' at the dark levels, which says the session was in a different "
    "adaptation state rather than that the rounds were wrong - and one anchor "
    "from a session aimed at one hue must not move the baseline every hue is "
    "measured against. What it *would* have moved is reported under "
    "`ifPooled`, so the choice is evidence rather than assertion. The pair's "
    "same-sitting difference is unaffected by any of this, which is exactly "
    "why the residual travels and the level does not.")

TARGETED_RESIDUAL_NOTE = (
    "A targeted pair measures one hue's residual a second time, so it joins "
    "that knot as a replicate and the knot becomes the mean of what it holds, "
    "weighted by how many observations that is. It fills in no other hue: a "
    "two-point sitting says nothing about the five it did not look at.")


def _residual_contribution(kind, round_id, observation, residual):
    return {
        "kind": kind,
        "roundId": round_id,
        "index": observation.index,
        "percent": observation.percent,
        "sequence": _sequence_of(observation),
        "logOutput": _round_number(observation.log_output),
        "logOutputResidual": _round_number(residual),
        "censored": bool(observation.censored),
    }


def build_profile(measurements, drive, weights=cl.PHOTOPIC_WEIGHTS,
                  percent_range=DEFAULT_PERCENT_RANGE, profile_id=None,
                  generator=None, include_targeted=True):
    """A factory profile out of parsed measurements. Deterministic.

    Everything in it is either measured or declared. Nothing is fitted: with
    six ambient levels and six hues the grid *is* the data, and a smooth
    surface laid over it would be a claim the leave-one-level-out check has
    not yet earned. See `cross_validate`.
    """
    try:
        weights = cl._check_weights(weights)
    except ValueError as problem:
        raise _refuse(problem)

    low, high = _percent_range(percent_range)
    rounds = measurements.rounds

    # --- what was observed -------------------------------------------------
    observed_white = [white_baseline(round_) for round_ in rounds]
    anchor_counts = [float(len(round_.anchors)) for round_ in rounds]
    by_hue = [{o.hue: o for o in round_.colours} for round_ in rounds]
    # Which pairs land on which level. A pair at a level with no six-hue round
    # has no white baseline to sit against and cannot join the grid; it is
    # recorded as validation-only rather than given one.
    pairs_by_level, orphans = _place_pairs(measurements, rounds,
                                           include_targeted)

    observed_targets, contributions, weights_by_hue = {}, {}, {}
    for hue in HUE_KNOTS:
        observed_targets[hue], contributions[hue], weights_by_hue[hue] = [], [], []
        for position, (round_, white, table) in enumerate(
                zip(rounds, observed_white, by_hue)):
            found = [_residual_contribution("round", round_.round_id,
                                            table[hue],
                                            residual_of(round_, table[hue]))]
            for pair in pairs_by_level.get(position, []):
                if pair.hue == hue:
                    found.append(_residual_contribution(
                        "targeted", pair.round_id, pair.colour, pair.residual))
            # Every observation of one quantity weighted the same, which is
            # what makes this an average rather than a preference.
            mean = (sum(c["logOutputResidual"] for c in found)
                    / float(len(found)))
            observed_targets[hue].append(white + mean)
            contributions[hue].append(found)
            weights_by_hue[hue].append(float(len(found)))

    # --- and the nearest thing to it that a brighter room cannot undo ------
    projected_white = isotonic(observed_white, anchor_counts)
    projected_targets = {}
    for hue in HUE_KNOTS:
        projected_targets[hue] = isotonic(observed_targets[hue],
                                          weights_by_hue[hue])
        _honour_lower_bounds(hue, rounds, by_hue, high,
                             observed_targets[hue], projected_targets[hue])

    levels = []
    for position, round_ in enumerate(rounds):
        residuals = []
        for hue in HUE_KNOTS:
            observation = by_hue[position][hue]
            target = projected_targets[hue][position]
            observed_target = observed_targets[hue][position]
            adjustment = _round_number(target - observed_target)
            residuals.append({
                "hue": hue,
                # What the evaluator reads: the projected target, expressed
                # against the projected white so the two stay consistent.
                "logOutputResidual": _round_number(
                    target - projected_white[position]),
                "targetLogOutput": _round_number(target),
                # What was seen, over every observation of this knot: one
                # residual where nothing repeated it, their mean where
                # something did. Each contribution stays visible under
                # `observations`, so the mean never hides what it averaged.
                "logOutputResidualObserved": _round_number(
                    observed_target - observed_white[position]),
                "targetLogOutputObserved": _round_number(observed_target),
                "adjustmentDecades": adjustment,
                "adjusted": adjustment != 0.0,
                # How many observations stand behind this knot, and which.
                # One is the ordinary case; two means a targeted sitting
                # measured the same residual again.
                "replicates": len(contributions[hue][position]),
                "weight": weights_by_hue[hue][position],
                "observations": contributions[hue][position],
                # "lower" says the slider ran out: the observer wanted at
                # least this much and may have wanted more, so the number is a
                # bound and never an equality. Null is an exact observation.
                "bound": "lower" if _is_censored(observation, high) else None,
                "percent": observation.percent,
                "sequence": _sequence_of(observation),
                "index": observation.index,
                "logOutput": _round_number(observation.log_output),
                "whiteLogOutputBeside": _round_number(
                    white_at(round_, _sequence_of(observation))),
            })

        levels.append({
            "roundId": round_.round_id,
            "declaredRound": round_.declared,
            "lux": round_.lux,
            "logLux": _round_number(round_.log_lux),
            "whiteLogOutput": _round_number(projected_white[position]),
            "whiteLogOutputObserved": _round_number(observed_white[position]),
            "whiteAdjustmentDecades": _round_number(
                projected_white[position] - observed_white[position]),
            "whiteAnchors": [{
                "index": o.index,
                "sequence": _sequence_of(o),
                "sat": o.sat,
                "percent": o.percent,
                "logOutput": _round_number(o.log_output),
            } for o in round_.anchors],
            "censored": round_.censored,
            "residuals": residuals,
        })

    payload = {
        "schemaVersion": SCHEMA_VERSION,
        "modelId": MODEL_ID,
        "profileId": profile_id or _profile_id(measurements, levels),
        "stackId": measurements.stack_id,
        "hueKnots": list(HUE_KNOTS),
        "hueCyclic": True,
        "huePeriodDegrees": HUE_PERIOD,
        "satFade": {
            "kind": "linear",
            "zeroAtSat": WHITE_SAT,
            "fullAtSat": COLOUR_SAT,
            "measured": False,
            "note": SAT_FADE_NOTE,
        },
        "percentRange": {"min": low, "max": high},
        "logLuxRange": {
            "min": _round_number(levels[0]["logLux"]),
            "max": _round_number(levels[-1]["logLux"]),
        },
        "luxRange": {"min": levels[0]["lux"], "max": levels[-1]["lux"]},
        "extrapolation": "clamp",
        "monotonic": {
            "method": "weighted-isotonic-pava",
            "coordinate": "targetLogOutput",
            "whiteWeight": "anchorCount",
            "censoredLoss": "one-sided-lower-bound",
            "note": ISOTONIC_NOTE,
        },
        "levels": levels,
        "targeted": _targeted_block(measurements, rounds, observed_white,
                                    by_hue, pairs_by_level, orphans,
                                    include_targeted),
        "diagnostics": _diagnostics(levels),
        "source": {
            "stackId": measurements.stack_id,
            "measurementSource": measurements.source,
            "measurementNote": measurements.note,
            "generator": generator or "scripts/factory_luminance.py",
            "photopicWeights": [float(w) for w in weights],
            "driveAssumed": bool(drive.assumed),
            "driveSource": drive.source,
            "driveLevels": list(drive.levels),
            "driveResponse": [float(r) for r in drive.response],
            "rounds": [{
                "roundId": round_.round_id,
                "lux": round_.lux,
                "declared": round_.declared,
                "protocol": round_.protocol,
                "anchors": len(round_.anchors),
                "censored": round_.censored,
            } for round_ in measurements.rounds],
            "excluded": measurements.excluded,
            "exclusionNote": EXCLUSION_NOTE,
            "supersededBy": _supersedes(measurements),
        },
    }
    payload["checksum"] = {"algorithm": "sha256",
                           "value": profile_checksum(payload)}
    return payload


# ==========================================================================
# A brighter room may not ask for a dimmer clock
# ==========================================================================
#
# The grid reproduces its measurements exactly, including where two sittings
# contradict each other - and in this record they do, once. The 0.15 lx round
# is the only one with anchors at its two ends and nothing in between, and its
# observer drifted from 40 % to 45 % across it, so its mean white sits *above*
# the steady 40 % of the 0.5 lx round three times further into the light. Four
# of the seven series inherit that step down: white, and hues 60, 180 and 240.
#
# It is fixed here rather than in the evaluator, because it is a property of
# the profile and not of a query, and by **isotonic regression** rather than by
# anything with a parameter in it:
#
# * it is a projection, so the answer is the nearest non-decreasing series to
#   the measurements there is - "smallest" is literal, not a judgement;
# * it is in the coordinate the model is in, `targetLogOutput`, which is where
#   the constraint means something. Percent is quantised and carries the gamma;
# * it leaves an already-monotone series exactly alone, so hues 0, 120 and 300
#   are untouched and can be seen to be;
# * pooling two contradicting levels into their weighted mean is the honest
#   reading of "these two sittings could not tell these levels apart", and the
#   weights are how many observations stand behind each - four steady anchors
#   outvote two drifting ones.
#
# Monotone white and monotone per-hue targets is enough for the whole surface,
# and that is the reason the projection is applied to those seven series and
# nothing else: every answer this profile gives is a convex combination of
# them - `fade` between white and a hue target, `fraction` between two hue
# knots, `along` between two levels - and a convex combination of
# non-decreasing functions is non-decreasing.

ISOTONIC_NOTE = (
    "Monotonicity in ambient light is imposed by isotonic regression (pool "
    "adjacent violators) on the white baseline and on each hue's target, in "
    "log output. It is the least-squares projection onto non-decreasing "
    "series, so an already-monotone series is returned unchanged and nothing "
    "moves further than it must. White is weighted by how many anchors stand "
    "behind each level. Censored observations are lower bounds and are never "
    "pulled below what was observed; a record that would require that is "
    "refused rather than averaged. The observed values are kept beside the "
    "projected ones, and `diagnostics` reports the contradiction in the "
    "source rather than hiding it.")


def isotonic(values, weights):
    """The nearest non-decreasing series, by weighted least squares.

    Pool adjacent violators: walk left to right, and whenever the block just
    added sits below the one before it, merge the two into their weighted mean
    and check again. What comes out is the unique minimiser of
    `sum(w * (fitted - value)^2)` over non-decreasing series.
    """
    values = _sequence_or_refuse("the values to project", values)
    weights = _sequence_or_refuse("the weights to project with", weights)
    if len(values) != len(weights):
        raise MeasurementError(
            "%d values and %d weights: the projection needs one weight each"
            % (len(values), len(weights)))

    blocks = []                       # (first index, count, mean, weight)
    for position, (value, weight) in enumerate(zip(values, weights)):
        value = _number("the value at position %d" % position, value)
        weight = _number("the weight at position %d" % position, weight)
        if weight <= 0.0:
            raise MeasurementError(
                "the weight at position %d is %r; a projection weight is "
                "positive" % (position, weight))
        blocks.append([position, 1, value, weight])
        while len(blocks) > 1 and blocks[-2][2] > blocks[-1][2]:
            later = blocks.pop()
            earlier = blocks.pop()
            total = earlier[3] + later[3]
            blocks.append([earlier[0], earlier[1] + later[1],
                           (earlier[2] * earlier[3] + later[2] * later[3]) / total,
                           total])

    projected = [0.0] * len(values)
    for start, count, mean, _ in blocks:
        for offset in range(count):
            projected[start + offset] = mean
    return projected


def _honour_lower_bounds(hue, rounds, by_hue, max_percent, observed, projected):
    """A censored observation may be raised, never lowered.

    "100 %" means "at least 100 %", so least squares reading it as an equality
    is the mistake this project already names in `censor()`. Here the same rule
    applies in the other direction: the projection may pull an *exact* point
    down to meet its neighbours, and it may not do that to a bound.

    Pooling only ever lowers a point that a **later** one undercuts, so a
    censored observation is at risk exactly when a brighter level asks for
    less than it does. Every censored point in this record is at the brightest
    level, with nothing after it, which is why the check never fires here -
    and why it is a check rather than a comment.

    When it does fire the case is **refused** rather than solved with a
    one-sided loss nothing here could validate against real data. A record
    that needs one is a record where a bound and a brighter round contradict
    each other, and that wants another sitting, not an algorithm.
    """
    for position, (round_, table) in enumerate(zip(rounds, by_hue)):
        observation = table[hue]
        if not _is_censored(observation, max_percent):
            continue
        if projected[position] < observed[position] - 1e-12:
            raise MeasurementError(
                "hue %d at %.4g lx is censored - the slider was at %d %%, so "
                "it says \"at least\" %.4f in log output - and making the "
                "series non-decreasing would pull it down to %.4f. A bound "
                "cannot be averaged away: the brightest rounds contradict a "
                "darker one, and that needs another sitting rather than a "
                "projection. Check the lower bound at %.4g lx against the "
                "levels below it"
                % (hue, round_.lux, observation.percent, observed[position],
                   projected[position], round_.lux))


NON_MONOTONE_NOTE = (
    "A brighter room asking for less light is not something anybody wants, "
    "and it is what these two sittings say to each other. The grid the "
    "evaluator reads has been projected onto the nearest non-decreasing one "
    "(see `monotonic`), but the contradiction is reported here from the "
    "*observed* values, because a profile that only showed the repaired "
    "numbers would hide the one thing a reader needs before trusting it. The "
    "real remedy is another sitting at these levels.")


def _diagnostics(levels):
    """Where the measurements contradict themselves, said out loud.

    Read off the **observed** fields, never the projected ones. The projection
    exists to stop a contradiction reaching the LEDs; it must not also stop it
    reaching the reader.
    """
    white = []
    for low, high in zip(levels, levels[1:]):
        if high["whiteLogOutputObserved"] < low["whiteLogOutputObserved"]:
            white.append({
                "fromLux": low["lux"], "toLux": high["lux"],
                "fromLogOutput": low["whiteLogOutputObserved"],
                "toLogOutput": high["whiteLogOutputObserved"],
            })

    hues = []
    for position, hue in enumerate(HUE_KNOTS):
        def target(level):
            return level["residuals"][position]["targetLogOutputObserved"]
        for low, high in zip(levels, levels[1:]):
            if target(high) < target(low):
                hues.append({
                    "hue": hue,
                    "fromLux": low["lux"], "toLux": high["lux"],
                    "fromTarget": _round_number(target(low)),
                    "toTarget": _round_number(target(high)),
                    "fromPercent": low["residuals"][position]["percent"],
                    "toPercent": high["residuals"][position]["percent"],
                })

    return {
        "nonMonotoneWhite": white,
        "nonMonotoneHues": hues,
        "monotone": not white and not hues,
        "note": NON_MONOTONE_NOTE,
    }


def _place_pairs(measurements, rounds, include_targeted):
    """Which pairs sit on which grid level, and which cannot sit anywhere.

    A pair needs a six-hue round at its own ambient level: the grid knot it
    would join is `whiteBaseline(level) + residual`, and without a round there
    is no white baseline at that level to add it to. Such a pair is kept and
    reported as validation-only rather than given a baseline borrowed from a
    neighbouring level, which would be inventing the very thing it lacks.
    """
    placed, orphans = {}, []
    for pair in measurements.pairs:
        position = next((i for i, round_ in enumerate(rounds)
                         if _close(round_.log_lux, pair.log_lux)), None)
        if position is None:
            orphans.append({
                "roundId": pair.round_id, "lux": pair.lux, "hue": pair.hue,
                "reason": ("no six-hue round at %g lx, so there is no white "
                           "baseline at that level for this residual to join. "
                           "Kept for validation only" % pair.lux),
            })
        elif include_targeted:
            placed.setdefault(position, []).append(pair)
        else:
            orphans.append({
                "roundId": pair.round_id, "lux": pair.lux, "hue": pair.hue,
                "reason": "validation only: include_targeted is off",
            })
    return placed, orphans


def _targeted_block(measurements, rounds, observed_white, by_hue,
                    pairs_by_level, orphans, include_targeted):
    """Every targeted pair against the round it repeats, and the white choice.

    Nothing here is read by the evaluator. It is the evidence for two
    decisions - that the pairs' residuals were averaged in, and that their
    whites were not - written where a reader will find it.
    """
    rows, pooled = [], []
    for pair in measurements.pairs:
        position = next((i for i, round_ in enumerate(rounds)
                         if _close(round_.log_lux, pair.log_lux)), None)
        row = {
            "roundId": pair.round_id,
            "family": pair.family,
            "lux": pair.lux,
            "hue": pair.hue,
            "targetedWhitePercent": pair.anchor.percent,
            "targetedColourPercent": pair.colour.percent,
            "targetedResidual": _round_number(pair.residual),
            "usedAsReplicate": bool(include_targeted and position is not None),
        }
        if position is None:
            row.update(roundResidual=None, differenceDecades=None,
                       roundWhitePercents=[], roundColourPercent=None)
        else:
            round_ = rounds[position]
            observation = by_hue[position][pair.hue]
            round_residual = residual_of(round_, observation)
            row.update(
                roundResidual=_round_number(round_residual),
                differenceDecades=_round_number(pair.residual - round_residual),
                roundWhitePercents=[a.percent for a in round_.anchors],
                roundColourPercent=observation.percent)
            pooled.append({
                "lux": pair.lux,
                "roundWhiteLogOutput": _round_number(observed_white[position]),
                "targetedWhiteLogOutput": _round_number(pair.anchor.log_output),
                # What the baseline would move by if this anchor were pooled
                # into it with the round's own. Not applied; see the note.
                "wouldMoveDecades": _round_number(
                    (observed_white[position] * len(round_.anchors)
                     + pair.anchor.log_output) / (len(round_.anchors) + 1.0)
                    - observed_white[position]),
            })
        rows.append(row)

    return {
        "families": sorted(measurements.families),
        "pairs": rows,
        "appliedToResiduals": bool(include_targeted),
        "residualNote": TARGETED_RESIDUAL_NOTE,
        "validationOnly": orphans,
        "whiteBaseline": {
            "appliedTo": "residualsOnly",
            "note": WHITE_BASELINE_NOTE,
            "ifPooled": pooled,
        },
    }


def _percent_range(percent_range):
    low, high = percent_range
    low = _whole("the bottom of the percent range", low, 0, 100)
    high = _whole("the top of the percent range", high, 0, 100)
    if low >= high:
        raise MeasurementError(
            "the percent range is %d..%d; the ends have to be apart, or the "
            "curve is a constant" % (low, high))
    return low, high


def _is_censored(observation, max_percent):
    """At the ceiling of the range being used now, or flagged in the record.

    Both, because the two answer different questions. The flag is what the
    observer wrote down; the comparison is what the range in front of us says.
    A record taken against a 100 % ceiling and read back against a 90 % one has
    more censored points than it knows about.
    """
    return bool(observation.censored) or observation.percent >= max_percent


def _supersedes(measurements):
    """Which round the excluded points are answered by, if any.

    Named rather than described: "superseded" with nothing after it is a claim
    the reader cannot check.
    """
    if not measurements.excluded:
        return ""
    covered = [e["lux"] for e in measurements.excluded]
    middle = sum(covered) / float(len(covered))
    nearest = min(measurements.rounds,
                  key=lambda r: abs(math.log10(r.lux) - math.log10(middle)))
    return nearest.round_id


def _profile_id(measurements, levels):
    """A name that says what this is of, and is the same for the same data."""
    return "%s-%dlevels-%dhues" % (measurements.stack_id, len(levels),
                                   len(HUE_KNOTS))


# --------------------------------------------------------------------------
# The checksum
#
# Over a **canonical** payload - sorted keys, no incidental whitespace, the
# checksum field itself removed - so that a profile pretty-printed by one tool
# and minified by another still verifies. What it protects against is a value
# changing, which is the only kind of difference that matters here.
# --------------------------------------------------------------------------

def canonical_bytes(profile):
    """The payload as the bytes the checksum is taken over."""
    payload = {key: value for key, value in _mapping_or_refuse(
        "the profile", profile).items() if key != "checksum"}
    return json.dumps(payload, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False, allow_nan=False).encode("utf-8")


def profile_checksum(profile):
    return hashlib.sha256(canonical_bytes(profile)).hexdigest()


def verify_profile(profile):
    """Whether the profile is the one that was built. No exception on 'no'."""
    profile = _mapping_or_refuse("the profile", profile)
    checksum = profile.get("checksum")
    if not isinstance(checksum, dict):
        return False
    if checksum.get("algorithm") != "sha256":
        return False
    return checksum.get("value") == profile_checksum(profile)


# ==========================================================================
# Evaluating one
# ==========================================================================

class Evaluation(object):
    """What the profile asks for here, and what that answer rests on.

    Three separate things can be wrong with an answer and they are reported
    separately, because the remedies differ:

    * `lux_clamped` - the room is outside the range anybody measured, so this
      is the nearest measured level and not a prediction;
    * `bound` - the grid corners behind it include a "at least this much"
      observation, so the real answer is this or brighter;
    * `limited` - the colour cannot emit what was asked for at any percentage
      in the range, which is the gamut and not a fault.
    """

    __slots__ = ("percent", "target", "white", "residual", "log_lux",
                 "limited", "bound", "lux_clamped", "hue", "sat", "lux",
                 "log_output")

    def __init__(self, percent, target, white, residual, log_lux, limited,
                 bound, lux_clamped, hue, sat, lux, log_output):
        self.percent = percent
        self.target = target
        self.white = white
        self.residual = residual
        self.log_lux = log_lux
        self.limited = limited
        self.bound = bound
        self.lux_clamped = lux_clamped
        self.hue = hue
        self.sat = sat
        self.lux = lux
        # What that percentage really emits, which is not the target: percent
        # is an integer and the drive table is a staircase.
        self.log_output = log_output

    @property
    def attainable(self):
        """Whether the target was reached, rather than run out of slider."""
        return self.limited is None

    @property
    def exact(self):
        """Whether nothing about this answer is a bound or a clamp."""
        return self.bound is None and self.lux_clamped is None

    def as_dict(self):
        return {
            "lux": self.lux,
            "logLux": self.log_lux,
            "hue": self.hue,
            "sat": self.sat,
            "percent": self.percent,
            "whiteLogOutput": self.white,
            "logOutputResidual": self.residual,
            "targetLogOutput": self.target,
            "achievedLogOutput": self.log_output,
            "limited": self.limited,
            "bound": self.bound,
            "luxClamped": self.lux_clamped,
            "attainable": self.attainable,
            "exact": self.exact,
        }

    def __repr__(self):
        return "Evaluation(%.4g lx, hue=%d, sat=%d -> %d%%%s%s%s)" % (
            self.lux, self.hue, self.sat, self.percent,
            ", clamped %s" % self.lux_clamped if self.lux_clamped else "",
            ", %s bound" % self.bound if self.bound else "",
            ", %s" % self.limited if self.limited else "")


def _checked_profile(profile):
    """A profile this code may act on, or a refusal naming what is wrong."""
    profile = _mapping_or_refuse("the profile", profile)
    version = profile.get("schemaVersion")
    if version != SCHEMA_VERSION:
        raise MeasurementError(
            "this profile is schema %r and this code reads schema %d. A field "
            "may have changed meaning; guessing which is how a stored record "
            "gets misread" % (version, SCHEMA_VERSION))
    if profile.get("modelId") != MODEL_ID:
        raise MeasurementError(
            "this profile is model %r and this code evaluates %r. Two models "
            "may share every field name and mean different things by them"
            % (profile.get("modelId"), MODEL_ID))
    if not verify_profile(profile):
        raise MeasurementError(
            "the profile checksum does not match its contents: it has been "
            "edited since it was built, and which value moved is exactly what "
            "a checksum cannot say")
    levels = _sequence_or_refuse("the profile levels", profile.get("levels", []))
    if len(levels) < 2:
        raise MeasurementError(
            "a profile needs at least two ambient levels to interpolate "
            "between; this one has %d" % len(levels))
    return profile


def _drive_from(profile):
    """The hardware model the profile was built against, out of the profile.

    Carried rather than looked up: a profile evaluated against a different
    drive table is being evaluated against a different lamp, and the caller
    who has to remember to pass the right file will one day not.
    """
    source = _mapping_or_refuse("the profile source", profile.get("source", {}))
    try:
        return cl.DriveTable(source.get("driveLevels"),
                             source.get("driveResponse"),
                             assumed=bool(source.get("driveAssumed")),
                             source=source.get("driveSource"))
    except ValueError as problem:
        raise _refuse(problem)


def _weights_from(profile):
    source = _mapping_or_refuse("the profile source", profile.get("source", {}))
    try:
        return cl._check_weights(source.get("photopicWeights",
                                            cl.PHOTOPIC_WEIGHTS))
    except ValueError as problem:
        raise _refuse(problem)


def _bracket(levels, log_lux):
    """The two levels this ambient sits between, and how far along it is.

    Clamped rather than extrapolated: a straight line run out past the last
    measurement is a claim nobody made, and this grid is short of exactly the
    conditions its ends are at.
    """
    xs = [level["logLux"] for level in levels]
    if log_lux <= xs[0]:
        return levels[0], levels[0], 0.0, ("below" if log_lux < xs[0] else None)
    if log_lux >= xs[-1]:
        return levels[-1], levels[-1], 0.0, ("above" if log_lux > xs[-1] else None)
    for low, high in zip(levels, levels[1:]):
        if low["logLux"] <= log_lux <= high["logLux"]:
            span = high["logLux"] - low["logLux"]
            return low, high, (log_lux - low["logLux"]) / span, None
    return levels[-1], levels[-1], 0.0, None                # unreachable


def _hue_segment(hue):
    """Which pair of knots a hue falls between, and how far along.

    Cyclic: 330 sits half way from the last knot back to the first, over the
    wrap. That segment has no knot at its far end in the stored order, which
    is exactly why it is the one that gets written wrong.
    """
    span = HUE_PERIOD // len(HUE_KNOTS)
    position = (hue % HUE_PERIOD) / float(span)
    index = int(math.floor(position)) % len(HUE_KNOTS)
    return index, (index + 1) % len(HUE_KNOTS), position - math.floor(position)


def _residual_at(level, low_index, high_index, fraction):
    """One level's residual at a hue between two of its knots, with its bound."""
    low = level["residuals"][low_index]
    high = level["residuals"][high_index]
    residual = (low["logOutputResidual"]
                + (high["logOutputResidual"] - low["logOutputResidual"]) * fraction)
    # A corner only taints the answer if it is actually being read: at
    # fraction 0 the far knot contributes nothing, and saying "at least" on
    # its account would be reporting a dependency that is not there.
    bound = ((low["bound"] if fraction < 1.0 else None)
             or (high["bound"] if fraction > 0.0 else None))
    return residual, bound


def evaluate(profile, lux, hue, sat, drive=None, weights=None):
    """The slider percentage this profile asks for, and what it rests on."""
    profile = _checked_profile(profile)
    levels = profile["levels"]

    hue = _whole("hue", hue, 0, HUE_PERIOD - 1)
    sat = _whole("saturation", sat, 0, 100)
    try:
        lux = _number("lux", lux)
    except ValueError as problem:
        raise _refuse(problem)
    if lux <= 0.0:
        raise MeasurementError("%r lx: light is positive" % (lux,))

    if drive is None:
        drive = _drive_from(profile)
    if weights is None:
        weights = _weights_from(profile)

    # On the profile's own axis, not on a freshly computed one. The stored
    # levels are rounded to `ROUND_DECIMALS` so the checksum is a statement
    # about the data rather than about the libm that read it; comparing an
    # unrounded value against them puts 0.02 lx a fraction of a decade *below*
    # the level it is, and the answer comes back clamped at exactly the point
    # it should not be.
    log_lux = _round_number(cl.log_lux(lux))
    low_level, high_level, along, clamped = _bracket(levels, log_lux)
    white = (low_level["whiteLogOutput"]
             + (high_level["whiteLogOutput"] - low_level["whiteLogOutput"]) * along)

    low_hue, high_hue, fraction = _hue_segment(hue)
    residual_low, bound_low = _residual_at(low_level, low_hue, high_hue, fraction)
    residual_high, bound_high = _residual_at(high_level, low_hue, high_hue, fraction)
    residual = residual_low + (residual_high - residual_low) * along
    bound = (bound_low if along < 1.0 else None) or (bound_high if along > 0.0 else None)

    # The fade, declared in the profile and applied here. Exactly zero at
    # sat=0 - which is what makes white the same answer whatever hue is stored
    # beside it - and exactly the whole residual at sat=100.
    fade = _fade(profile, sat)
    residual *= fade
    if fade == 0.0:
        # Nothing of the colour grid reached the answer, so nothing about the
        # colour grid limits it. Reporting a bound here would be describing a
        # dependency that was multiplied away.
        residual, bound = 0.0, None

    target = white + residual
    found = cl.percent_for_output(target, hue, sat, weights, drive,
                                  profile["percentRange"]["min"],
                                  profile["percentRange"]["max"])
    return Evaluation(percent=found.percent, target=target, white=white,
                      residual=residual, log_lux=log_lux,
                      limited=found.limited, bound=bound, lux_clamped=clamped,
                      hue=hue, sat=sat, lux=lux, log_output=found.log_output)


def _fade(profile, sat):
    """How much of the colour residual applies at this saturation."""
    fade = _mapping_or_refuse("the saturation fade", profile.get("satFade", {}))
    kind = fade.get("kind")
    if kind != "linear":
        raise MeasurementError(
            "this profile fades the colour residual by %r and this code knows "
            "only 'linear'" % (kind,))
    zero = fade.get("zeroAtSat", WHITE_SAT)
    full = fade.get("fullAtSat", COLOUR_SAT)
    if full == zero:
        raise MeasurementError("the saturation fade has no span: %r to %r"
                               % (zero, full))
    if sat <= zero:
        return 0.0
    if sat >= full:
        return 1.0
    return (sat - zero) / float(full - zero)


# ==========================================================================
# Cross-validation: what the grid is worth on a level it has not seen
# ==========================================================================
#
# A grid interpolator reproduces its own knots, so an in-sample residual says
# almost nothing - the only thing it measures is the drift correction and the
# quantisation of an integer percentage. The question worth asking is what
# happens at an ambient level the profile was never given, which is what a
# clock in a real room is doing every evening.
#
# **The endpoints cannot answer it.** Hold out the darkest level and every
# prediction below it is clamped to the next one up; hold out the brightest and
# the same happens above. That measures the clamp, not the model, so those
# folds are run and reported and **not scored**. What is left is the clean
# interior levels, and "clean" excludes the censored round for the separate
# reason that its rows are bounds rather than equalities.

ACCEPTANCE_RMS = 6.0
ACCEPTANCE_MAX = 10.0


def _sub_measurements(measurements, without):
    """The same record with one *level* removed, for one fold.

    The level, not the round: a targeted pair at the held-out level is another
    observation of the same lighting condition, and leaving it in the training
    set would let the fold predict hue 240 at a level where it has just been
    told the answer. The fold would look excellent and measure nothing.
    """
    return Measurements(
        [r for r in measurements.rounds if r is not without],
        measurements.excluded, measurements.stack_id, measurements.source,
        measurements.note,
        pairs=[p for p in measurements.pairs
               if not _close(p.log_lux, without.log_lux)],
        families=measurements.families)


def _fold_rows(profile, round_, max_percent, pairs=()):
    """Every observation at the held-out level, predicted without that level.

    The round's six colours, and any targeted pair taken at the same level -
    each labelled with `kind`, because a replicate and the round it repeats
    are different observations and averaging them into one row would hide the
    disagreement this whole exercise exists to measure.
    """
    rows = []
    for kind, round_id, observation in (
            [("round", round_.round_id, o) for o in round_.colours]
            + [("targeted", p.round_id, p.colour) for p in pairs]):
        found = evaluate(profile, observation.lux, observation.hue,
                         observation.sat)
        censored = _is_censored(observation, max_percent)
        rows.append({
            "kind": kind,
            "sourceRoundId": round_id,
            "hue": observation.hue,
            "sat": observation.sat,
            "index": observation.index,
            "observed": observation.percent,
            "predicted": found.percent,
            # An error only where there is an equality to take one from. A
            # censored row carries `atLeast` and `honoured` instead.
            "error": None if censored else found.percent - observation.percent,
            "censored": censored,
            "atLeast": observation.percent if censored else None,
            "honoured": (found.percent >= observation.percent
                         if censored else None),
            "bound": found.bound,
            "luxClamped": found.lux_clamped,
            "limited": found.limited,
            "scored": not censored,
        })
    return rows


def _stats(errors):
    if not errors:
        return {"count": 0, "rms": None, "max": None}
    return {
        "count": len(errors),
        "rms": (sum(e * e for e in errors) / float(len(errors))) ** 0.5,
        "max": max(abs(e) for e in errors),
    }


def cross_validate(measurements, drive, weights=cl.PHOTOPIC_WEIGHTS,
                   percent_range=DEFAULT_PERCENT_RANGE,
                   goal_rms=ACCEPTANCE_RMS, goal_max=ACCEPTANCE_MAX,
                   profile=None, include_targeted=True, sensitivity=True):
    """Leave one clean interior level out, and report what it costs.

    Errors are in **slider percentage points**, which is neither the model's
    coordinate nor a flattering one - it is what somebody would notice on the
    wall, and it is the unit the goal is written in.

    `profile` is the document being judged, and the caller passes the one it
    is going to write. Building a second one here instead looks equivalent and
    is not: `profileId` and `generator` sit inside the checksummed payload, so
    a rebuild with either of them different produces a different checksum, and
    the report ends up naming a profile nobody has. The whole job of
    `profileChecksum` is to say which document these numbers are about.
    """
    low, high = _percent_range(percent_range)
    rounds = measurements.rounds
    if len(rounds) < 3:
        raise MeasurementError(
            "cross-validation needs at least three levels - one to hold out "
            "and two to interpolate between - and this record has %d"
            % len(rounds))

    if profile is None:
        full = build_profile(measurements, drive, weights, (low, high),
                             include_targeted=include_targeted)
    else:
        full = _checked_profile(profile)
        if (full["percentRange"]["min"], full["percentRange"]["max"]) != (low, high):
            raise MeasurementError(
                "the profile's range is %d..%d %% and this run was asked for "
                "%d..%d %%: which observations count as censored depends on "
                "where the ceiling is, so the two would score different "
                "experiments"
                % (full["percentRange"]["min"], full["percentRange"]["max"],
                   low, high))
    darkest, brightest = rounds[0].log_lux, rounds[-1].log_lux

    # --- in sample, for the censored inequalities above all ----------------
    pairs_at = {}
    for pair in measurements.pairs:
        position = next((i for i, r in enumerate(rounds)
                         if _close(r.log_lux, pair.log_lux)), None)
        if position is not None:
            pairs_at.setdefault(position, []).append(pair)

    in_sample, censored_rows, in_sample_folds = [], [], []
    for position, round_ in enumerate(rounds):
        rows = _fold_rows(full, round_, high, pairs_at.get(position, ()))
        for row in rows:
            if row["scored"]:
                in_sample.append(row["error"])
            else:
                censored_rows.append(row)
        # Kept grouped as well as pooled, so the CSV can write the in-sample
        # pass with the same columns as the folds and the two can be read
        # against each other row for row.
        in_sample_folds.append(dict(
            _stats([row["error"] for row in rows if row["scored"]]),
            roundId=round_.round_id, lux=round_.lux, rows=rows))

    folds = []
    for position, round_ in enumerate(rounds):
        interior = darkest < round_.log_lux < brightest
        clean = not round_.censored
        reason = ""
        if not interior:
            reason = ("holding out the %s level leaves every prediction "
                      "clamped against it, which measures the clamp rather "
                      "than the model"
                      % ("darkest" if round_.log_lux <= darkest else "brightest"))
        elif not clean:
            reason = ("this round's rows are lower bounds, not equalities, so "
                      "an error against them is not an error")

        trained = _sub_measurements(measurements, round_)
        held_pairs = pairs_at.get(position, ())
        rows = _fold_rows(build_profile(trained, drive, weights, (low, high),
                                        include_targeted=include_targeted),
                          round_, high, held_pairs)
        errors = [row["error"] for row in rows if row["scored"]]
        folds.append(dict(_stats(errors),
                          roundId=round_.round_id,
                          lux=round_.lux,
                          scored=bool(interior and clean),
                          reason=reason,
                          trainedOnLux=[r.lux for r in trained.rounds],
                          trainedOnTargetedLux=[p.lux for p in trained.pairs],
                          heldOutTargeted=[p.round_id for p in held_pairs],
                          rows=rows))

    scored_rows = [row for fold in folds if fold["scored"]
                   for row in fold["rows"] if row["scored"]]
    scored_errors = [row["error"] for row in scored_rows]
    overall = _stats(scored_errors)

    by_hue = []
    for hue in HUE_KNOTS:
        errors = [row["error"] for row in scored_rows if row["hue"] == hue]
        by_hue.append(dict(_stats(errors), hue=hue))

    worst = max(by_hue, key=lambda entry: (entry["max"] or 0, entry["rms"] or 0))
    without = _stats([row["error"] for row in scored_rows
                      if row["hue"] != worst["hue"]])

    rms_met = overall["rms"] is not None and overall["rms"] <= goal_rms
    max_met = overall["max"] is not None and overall["max"] <= goal_max

    return {
        "schemaVersion": SCHEMA_VERSION,
        "modelId": MODEL_ID,
        "stackId": measurements.stack_id,
        "profileId": full["profileId"],
        "profileChecksum": full["checksum"]["value"],
        "unit": "slider percentage points",
        "percentRange": {"min": low, "max": high},
        "inSample": dict(_stats(in_sample), censored={
            "count": len(censored_rows),
            "honoured": sum(1 for row in censored_rows if row["honoured"]),
            "violations": [row for row in censored_rows if not row["honoured"]],
            "rows": censored_rows,
            "note": ("A censored observation says \"at least this much\". It "
                     "is checked as an inequality - the prediction must reach "
                     "it or exceed it - and never as an equality to take a "
                     "residual from."),
        }),
        "inSampleFolds": in_sample_folds,
        "folds": folds,
        "targeted": full["targeted"],
        "sensitivity": _sensitivity(measurements, drive, weights, low, high,
                                    goal_rms, goal_max, include_targeted,
                                    overall, rms_met, max_met, sensitivity),
        "byHue": by_hue,
        "worstHueExcluded": dict(without, hue=worst["hue"]),
        "acceptance": {
            "goalRms": goal_rms,
            "goalMax": goal_max,
            "folds": sum(1 for fold in folds if fold["scored"]),
            "count": overall["count"],
            "rms": overall["rms"],
            "max": overall["max"],
            "rmsMet": rms_met,
            "maxMet": max_met,
            "met": rms_met and max_met,
            "censoredHonoured": not [row for row in censored_rows
                                     if not row["honoured"]],
            "note": _acceptance_note(overall, worst, without, rms_met, max_met,
                                     goal_rms, goal_max),
        },
        "limitations": _limitations(full, folds, worst, without),
    }


SENSITIVITY_NOTE = (
    "The targeted pairs can be averaged into the hue's knot as replicates, or "
    "held out of the grid and used only to check it. Both are reported. "
    "Averaging is what this profile does, because a replicate is an "
    "observation and dropping observations to improve a number is the one "
    "thing this exercise may not do - and because the residual is "
    "demonstrably the transferable quantity, agreeing to 0.014 decades at "
    "0.02 lx and exactly at 1.9 lx. Neither choice meets the goals, so "
    "neither can be a threshold being chased.")


def _sensitivity(measurements, drive, weights, low, high, goal_rms, goal_max,
                 include_targeted, overall, rms_met, max_met, wanted):
    """The same run with the other choice about the targeted pairs.

    Reported rather than argued: a decision about which observations count is
    exactly the kind that should come with the number it would have produced.
    """
    if not wanted or not measurements.pairs:
        return {}
    other = cross_validate(measurements, drive, weights, (low, high),
                           goal_rms, goal_max,
                           include_targeted=not include_targeted,
                           sensitivity=False)["acceptance"]
    applied = {"rms": overall["rms"], "max": overall["max"],
               "count": overall["count"], "rmsMet": rms_met,
               "maxMet": max_met, "met": rms_met and max_met}
    alternative = {key: other[key] for key in
                   ("rms", "max", "count", "rmsMet", "maxMet", "met")}
    return {
        "targetedAsReplicates": {
            "applied": applied if include_targeted else alternative,
            "validationOnly": alternative if include_targeted else applied,
            "note": SENSITIVITY_NOTE,
        },
    }


def _acceptance_note(overall, worst, without, rms_met, max_met, goal_rms,
                     goal_max):
    if rms_met and max_met:
        return ("Both goals met: RMS %.2f of %.1f and worst %d of %d over %d "
                "held-out observations."
                % (overall["rms"], goal_rms, overall["max"], goal_max,
                   overall["count"]))
    parts = ["RMS %.2f against a goal of %.1f (%s)"
             % (overall["rms"], goal_rms, "met" if rms_met else "not met"),
             "worst %d against a goal of %d (%s)"
             % (overall["max"], goal_max, "met" if max_met else "not met")]
    parts.append(
        "The worst errors are all hue %d; leaving that one hue out gives RMS "
        "%.2f and worst %d, which meets both. The shortfall is one colour's "
        "observations contradicting each other, not the shape of the model - "
        "see `diagnostics` in the profile. The remedy is another sitting at "
        "hue %d, not a different fit."
        % (worst["hue"], without["rms"], without["max"], worst["hue"]))
    return " ".join(parts)


def _disagreements(profile):
    """Targeted pairs that disagree with the round they repeat, worst first."""
    rows = [row for row in profile["targeted"]["pairs"]
            if row["differenceDecades"] is not None]
    rows.sort(key=lambda row: -abs(row["differenceDecades"]))
    return rows


def _limitations(profile, folds, worst, without):
    """What a reader has to know before quoting any of these numbers."""
    limitations = [
        "Every observation is a subjective visual setting by one person on one "
        "clock, in the units of an integer slider. It is a preference, not a "
        "photometric measurement, and it is not transferable to another "
        "optical stack.",
        "Saturation is never interpolated from data: every colour observation "
        "is at sat=100 and every anchor at sat=0, so the linear fade between "
        "them is a declared assumption and nothing here tests it.",
        "The grid reproduces its own knots, so the in-sample numbers measure "
        "the drift correction and the integer quantisation and little else. "
        "Only the held-out folds say anything about a level nobody measured.",
        "Neither endpoint level can be cross-validated, so nothing here says "
        "what the profile is worth outside 0.02..10 lx, where it clamps.",
    ]
    unscored = [fold for fold in folds if not fold["scored"]]
    if unscored:
        limitations.append(
            "%d of %d folds are unscored (%s): %s."
            % (len(unscored), len(folds),
               ", ".join("%g lx" % fold["lux"] for fold in unscored),
               "; ".join(fold["reason"] for fold in unscored)))
    diagnostics = profile["diagnostics"]
    if not diagnostics["monotone"]:
        limitations.append(
            "The measurements are not monotone in ambient light: %s. The grid "
            "the evaluator reads has been projected onto the nearest "
            "non-decreasing one, which is a repair and not a measurement - "
            "the observed values are kept beside it."
            % "; ".join(
                ["white between %g and %g lx" % (entry["fromLux"], entry["toLux"])
                 for entry in diagnostics["nonMonotoneWhite"]]
                + ["hue %d between %g and %g lx"
                   % (entry["hue"], entry["fromLux"], entry["toLux"])
                   for entry in diagnostics["nonMonotoneHues"]]))
    if worst["max"] is not None and without["max"] is not None:
        limitations.append(
            "Hue %d carries the whole shortfall: RMS %.2f and worst %d against "
            "%.2f and %d for every other hue together."
            % (worst["hue"], worst["rms"], worst["max"], without["rms"],
               without["max"]))
    disagreements = _disagreements(profile)
    if disagreements:
        limitations.append(
            "The targeted repeat did not resolve hue %d, it measured more of "
            "the disagreement: %s. Two sessions differing by a quarter of the "
            "slider at one level cannot both be fitted, so the curve lands "
            "between them and both rows come back wrong. The repeat is "
            "trustworthy where the two sessions shared an adaptation state "
            "and the question is which of the two readings at %g lx to "
            "believe - which needs a third sitting, not a different fit."
            % (disagreements[0]["hue"],
               "; ".join("%g lx round %d %% against repeat %d %% (%+.4f "
                         "decades)"
                         % (row["lux"], row["roundColourPercent"],
                            row["targetedColourPercent"],
                            row["differenceDecades"])
                         for row in disagreements),
               disagreements[0]["lux"]))
    return limitations


# ==========================================================================
# Output: a table to read, a CSV to plot, and the command that makes both
# ==========================================================================

# Three different questions get a yes/no here, and conflating any two of them
# miscounts the headline figure - which is exactly what a column called `fold`
# holding a boolean, beside a `scored` that meant something narrower, invited:
#
#   censored             this row is a lower bound, not an equality
#   foldScored           the fold this row belongs to counts towards acceptance
#   countedInAcceptance  both of the above, which is the number that is quoted
#
# The last one is derivable from the first two and is written out all the same,
# because the alternative is every reader re-deriving the one number the report
# leads with.
CSV_COLUMNS = ["kind", "source", "roundId", "sourceRoundId", "lux", "hue",
               "sat", "observed", "predicted", "error", "censored", "atLeast",
               "honoured", "bound", "luxClamped", "limited", "foldScored",
               "countedInAcceptance"]


def _csv_cell(value):
    """`None` as an empty cell, booleans as 0/1, everything else as itself.

    A CSV has no null and no boolean. Writing "None" and "True" into one and
    hoping the reader guesses is how a column stops being a number.
    """
    if value is None:
        return ""
    if isinstance(value, bool):
        return 1 if value else 0
    return value


def report_to_csv(report):
    """Every predicted row, held-out and in-sample, one per line.

    No plotting dependency: this project ships stdlib scripts, and one of them
    drawing a chart would be the first thing to break on a machine nobody
    prepared. `kind` and `scored` are what a reader filters on.
    """
    import csv as _csv

    buffer = io.StringIO()
    writer = _csv.DictWriter(buffer, fieldnames=CSV_COLUMNS,
                             lineterminator="\n")
    writer.writeheader()

    def emit(kind, fold_scored, round_id, lux, row):
        writer.writerow({
            "kind": kind, "roundId": round_id, "lux": lux,
            # Which observation this row is: the six-hue round's own, or a
            # targeted pair taken at the same level. They are never merged.
            "source": row.get("kind", "round"),
            "sourceRoundId": row.get("sourceRoundId", round_id),
            "hue": row["hue"], "sat": row["sat"],
            "observed": row["observed"], "predicted": row["predicted"],
            "error": _csv_cell(row["error"]),
            "censored": _csv_cell(row["censored"]),
            "atLeast": _csv_cell(row["atLeast"]),
            "honoured": _csv_cell(row["honoured"]),
            "bound": _csv_cell(row["bound"]),
            "luxClamped": _csv_cell(row["luxClamped"]),
            "limited": _csv_cell(row["limited"]),
            "foldScored": _csv_cell(fold_scored),
            "countedInAcceptance": _csv_cell(
                bool(fold_scored) and bool(row["scored"])),
        })

    for fold in report["folds"]:
        for row in fold["rows"]:
            emit("fold", fold["scored"], fold["roundId"], fold["lux"], row)

    # The in-sample pass is written from the same fold structure so the two
    # halves of the file have the same columns and can be compared row for
    # row. None of it counts towards acceptance - a grid reproduces its own
    # knots - so `foldScored` is false on every one of these.
    for fold in report["inSampleFolds"]:
        for row in fold["rows"]:
            emit("inSample", False, fold["roundId"], fold["lux"], row)
    return buffer.getvalue()


def report_to_text(report):
    """The same thing for a person, which is a different shape."""
    acceptance = report["acceptance"]
    lines = [
        "Stack:    %s" % report["stackId"],
        "Profile:  %s" % report["profileId"],
        "Checksum: %s" % report["profileChecksum"],
        "Range:    %d..%d %%   errors in %s"
        % (report["percentRange"]["min"], report["percentRange"]["max"],
           report["unit"]),
        "",
        "In sample   n=%d  RMS %.2f  worst %d   (a grid reproduces its own "
        "knots; this measures little)"
        % (report["inSample"]["count"], report["inSample"]["rms"],
           report["inSample"]["max"]),
        "Censored    %d of %d honoured as \"at least\" inequalities"
        % (report["inSample"]["censored"]["honoured"],
           report["inSample"]["censored"]["count"]),
        "",
        "Leave one level out:",
    ]
    for fold in report["folds"]:
        if fold["count"]:
            lines.append("  %-46s %-8s n=%d  RMS %6.2f  worst %3d%s"
                         % (fold["roundId"], "scored" if fold["scored"]
                            else "UNSCORED", fold["count"], fold["rms"],
                            fold["max"],
                            "" if fold["scored"] else "  <- %s" % fold["reason"]))
        else:
            lines.append("  %-46s %-8s no scorable rows  <- %s"
                         % (fold["roundId"], "UNSCORED", fold["reason"]))
    lines.append("")
    lines.append("Per hue, over the scored folds:")
    for entry in report["byHue"]:
        lines.append("  hue %3d  n=%d  RMS %6.2f  worst %3d"
                     % (entry["hue"], entry["count"], entry["rms"],
                        entry["max"]))
    without = report["worstHueExcluded"]
    lines.append("  without hue %d: n=%d  RMS %6.2f  worst %3d"
                 % (without["hue"], without["count"], without["rms"],
                    without["max"]))
    lines.append("")
    lines.append("Acceptance  RMS %.2f / %.1f  %s      worst %d / %d  %s"
                 % (acceptance["rms"], acceptance["goalRms"],
                    "MET" if acceptance["rmsMet"] else "NOT MET",
                    acceptance["max"], acceptance["goalMax"],
                    "MET" if acceptance["maxMet"] else "NOT MET"))
    lines.append("            %s" % acceptance["note"])
    lines.append("")
    lines.append("Limitations:")
    for entry in report["limitations"]:
        lines.append("  - %s" % entry)
    return "\n".join(lines)


def _write(path, text):
    try:
        with io.open(path, "w", encoding="utf-8") as handle:
            handle.write(text)
    except OSError as problem:
        raise MeasurementError("cannot write %s: %s"
                               % (path, problem.strerror or problem))


def _write_json(path, document):
    """Sorted keys and a trailing newline, so two runs differ only if the
    numbers do and a diff is worth reading."""
    _write(path, json.dumps(document, indent=1, sort_keys=True,
                            ensure_ascii=False, allow_nan=False) + "\n")


def _parse_range(text):
    pieces = [piece.strip() for piece in text.split(",")]
    if len(pieces) != 2:
        raise MeasurementError(
            "--percent-range wants two whole numbers separated by a comma, "
            "such as 20,100 - not %r" % text)
    try:
        return int(pieces[0]), int(pieces[1])
    except ValueError:
        raise MeasurementError(
            "--percent-range wants two whole numbers separated by a comma, "
            "such as 20,100 - not %r" % text)


# ==========================================================================
# The compact runtime profile - what the clock is actually given
# ==========================================================================
#
# The reviewed profile is 43 KB of provenance: every observation, every
# replicate, every note about how a round was taken. That is what makes it
# reviewable and it is exactly what a firmware must not carry - a 3.5 MB
# partition already holds a web UI, and an evaluator that walks a laboratory
# notebook to find two numbers has the notebook in RAM while it does it.
#
# So a second document is **derived** from it: the same model, only the numbers
# the evaluator reads. Three properties make that safe.
#
# * **Derived deterministically.** Same reviewed profile in, same bytes out, so
#   the file can be committed and a diff only ever shows a value that moved.
#   The same rule as `zones.json` and the icons: generated, committed, and
#   never fetched or built on demand.
# * **Self-verifying, in a layout a small machine can check.** The clock cannot
#   re-canonicalise a parsed document - ArduinoJson does not sort keys and
#   `float` is not `double` - so it hashes a *substring* of the file instead.
#   That is only honest if the substring is at a fixed place, hence the layout
#   below, which is asserted from both sides.
# * **The same answers.** `evaluate_runtime()` is the parity reference for the
#   firmware and is tested against `evaluate()` at the knots, between them,
#   across the hue seam and outside the measured range.
#
# What is deliberately kept is the profile's own **status**. The reviewed
# HUE240 repeat misses its acceptance goal; that is provenance, not a reason to
# withhold the model, and a file that quietly dropped it would be claiming an
# accuracy nobody measured.
# --------------------------------------------------------------------------

RUNTIME_SCHEMA = 1

# The file is the canonical serialisation of `{"checksum": ..., "payload": ...}`
# and "checksum" sorts before "payload", so the payload is a contiguous
# substring running from a fixed offset to the closing brace. The firmware
# hashes exactly that and refuses a file whose head is not this.
RUNTIME_HEAD = '{"checksum":{"algorithm":"sha256","value":"'
RUNTIME_CHECKSUM_AT = len(RUNTIME_HEAD)
RUNTIME_MARK = '"},"payload":'
RUNTIME_PAYLOAD_AT = RUNTIME_CHECKSUM_AT + 64 + len(RUNTIME_MARK)

# What the derived file is for, in the file, because somebody will open it
# without this module beside them.
RUNTIME_NOTE = (
    "Derived from the reviewed factory profile by scripts/factory_luminance.py "
    "and shipped in the filesystem image. The reviewed profile keeps the "
    "provenance; this keeps the arithmetic.")

MONOTONE_STATUS_NOTE = (
    "Whether the stored grid rises everywhere with ambient light. It does not "
    "have to: the isotonic step imposes monotonicity per hue on the target and "
    "what is left is reported rather than smoothed away, because a grid edited "
    "until it looked right would be a model of the editing. The clock uses it "
    "either way - a segment that dips asks for a little less light in a "
    "slightly brighter room, which is a small wrong answer and not an unsafe "
    "one.")


def _finite(name, value):
    """A number that arithmetic may touch. JSON has no NaN, so one arrives as
    a string or a null, and both used to reach the inversion and come back as
    a percentage with nothing behind it."""
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise MeasurementError("%s is %s; a number was expected"
                               % (name, cl._shape_of(value)))
    if not math.isfinite(value):
        raise MeasurementError("%s is %r, which is not a number this can use"
                               % (name, value))
    return float(value)


def _exact_whole(name, value, low, high):
    """An integer that really is one, inside the range it has to be in.

    `_whole` accepts 20.0 for 20, which is right where a *measurement* file is
    being read: a spreadsheet writes floats and refusing them would refuse real
    records. It is wrong here. The firmware reads the same field through
    ArduinoJson's `is<int>()`, which a JSON float fails, so a value this side
    accepted and that side refused would be a profile that passes the
    generator's own tests and then will not load on a clock - the one kind of
    disagreement a shared contract exists to prevent.
    """
    if isinstance(value, bool) or not isinstance(value, int):
        raise MeasurementError(
            "%s is %s; a whole number was expected, and written as one - a "
            "JSON float is not an integer to the reader on the clock"
            % (name, cl._shape_of(value)))
    if not low <= value <= high:
        raise MeasurementError("%s is %r; it has to be %d..%d"
                               % (name, value, low, high))
    return value


def _exact_flag(name, value):
    """`true` or `false`, and not the several things that convert to one.

    Deliberately not `_flag`, which defaults a missing value to False: a flag
    that is absent from a *generated* document is a document this generator did
    not write, and reading it as "no" would invent the answer rather than
    refuse the file.
    """
    if not isinstance(value, bool):
        raise MeasurementError("%s is %s; true or false was expected"
                               % (name, cl._shape_of(value)))
    return value


def _nonempty_text(name, value):
    """A string with something in it. Both callers key something by it."""
    if not isinstance(value, str) or not value:
        raise MeasurementError(
            "%s is %s; a name with something in it was expected"
            % (name, cl._shape_of(value)))
    return value


def _worst_hue(evaluation):
    worst = evaluation.get("worstHueExcluded")
    if isinstance(worst, dict):
        worst = worst.get("hue")
    return worst if isinstance(worst, int) else None


def _status(profile, evaluation=None):
    """What is known to be wrong with this profile, carried with it."""
    diagnostics = profile.get("diagnostics", {}) or {}
    source = profile.get("source", {}) or {}
    status = {
        "monotone": bool(diagnostics.get("monotone", True)),
        "monotoneNote": MONOTONE_STATUS_NOTE,
        "supersededBy": _text("supersededBy", source.get("supersededBy", "")),
        "driveAssumed": bool(source.get("driveAssumed")),
        "note": RUNTIME_NOTE,
    }
    if evaluation is None:
        return status

    evaluation = _mapping_or_refuse("the evaluation", evaluation)
    stated = evaluation.get("profileChecksum")
    if stated != profile.get("checksum", {}).get("value"):
        raise MeasurementError(
            "this evaluation is of profile %r and this profile is %r. An "
            "acceptance figure from another build would be a number about a "
            "document nobody is shipping"
            % (stated, profile.get("checksum", {}).get("value")))

    acceptance = _mapping_or_refuse("the acceptance",
                                    evaluation.get("acceptance", {}))
    status.update({
        "acceptanceMet": bool(acceptance.get("met")),
        "rms": _round_number(_finite("the acceptance RMS",
                                     acceptance.get("rms", 0.0))),
        "maxError": acceptance.get("max"),
        "goalRms": acceptance.get("goalRms"),
        "goalMax": acceptance.get("goalMax"),
        "worstHue": _worst_hue(evaluation),
        "acceptanceNote": _text("the acceptance note", acceptance.get("note", "")),
    })
    return status


def runtime_profile(profile, evaluation=None):
    """The reviewed profile reduced to what the evaluator reads.

    Refuses a profile it may not act on first, so a compact file can never be
    made out of one the full evaluator would not touch.
    """
    profile = _checked_profile(profile)

    levels = []
    for level in profile["levels"]:
        residuals = _sequence_or_refuse("a level's residuals",
                                        level.get("residuals", []))
        if len(residuals) != len(HUE_KNOTS):
            raise MeasurementError(
                "a level carries %d hue knots and the model has %d"
                % (len(residuals), len(HUE_KNOTS)))
        levels.append({
            "logLux": _round_number(_finite("a level's logLux",
                                            level.get("logLux"))),
            "white": _round_number(_finite("a level's white output",
                                           level.get("whiteLogOutput"))),
            "residuals": [
                _round_number(_finite("a residual", one.get("logOutputResidual")))
                for one in residuals],
            # The word becomes a flag. There is one kind of bound here - "at
            # least this much", from an observation that ran out of slider -
            # and a string in the file would invite a second kind nothing
            # reads. A real boolean rather than 1 and 0, because that is what
            # `load_runtime` and the firmware's `flag()` require: a number here
            # would be a document this generator writes and the clock refuses.
            "bounds": [bool(one.get("bound")) for one in residuals],
            "censored": bool(level.get("censored")),
        })

    source = _mapping_or_refuse("the profile source", profile.get("source", {}))
    fade = _mapping_or_refuse("the saturation fade", profile.get("satFade", {}))
    return {
        "runtimeSchema": RUNTIME_SCHEMA,
        "schemaVersion": profile["schemaVersion"],
        "modelId": profile["modelId"],
        "profileId": profile.get("profileId", ""),
        "stackId": profile.get("stackId", ""),
        # The reviewed document this was derived from, so a clock can say which
        # measurement it is running rather than only that it has one.
        "sourceChecksum": profile["checksum"]["value"],
        "percentRange": {"min": profile["percentRange"]["min"],
                         "max": profile["percentRange"]["max"]},
        "hueKnots": list(HUE_KNOTS),
        "huePeriod": HUE_PERIOD,
        "satFade": {"kind": fade.get("kind"),
                    "zeroAtSat": fade.get("zeroAtSat", WHITE_SAT),
                    "fullAtSat": fade.get("fullAtSat", COLOUR_SAT)},
        # The hardware model, carried rather than looked up: a profile
        # evaluated against a different drive table is being evaluated against
        # a different lamp.
        "drive": {"levels": list(source.get("driveLevels", [])),
                  "response": list(source.get("driveResponse", []))},
        "weights": list(source.get("photopicWeights", cl.PHOTOPIC_WEIGHTS)),
        "levels": levels,
        "status": _status(profile, evaluation),
    }


def _canonical(payload):
    return json.dumps(payload, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False, allow_nan=False)


def reseal_runtime(payload):
    """A payload wrapped in the file layout, with its checksum recomputed."""
    body = _canonical(payload)
    digest = hashlib.sha256(body.encode("utf-8")).hexdigest()
    return RUNTIME_HEAD + digest + RUNTIME_MARK + body + "}"


def runtime_text(profile, evaluation=None):
    """The file, as the bytes that go into the filesystem image."""
    return reseal_runtime(runtime_profile(profile, evaluation=evaluation))


def verify_runtime(text):
    """Whether the file is the one that was generated. No exception on 'no'."""
    if not isinstance(text, str) or not text.startswith(RUNTIME_HEAD):
        return False
    if not text.endswith("}") or len(text) <= RUNTIME_PAYLOAD_AT + 1:
        return False
    if text[RUNTIME_CHECKSUM_AT + 64:RUNTIME_PAYLOAD_AT] != RUNTIME_MARK:
        return False
    stated = text[RUNTIME_CHECKSUM_AT:RUNTIME_CHECKSUM_AT + 64]
    body = text[RUNTIME_PAYLOAD_AT:-1]
    return hashlib.sha256(body.encode("utf-8")).hexdigest() == stated


# --------------------------------------------------------------------------
# Whether the grid rises with light, measured on the grid
#
# `status.monotone` is a finding about the *observations* and the evaluator
# must not read it as a statement about the numbers it was handed: the two are
# written by different code at different times, and a status field is exactly
# what an edit leaves behind while moving a value.
#
# So it is computed here, in the coordinate that matters - the target log
# output, white plus residual, at every hue knot and on the white line itself.
# The size of the worst dip decides which of two different things it is:
#
# **And the answer for the reviewed profile is that it does not dip at all.**
# That is worth stating, because `status.monotone` on it is `false` and reads
# like the opposite. The two are about different numbers: the diagnostics
# report the *observed* values, where hue 240 falls a quarter of a decade
# between 0.5 lx and 0.15 lx, and the isotonic step then pools the levels those
# disagreements sit between - which is why levels three and four of the shipped
# grid carry the same white output and the same residual at four of six knots.
# A firmware that took `status.monotone` for a statement about its own grid
# would report a fault it does not have; one that took it for a licence would
# accept a grid that really does fall.
#
# So a dip in the *installed* grid is a **fault**, not a finding: this
# generator cannot produce one, so a file carrying one was not produced by this
# generator. The tolerance below is for the last bit of a rounded double and
# nothing else - it is not a budget.
# --------------------------------------------------------------------------

GRID_MAX_DIP = 1e-9


def grid_dip(payload):
    """The largest fall in target log output between adjacent ambient levels."""
    levels = payload["levels"]
    knots = len(payload["hueKnots"])
    worst = 0.0
    for low, high in zip(levels, levels[1:]):
        # The white line on its own, because a dip can sit there with every
        # residual flat - a different fault from one in a single hue, and one
        # a check that only walked the colour rows would miss.
        worst = max(worst, low["white"] - high["white"])
        for knot in range(knots):
            fell = ((low["white"] + low["residuals"][knot])
                    - (high["white"] + high["residuals"][knot]))
            worst = max(worst, fell)
    return worst


def _knots_or_refuse(payload):
    knots = _sequence_or_refuse("the hue knots", payload.get("hueKnots"))
    period = payload.get("huePeriod")
    if not isinstance(period, int) or period <= 0:
        raise MeasurementError("the hue period is %r; it has to be a positive "
                               "whole number of degrees" % (period,))
    if len(knots) < 2:
        raise MeasurementError("%d hue knots; two is the least that can be "
                               "interpolated between" % len(knots))
    if period % len(knots):
        raise MeasurementError(
            "%d hue knots do not divide a period of %d, and the segment "
            "arithmetic assumes they do" % (len(knots), period))
    span = period // len(knots)
    for index, knot in enumerate(knots):
        if knot != index * span:
            raise MeasurementError(
                "hue knot %d is %r and evenly spaced knots would put it at %d"
                % (index, knot, index * span))
    return knots, period


def load_runtime(text):
    """A compact profile this code may act on, or a refusal naming what is wrong.

    Every check here has a counterpart in the firmware, and the order is the
    same: nothing is parsed as a number before the document has been shown to
    be the one that was generated.
    """
    if not verify_runtime(text):
        raise MeasurementError(
            "the runtime profile does not match its own checksum: it has been "
            "edited or truncated since it was generated, and which byte moved "
            "is exactly what a checksum cannot say")

    try:
        document = json.loads(text)
    except ValueError as problem:
        raise _refuse(problem)
    payload = _mapping_or_refuse("the runtime payload", document.get("payload"))

    if payload.get("runtimeSchema") != RUNTIME_SCHEMA:
        raise MeasurementError(
            "this runtime profile is runtime schema %r and this code reads %d"
            % (payload.get("runtimeSchema"), RUNTIME_SCHEMA))
    if payload.get("schemaVersion") != SCHEMA_VERSION:
        raise MeasurementError(
            "this runtime profile is schema %r and this code reads schema %d"
            % (payload.get("schemaVersion"), SCHEMA_VERSION))
    if payload.get("modelId") != MODEL_ID:
        raise MeasurementError(
            "this runtime profile is model %r and this code evaluates %r"
            % (payload.get("modelId"), MODEL_ID))
    if not _text("the stack id", payload.get("stackId", "")):
        raise MeasurementError(
            "this runtime profile names no optical stack. The coefficients "
            "belong to one clock's diffuser and mask, and a profile that does "
            "not say which is one nobody can match against a clock")

    # The identity. A checksum says nobody edited the document since it was
    # written; it says nothing about the document having been a profile, and
    # anybody holding `reseal_runtime` can produce one that verifies perfectly
    # and carries nonsense. Both of these key something: `profileId` is what
    # the read-out names when somebody asks which measurement their clock is
    # running, and `sourceChecksum` is what the stored colour corrections are
    # filed under - so an empty one makes every correction ever made look as
    # though it belonged to this profile, whatever it was really learned on.
    # That is the one failure the checksum-keyed record exists to prevent.
    _nonempty_text("the profile id", payload.get("profileId"))
    _nonempty_text("the source checksum", payload.get("sourceChecksum"))

    percent = _mapping_or_refuse("the percent range", payload.get("percentRange"))
    low = _whole("the bottom of the percent range", percent.get("min"), 0, 100)
    high = _whole("the top of the percent range", percent.get("max"), 0, 100)
    if low >= high:
        raise MeasurementError(
            "the regulated range is empty: %d up to %d" % (low, high))

    knots, _period = _knots_or_refuse(payload)

    fade = _mapping_or_refuse("the saturation fade", payload.get("satFade"))
    if fade.get("kind") != "linear":
        raise MeasurementError(
            "this profile fades the colour residual by %r and this code knows "
            "only 'linear'" % (fade.get("kind"),))
    zero_at = _exact_whole("the bottom of the saturation fade",
                           fade.get("zeroAtSat"), 0, 100)
    full_at = _exact_whole("the top of the saturation fade",
                           fade.get("fullAtSat"), 0, 100)
    # Strictly below, not merely different. Reversed, the colour residual would
    # be whole on a white face - where there is no colour to correct - and
    # absent at full saturation, where the whole correction lives: the model
    # meaning the opposite of itself. Refused rather than silently flipped,
    # because a file reinterpreted on the way in is one nobody can reason about
    # afterwards. FactoryProfile::valid() refuses the same shape.
    if zero_at >= full_at:
        raise MeasurementError(
            "the saturation fade runs from %d to %d. It has to rise: zero at "
            "the white end and whole at the coloured one, or the correction is "
            "applied exactly where there is no colour to correct"
            % (zero_at, full_at))

    levels = _sequence_or_refuse("the profile levels", payload.get("levels"))
    if len(levels) < 2:
        raise MeasurementError(
            "a profile needs at least two ambient levels to interpolate "
            "between; this one has %d" % len(levels))
    previous = None
    for index, level in enumerate(levels):
        level = _mapping_or_refuse("level %d" % index, level)
        log_lux = _finite("level %d's logLux" % index, level.get("logLux"))
        # Strictly ascending, because the bracket search walks the pairs and
        # divides by the span. A repeated or descending level makes it answer
        # something plausible for the wrong bracket, which is worse than an
        # error - and it is the one shape of "not monotone" that is a fault
        # rather than a finding. See `status.monotone` for the other.
        if previous is not None and log_lux <= previous:
            raise MeasurementError(
                "the ambient levels must ascend strictly, and %r does not come "
                "above %r" % (log_lux, previous))
        previous = log_lux
        _finite("level %d's white output" % index, level.get("white"))
        residuals = _sequence_or_refuse("level %d's residuals" % index,
                                        level.get("residuals"))
        bounds = _sequence_or_refuse("level %d's bounds" % index,
                                     level.get("bounds"))
        if len(residuals) != len(knots) or len(bounds) != len(knots):
            raise MeasurementError(
                "level %d carries %d residuals and %d bounds for %d hue knots"
                % (index, len(residuals), len(bounds), len(knots)))
        for one in residuals:
            _finite("a residual of level %d" % index, one)
        # A bound is a boolean and every one of them is looked at, not only the
        # first. Read as "anything truthy" the string "no" would be a bound and
        # every answer touching that corner would say "at least this much"
        # without cause; read as a number, a 2 would be a bound with no meaning
        # attached to it. The firmware reads these through `flag()`, which
        # refuses 1 and 0 as firmly as it refuses "lower".
        for knot, one in enumerate(bounds):
            _exact_flag("the bound at level %d knot %d" % (index, knot), one)
        _exact_flag("whether level %d is censored" % index,
                    level.get("censored"))

    drive = _mapping_or_refuse("the drive table", payload.get("drive"))
    try:
        cl.DriveTable(drive.get("levels"), drive.get("response"),
                      source="the runtime profile")
        cl._check_weights(payload.get("weights"))
    except ValueError as problem:
        raise _refuse(problem)

    # Measured on the grid, never read out of `status`. See grid_dip().
    dip = grid_dip(payload)
    if dip > GRID_MAX_DIP:
        raise MeasurementError(
            "the grid falls by %.6f decades between two ambient levels. The "
            "isotonic step cannot leave one, so this file was not built by "
            "this generator - and a clock regulating on it would get dimmer as "
            "the sun came up" % dip)

    return payload


def _runtime_drive(payload):
    drive = payload["drive"]
    return cl.DriveTable(drive["levels"], drive["response"],
                         source="the runtime profile")


def _runtime_bracket(levels, log_lux):
    xs = [level["logLux"] for level in levels]
    if log_lux <= xs[0]:
        return 0, 0, 0.0, ("below" if log_lux < xs[0] else None)
    if log_lux >= xs[-1]:
        last = len(levels) - 1
        return last, last, 0.0, ("above" if log_lux > xs[-1] else None)
    for index in range(len(levels) - 1):
        if xs[index] <= log_lux <= xs[index + 1]:
            span = xs[index + 1] - xs[index]
            return index, index + 1, (log_lux - xs[index]) / span, None
    return len(levels) - 1, len(levels) - 1, 0.0, None      # unreachable


def _runtime_residual(level, low, high, fraction):
    residual = (level["residuals"][low]
                + (level["residuals"][high] - level["residuals"][low]) * fraction)
    bound = ((level["bounds"][low] if fraction < 1.0 else 0)
             or (level["bounds"][high] if fraction > 0.0 else 0))
    return residual, bool(bound)


def evaluate_runtime(payload, lux, hue, sat, drive=None, weights=None):
    """The compact profile's answer - the parity reference for the firmware.

    Deliberately a separate function rather than `evaluate()` given a smaller
    document: the firmware reads *this* shape, and a reference that walks the
    reviewed one would be checking the wrong thing.
    """
    knots = payload["hueKnots"]
    period = payload["huePeriod"]
    levels = payload["levels"]

    hue = _whole("hue", hue, 0, period - 1)
    sat = _whole("saturation", sat, 0, 100)
    try:
        lux = _number("lux", lux)
    except ValueError as problem:
        raise _refuse(problem)
    if lux <= 0.0:
        raise MeasurementError("%r lx: light is positive" % (lux,))

    if drive is None:
        drive = _runtime_drive(payload)
    if weights is None:
        weights = cl._check_weights(payload["weights"])

    log_lux = _round_number(cl.log_lux(lux))
    low_index, high_index, along, clamped = _runtime_bracket(levels, log_lux)
    low_level, high_level = levels[low_index], levels[high_index]
    white = low_level["white"] + (high_level["white"] - low_level["white"]) * along

    span = period // len(knots)
    position = (hue % period) / float(span)
    low_hue = int(math.floor(position)) % len(knots)
    high_hue = (low_hue + 1) % len(knots)
    fraction = position - math.floor(position)

    residual_low, bound_low = _runtime_residual(low_level, low_hue, high_hue,
                                                fraction)
    residual_high, bound_high = _runtime_residual(high_level, low_hue, high_hue,
                                                  fraction)
    residual = residual_low + (residual_high - residual_low) * along
    bound = ((bound_low if along < 1.0 else False)
             or (bound_high if along > 0.0 else False))

    fade = payload["satFade"]
    zero, full = fade["zeroAtSat"], fade["fullAtSat"]
    if sat <= zero:
        scale = 0.0
    elif sat >= full:
        scale = 1.0
    else:
        scale = (sat - zero) / float(full - zero)
    residual *= scale
    if scale == 0.0:
        # Nothing of the colour grid reached the answer, so nothing about the
        # colour grid limits it.
        residual, bound = 0.0, False

    target = white + residual
    found = cl.percent_for_output(target, hue, sat, weights, drive,
                                  payload["percentRange"]["min"],
                                  payload["percentRange"]["max"])
    return Evaluation(percent=found.percent, target=target, white=white,
                      residual=residual, log_lux=log_lux,
                      limited=found.limited, bound="lower" if bound else None,
                      lux_clamped=clamped, hue=hue, sat=sat, lux=lux,
                      log_output=found.log_output)


# --------------------------------------------------------------------------
# The golden vectors
#
# The firmware evaluator has to agree with this one to the integer percentage,
# and there is no environment where both can be run in the same process. So the
# agreement is written down: a committed file of cases, checked against this
# module by the Python tests and against the firmware by a host build of the
# pure C++ evaluator.
#
# The cases are chosen for the places the two implementations can differ, not
# for coverage in the abstract: the knots themselves, halfway between them, the
# hue seam at 300 -> 0 where the far knot is not the next one in store order,
# both ends of the saturation fade, ambient outside the measured range where
# the answer is a clamp, and the corners the reviewed profile marks as bounds.
# --------------------------------------------------------------------------

def runtime_vectors(payload):
    """The cases, and what this module makes of each."""
    knots = payload["hueKnots"]
    luxes = [round(10.0 ** level["logLux"], 6) for level in payload["levels"]]

    asked = []
    for lux in luxes:                       # every knot, both fade ends
        for hue in knots:
            asked.append((lux, hue, 100))
            asked.append((lux, hue, 0))
    for lux in (0.03, 0.11, 0.4, 1.9, 4.5, 8.0):     # between the levels
        for hue in (15, 45, 91, 137, 210, 259, 331):
            asked.append((lux, hue, 100))
    for hue in (300, 305, 315, 330, 345, 355, 359, 0):   # the seam
        asked.append((0.5, hue, 100))
    for sat in (1, 17, 33, 50, 66, 99):                  # down the fade
        asked.append((0.5, 240, sat))
        asked.append((0.5, 60, sat))
    for lux in (0.001, 0.005, 0.019, 10.5, 90.0, 5000.0):   # outside it
        asked.append((lux, 240, 100))
        asked.append((lux, 0, 100))

    cases = []
    for lux, hue, sat in asked:
        found = evaluate_runtime(payload, lux, hue, sat)
        cases.append({
            "lux": lux, "hue": hue, "sat": sat,
            "percent": found.percent,
            "target": _round_number(found.target),
            "white": _round_number(found.white),
            "residual": _round_number(found.residual),
            "limited": found.limited or "",
            "bound": found.bound or "",
            "clamped": found.lux_clamped or "",
        })
    return {"sourceChecksum": payload["sourceChecksum"],
            "profileId": payload["profileId"],
            "note": "Generated by scripts/factory_luminance.py vectors. The "
                    "contract between the Python model and the firmware "
                    "evaluator; neither may be changed without the other.",
            "cases": cases}


# --------------------------------------------------------------------------
# The same two things, flat
#
# The host build of `src/FactoryProfile.cpp` has no JSON parser and must not
# grow one: the point of that file is that it is pure arithmetic a desktop
# compiler can take on its own. So the profile and the vectors are also written
# as whitespace-separated numbers, which `ifstream >> double` reads in a dozen
# lines. They are derived from the JSON above by the tests, so the two cannot
# drift.
#
# The codes are small integers rather than words for the same reason.
# --------------------------------------------------------------------------

LIMITED_CODES = {"": 0, "ceiling": 1, "floor": 2}
CLAMP_CODES = {"": 0, "below": 1, "above": 2}


def _repr17(value):
    """A double that reads back as the same double. `repr` is not enough on
    every Python this may run on, and a fixture that loses a bit turns a parity
    test into a tolerance test."""
    return "%.17g" % float(value)


def runtime_flat(payload):
    """The profile as numbers, in the order the host harness reads them."""
    lines = ["# generated by scripts/factory_luminance.py vectors - do not edit",
             "%d %d" % (payload["percentRange"]["min"],
                        payload["percentRange"]["max"]),
             "%d %d" % (payload["satFade"]["zeroAtSat"],
                        payload["satFade"]["fullAtSat"]),
             "%d %d" % (payload["huePeriod"], len(payload["hueKnots"])),
             " ".join(str(knot) for knot in payload["hueKnots"]),
             str(len(payload["levels"]))]
    for level in payload["levels"]:
        row = [_repr17(level["logLux"]), _repr17(level["white"])]
        row += [_repr17(one) for one in level["residuals"]]
        row += [str(int(one)) for one in level["bounds"]]
        row.append("1" if level["censored"] else "0")
        lines.append(" ".join(row))
    drive = payload["drive"]
    lines.append(str(len(drive["levels"])))
    lines.append(" ".join(str(one) for one in drive["levels"]))
    lines.append(" ".join(_repr17(one) for one in drive["response"]))
    lines.append(" ".join(_repr17(one) for one in payload["weights"]))
    # The worst dip, so the host harness can compare its own reading of the
    # grid against this one. A dip found by one implementation and not the
    # other means the two are walking different grids.
    lines.append(_repr17(grid_dip(payload)))
    return "\n".join(lines) + "\n"


def vectors_flat(vectors):
    """The cases as numbers: lux hue sat percent target limited clamped bound."""
    lines = ["# generated by scripts/factory_luminance.py vectors - do not edit",
             str(len(vectors["cases"]))]
    for case in vectors["cases"]:
        lines.append(" ".join([
            _repr17(case["lux"]), str(case["hue"]), str(case["sat"]),
            str(case["percent"]), _repr17(case["target"]),
            str(LIMITED_CODES[case["limited"]]),
            str(CLAMP_CODES[case["clamped"]]),
            "1" if case["bound"] else "0"]))
    return "\n".join(lines) + "\n"


def main(argv=None):
    import argparse

    parser = argparse.ArgumentParser(
        prog="factory_luminance",
        description="Build a factory colour profile from saved measurements "
                    "and say what it is worth on a level it has not seen. "
                    "Reads files; talks to no clock.")
    sub = parser.add_subparsers(dest="command")

    build = sub.add_parser("build", help="write a profile and an evaluation")
    build.add_argument("--measurements", required=True,
                       help="the measurement record as a saved file")
    build.add_argument("--coupling", required=True,
                       help="the coupling record from `lab.py calibrate`, for "
                            "the measured drive response. Not optional: an "
                            "assumed proportional response is wrong by up to "
                            "a factor of three at the settings this clock runs")
    build.add_argument("--profile", required=True,
                       help="where to write the profile JSON")
    build.add_argument("--report", help="where to write the evaluation JSON")
    build.add_argument("--csv", help="where to write the evaluation rows")
    build.add_argument("--percent-range", default="20,100",
                       help="the regulated range, default 20,100")
    build.add_argument("--profile-id", help="override the generated profile id")
    build.add_argument("--json", action="store_true",
                       help="print the evaluation as JSON instead of a table")

    check = sub.add_parser("evaluate", help="ask a written profile for one answer")
    check.add_argument("--profile", required=True)
    check.add_argument("--lux", required=True)
    check.add_argument("--hue", required=True)
    check.add_argument("--sat", required=True)

    # The two derived files. Both are generated, committed and shipped rather
    # than built on demand - the same rule as `zones.json` and the icons - so
    # neither a web build nor a firmware build needs Python.
    here = os.path.dirname(os.path.abspath(__file__))
    runtime = sub.add_parser(
        "runtime", help="write the compact profile the clock is given")
    runtime.add_argument("--profile", default=os.path.join(
        here, "..", "artifacts", "2026-08-27-hue240-repeat-profile.json"))
    runtime.add_argument("--evaluation", default=os.path.join(
        here, "..", "artifacts", "2026-08-27-hue240-repeat-evaluation.json"),
        help="the evaluation of that same profile, whose acceptance figures "
             "travel with it as status. Must be of that profile: an "
             "acceptance number from another build would describe a document "
             "nobody is shipping")
    runtime.add_argument("--out", default=os.path.join(
        here, "..", "web", "public", "factory-luminance.json"),
        help="web/public is copied into the LittleFS image by Vite, which is "
             "what keeps emptyOutDir from eating it")

    vectors = sub.add_parser(
        "vectors", help="write the golden vectors the firmware is checked against")
    vectors.add_argument("--runtime", default=os.path.join(
        here, "..", "web", "public", "factory-luminance.json"))
    vectors.add_argument("--out", default=os.path.join(
        here, "..", "tests", "golden", "factory_luminance_vectors.json"))

    args = parser.parse_args(argv)
    if args.command not in ("build", "evaluate", "runtime", "vectors"):
        parser.print_help()
        return 2

    try:
        if args.command == "runtime":
            profile = cl.load_json(args.profile)
            report = (cl.load_json(args.evaluation) if args.evaluation else None)
            text = runtime_text(profile, evaluation=report)
            _write(args.out, text)
            print("%s: %d bytes, profile %s"
                  % (args.out, len(text.encode("utf-8")),
                     profile.get("profileId")))
            return 0

        if args.command == "vectors":
            with open(args.runtime, "r") as handle:
                payload = load_runtime(handle.read())
            found = runtime_vectors(payload)
            _write(args.out, json.dumps(found, indent=1, sort_keys=True) + "\n")
            # The flat pair beside it, for the host build of the firmware
            # evaluator - which has no JSON parser and must not grow one.
            base = os.path.dirname(args.out)
            _write(os.path.join(base, "factory_profile.txt"), runtime_flat(payload))
            _write(os.path.join(base, "factory_vectors.txt"), vectors_flat(found))
            print("%s: %d cases" % (args.out, len(found["cases"])))
            return 0
    except (OSError, ValueError) as problem:
        parser.error(str(problem))

    # Everything past here reads files somebody typed the names of. A traceback
    # would be this script blaming the user for its own lack of an error path,
    # so each refusal comes back the way argparse's own do: one line, exit 2,
    # nothing on stdout.
    try:
        if args.command == "evaluate":
            found = evaluate(cl.load_json(args.profile),
                             _number("--lux", _numeric(args.lux, "--lux")),
                             _numeric(args.hue, "--hue"),
                             _numeric(args.sat, "--sat"))
            print(json.dumps(found.as_dict(), indent=1, sort_keys=True))
            return 0

        low, high = _parse_range(args.percent_range)
        coupling = cl.load_json(args.coupling)
        table = cl.DriveTable.from_record(coupling, source=args.coupling)
        measurements = load_measurements(cl.load_json(args.measurements),
                                         drive=table)
        built = build_profile(measurements, table, percent_range=(low, high),
                              profile_id=args.profile_id,
                              generator="scripts/factory_luminance.py build")
        # The profile that is about to be written, not a fresh one built to
        # the same recipe: `profileId` and `generator` are inside the
        # checksummed payload, so a rebuild here would leave the report
        # naming a document that never reaches the disk.
        report = cross_validate(measurements, table, percent_range=(low, high),
                                profile=built)

        _write_json(args.profile, built)
        if args.report:
            _write_json(args.report, report)
        if args.csv:
            _write(args.csv, report_to_csv(report))
    except (OSError, ValueError) as problem:
        # `MeasurementError` is a `ValueError`, and so is every refusal
        # `colour_luminance` raises and `json.JSONDecodeError`.
        parser.error(str(problem))

    print(json.dumps(report, indent=1, sort_keys=True) if args.json
          else report_to_text(report))
    return 0


def _numeric(text, option):
    try:
        return float(text)
    except (TypeError, ValueError):
        raise MeasurementError("%s wants a number, not %r" % (option, text))


if __name__ == "__main__":
    raise SystemExit(main())
