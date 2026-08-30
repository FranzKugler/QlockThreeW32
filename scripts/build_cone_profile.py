# -*- coding: utf-8 -*-
"""Builds web/public/factory-luminance.json for the cone-plus-harmonic-nose
model (FACTORY_SCHEMA 2) from the real, drift-corrected measurement record.

This is deliberately a small, standalone builder rather than a new code path
bolted onto factory_luminance.py's `build_profile()`/`runtime_profile()`,
which describe the six-knot residual grid the cone replaced end to end -
`load_measurements()`, `residual_of()` and `white_baseline()` are the only
things borrowed from there, because they read the record and drift-correct it
and neither of those changed. Migrating the rest of that pipeline (the CLI,
the cross-validation, the isotonic grid construction) to the parametric shape
is real, separate work, left for when the model this file writes has been
run on the clock it describes.

    python3 scripts/build_cone_profile.py
"""
import hashlib
import json
import math
import sys

sys.path.insert(0, "/workspace/scripts")
import colour_luminance as cl
import factory_luminance as fl

ROOT = "/workspace"
MEASUREMENTS_PATH = ROOT + "/tests/fixtures/2026-08-27-manual-colour-brightness-points-with-hue240-repeat.json"
COUPLING_PATH = ROOT + "/tests/fixtures/2026-08-26-current-diffuser-before-mask-coupling.json"
OUT_PATH = ROOT + "/web/public/factory-luminance.json"

BLUE_HUE = 240
BLEND_HALF_WIDTH = 45.0     # degrees; see FactoryProfile.h for the shape
GOAL_RMS = 6.0
GOAL_MAX = 10.0
PERCENT_RANGE = (cl.LUM_MIN_PERCENT, cl.LUM_MAX_PERCENT)

RUNTIME_HEAD = '{"checksum":{"algorithm":"sha256","value":"'
RUNTIME_MARK = '"},"payload":'


def solve(rows, targets):
    n = len(rows[0])
    a = [[0.0] * n for _ in range(n)]
    b = [0.0] * n
    for row, t in zip(rows, targets):
        for i in range(n):
            b[i] += row[i] * t
            for j in range(n):
                a[i][j] += row[i] * row[j]
    m = [a[i][:] + [b[i]] for i in range(n)]
    for col in range(n):
        piv = max(range(col, n), key=lambda r: abs(m[r][col]))
        m[col], m[piv] = m[piv], m[col]
        pivot = m[col][col]
        for j in range(col, n + 1):
            m[col][j] /= pivot
        for r in range(n):
            if r != col:
                factor = m[r][col]
                for j in range(col, n + 1):
                    m[r][j] -= factor * m[col][j]
    return [m[i][n] for i in range(n)]


def nose_features(hue):
    rad = math.radians(hue)
    return [1.0, math.cos(rad), math.sin(rad)]


def build(out_path=OUT_PATH):
    coupling = json.load(open(COUPLING_PATH))
    drive = cl.DriveTable.from_record(coupling, source="coupling.json")
    measurement_bytes = open(MEASUREMENTS_PATH, "rb").read()
    doc = json.loads(measurement_bytes)
    meas = fl.load_measurements(doc, drive=drive, weights=cl.PHOTOPIC_WEIGHTS)

    rows = []  # (hue, sat, log_lux, residual, censored)
    for r in meas.rounds:
        for o in r.colours:
            rows.append((o.hue, o.sat, o.log_lux, fl.residual_of(r, o), o.censored))
    for p in meas.pairs:
        rows.append((p.hue, p.colour.sat, p.log_lux, p.residual, p.censored))

    # The cone: one straight line through the white anchors of every round.
    white_xs = [r.log_lux for r in meas.rounds]
    white_zs = [fl.white_baseline(r) for r in meas.rounds]
    cone = cl.fit_line(white_xs, white_zs, keep_slope=None, min_decades=0.0001)

    # The nose: 1st-harmonic Fourier fit on the five non-blue colours,
    # censored (ceiling) points excluded - they say "at least", and least
    # squares reading that as "exactly" would drag the fit down.
    nb_rows = [(h, x, res) for h, s, x, res, c in rows if h != BLUE_HUE and not c]
    nose_coeffs = solve([nose_features(h) for h, x, res in nb_rows],
                        [res for h, x, res in nb_rows])

    # Blue's own line: its residual has a real slope in light the other five
    # do not (measured: -0.19 decades/decade against +/-0.05 for the rest),
    # so it is fitted on its own rather than folded into the wave.
    b_rows = [(x, res) for h, s, x, res, c in rows if h == BLUE_HUE and not c]
    bx = [x for x, res in b_rows]
    br = [res for x, res in b_rows]
    n = len(bx)
    mx = sum(bx) / n
    mr = sum(br) / n
    cov = sum((x - mx) * (r - mr) for x, r in zip(bx, br))
    var = sum((x - mx) ** 2 for x in bx)
    blue_slope = cov / var if var > 1e-9 else 0.0
    blue_offset = mr - blue_slope * mx

    log_lux_min = min(white_xs)
    log_lux_max = max(white_xs)

    # Honest accuracy figures, not asserted: the same evaluation every
    # earlier finding in this session used, on every un-censored observation.
    def blue_weight(hue):
        d = abs(((hue - BLUE_HUE + 180) % 360) - 180)
        if d >= BLEND_HALF_WIDTH:
            return 0.0
        return 0.5 * (1.0 + math.cos(math.pi * d / BLEND_HALF_WIDTH))

    def predict(hue, x):
        nose = sum(c * f for c, f in zip(nose_coeffs, nose_features(hue)))
        blue_line = blue_slope * x + blue_offset
        w = blue_weight(hue)
        return cone.slope * x + cone.offset + nose + w * (blue_line - nose)

    def percent_rows():
        out = []
        for r in meas.rounds:
            for o in r.colours:
                out.append((o.hue, o.sat, o.lux, o.percent, o.censored))
        for p in meas.pairs:
            out.append((p.hue, p.colour.sat, p.colour.lux, p.colour.percent, p.censored))
        return out

    errors = []
    for hue, sat, lux, percent, censored in percent_rows():
        if censored:
            continue
        x = cl.log_lux(lux)
        target = predict(hue, x)
        found = cl.percent_for_output(target, hue, sat, cl.PHOTOPIC_WEIGHTS, drive,
                                      PERCENT_RANGE[0], PERCENT_RANGE[1])
        errors.append((percent - found.percent, hue))

    rms = (sum(e * e for e, h in errors) / len(errors)) ** 0.5
    worst = max(errors, key=lambda t: abs(t[0]))

    # Whether the raw observations - not the fit - rise with light at every
    # hue. Independent of the model: log output is white + residual only
    # once fitted, but a real observation's own log output either rose or it
    # did not, and that is worth knowing even once the fit has smoothed over
    # a contradiction. Mirrors the six-knot grid's worstDip(), on points
    # rather than on a grid.
    by_hue = {}
    for h, s, x, res, c in rows:
        if c:
            continue
        by_hue.setdefault(h, []).append((x, cone.slope * x + cone.offset + res))
    observations_monotone = True
    for h, points in by_hue.items():
        points.sort()
        for (x0, z0), (x1, z1) in zip(points, points[1:]):
            if z1 < z0:
                observations_monotone = False

    source_checksum = hashlib.sha256(measurement_bytes).hexdigest()
    profile_id = "cone-plus-first-harmonic-hue-nose-%s" % meas.stack_id

    # Table 1, for ResidualStore::refit() to combine with what the owner
    # teaches: exactly the points nose_coeffs/blue_slope/blue_offset above
    # were fitted from, so a clock with nothing taught yet reproduces them
    # exactly (checked in the host test).
    points = [{"hue": h, "sat": 100, "logLux": x, "residual": res}
              for h, x, res in nb_rows]
    points += [{"hue": BLUE_HUE, "sat": 100, "logLux": x, "residual": res}
               for x, res in b_rows]

    payload = {
        "runtimeSchema": 2,
        "schemaVersion": 2,
        "modelId": "white-cone-plus-first-harmonic-hue-nose-with-blue-line",
        "profileId": profile_id,
        "stackId": meas.stack_id,
        "sourceChecksum": source_checksum,
        "huePeriod": 360,
        "cone": {
            "slope": cone.slope,
            "offset": cone.offset,
            "logLuxMin": log_lux_min,
            "logLuxMax": log_lux_max,
        },
        "nose": {
            "a0": nose_coeffs[0],
            "a1": nose_coeffs[1],
            "b1": nose_coeffs[2],
        },
        "blue": {
            "hue": BLUE_HUE,
            "slope": blue_slope,
            "offset": blue_offset,
            "blendHalfWidth": BLEND_HALF_WIDTH,
        },
        "percentRange": {"min": PERCENT_RANGE[0], "max": PERCENT_RANGE[1]},
        "satFade": {"kind": "linear", "zeroAtSat": 0, "fullAtSat": 100},
        "drive": {"levels": list(coupling["drive"]["levels"]),
                  "response": list(coupling["drive"]["response"])},
        "weights": list(cl.PHOTOPIC_WEIGHTS),
        "points": points,
        "status": {
            "monotone": observations_monotone,
            "acceptanceMet": rms <= GOAL_RMS and abs(worst[0]) <= GOAL_MAX,
            "maxError": int(round(abs(worst[0]))),
            "worstHue": worst[1],
        },
    }

    body = json.dumps(payload, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False, allow_nan=False)
    digest = hashlib.sha256(body.encode("utf-8")).hexdigest()
    text = RUNTIME_HEAD + digest + RUNTIME_MARK + body + "}"

    open(out_path, "w").write(text)
    print("cone: slope=%.4f offset=%.4f" % (cone.slope, cone.offset))
    print("nose: a0=%.4f a1=%.4f b1=%.4f (n=%d)" % (
        nose_coeffs[0], nose_coeffs[1], nose_coeffs[2], len(nb_rows)))
    print("blue: slope=%.4f offset=%.4f (n=%d)" % (blue_slope, blue_offset, n))
    print("RMS=%.2f%% worst=%.2f%% at hue=%d (n=%d)" % (rms, worst[0], worst[1], len(errors)))
    print("observationsMonotone=%s" % observations_monotone)
    print("points shipped for refit: %d" % len(points))
    print("acceptanceMet=%s (goal %.1f%%/%.1f%%)" % (
        payload["status"]["acceptanceMet"], GOAL_RMS, GOAL_MAX))
    print("written %s (%d bytes)" % (out_path, len(text)))


if __name__ == "__main__":
    build(sys.argv[1] if len(sys.argv) > 1 else OUT_PATH)
