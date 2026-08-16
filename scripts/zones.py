"""
zones
Generates web/public/zones.json, the list the timezone picker offers.

Every compiled zone file of the IANA database ends with a POSIX TZ string that
states the currently recurring rule - "CET-1CEST,M3.5.0,M10.5.0/3" for Berlin.
That is precisely the model the clock stores: two changeover rules of month,
week, weekday, hour and offset. So the list is nothing more than a mapping of
zone name to that one line, and the web UI turns it into the fourteen fields.

The data comes from the `tzdata` package on PyPI rather than from the system,
so a build on Windows and a build in CI produce the same file:

    pip install tzdata
    python scripts/zones.py

The result is committed. That keeps `npm run build` free of a Python
dependency, and it makes a tzdata release show up as a reviewable diff of the
zones whose rules actually moved. Nothing is fetched at runtime: the clock has
to work on a network without any internet at all, and 16 KB in a 3.5 MB
filesystem partition is not worth a second code path for.

@author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
@version  2.1
@created  16.8.2026
@updated  16.8.2026
"""

import json
import os
import re
import sys

try:
    import tzdata
    import zoneinfo
except ImportError:
    sys.exit("The tzdata package is missing - run: pip install tzdata")

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUTPUT = os.path.join(PROJECT_DIR, "web", "public", "zones.json")

# The continents the picker groups by. Everything else the database carries is
# an alias kept for compatibility - "US/Eastern", "Poland", "Etc/GMT+5" - and
# only makes the list longer without naming a place anyone looks for.
#
# Note that this is deliberately wider than zone1970.tab, which is the usual
# choice: recent tzdata releases turned Amsterdam, Oslo, Stockholm, Copenhagen
# and Luxembourg into links to Europe/Berlin and dropped them from that table.
# Their rules are identical, but a list without Amsterdam looks broken to
# someone who lives there.
AREAS = (
    "Africa", "America", "Antarctica", "Arctic", "Asia", "Atlantic",
    "Australia", "Europe", "Indian", "Pacific",
)

# Plain UTC carries no continent but is a reasonable thing to want.
EXTRA = ("UTC",)

# Mm.w.d[/h] - the only rule form the clock can represent. The two alternatives
# POSIX allows (Jn and n, counting days from the start of the year) have no
# week-and-weekday to store and would need a different firmware model.
RULE = re.compile(r"^M(\d{1,2})\.(\d)\.(\d)(?:/(-?\d{1,2})(?::(\d{2}))?(?::\d{2})?)?$")


def posix_rule(root, name):
    """The POSIX TZ line of a zone, read from the footer of its TZif file."""
    path = os.path.join(root, *name.split("/"))
    if not os.path.isfile(path):
        return None

    with open(path, "rb") as handle:
        data = handle.read()

    # Only TZif version 2 and up carry the footer; version 1 files end with the
    # transition table. The version is a single byte behind the magic.
    if not data.startswith(b"TZif") or data[4:5] == b"\x00":
        return None

    return data.rstrip(b"\n").rsplit(b"\n", 1)[-1].decode("ascii")


def unsupported(rule):
    """Why the clock cannot follow this rule exactly, or None when it can."""
    parts = rule.split(",")
    if len(parts) == 1:
        return None  # no daylight saving at all, nothing to schedule
    if len(parts) != 3:
        return "unexpected shape"

    for part in parts[1:]:
        match = RULE.match(part)
        if not match:
            return "day-of-year rule (%s)" % part
        hour, minute = match.group(4), match.group(5)
        if hour is not None and not (0 <= int(hour) <= 23):
            return "changeover at hour %s" % hour
        if minute and minute != "00":
            return "changeover at %s minutes past" % minute
    return None


def main():
    root = os.path.join(os.path.dirname(tzdata.__file__), "zoneinfo")
    zoneinfo.reset_tzpath([root])

    wanted = sorted(
        name for name in zoneinfo.available_timezones()
        if name.split("/")[0] in AREAS or name in EXTRA
    )

    zones = {}
    skipped = []
    notes = []
    for name in wanted:
        rule = posix_rule(root, name)
        if not rule:
            skipped.append(name)
            continue
        zones[name] = rule
        reason = unsupported(rule)
        if reason:
            notes.append((name, rule, reason))

    document = {"tzdata": tzdata.IANA_VERSION, "zones": zones}

    os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
    # Compact and sorted, with no build timestamp: regenerating without a
    # tzdata change then leaves the file byte for byte identical, so a diff
    # only ever shows a rule that really moved.
    with open(OUTPUT, "w", encoding="ascii", newline="\n") as out:
        json.dump(document, out, separators=(",", ":"), sort_keys=True)
        out.write("\n")

    size = os.path.getsize(OUTPUT)
    print("tzdata %s -> %s" % (tzdata.IANA_VERSION, os.path.relpath(OUTPUT, PROJECT_DIR)))
    print("%d zones, %d bytes" % (len(zones), size))

    if skipped:
        print("no POSIX rule, left out: %s" % ", ".join(skipped))

    # These are followed as closely as two rules allow, which is not exactly.
    # Worth printing rather than hiding: the list is short and stable, and a
    # new entry means a zone has moved outside what the clock can express.
    if notes:
        print("\napproximated (%d):" % len(notes))
        for name, rule, reason in notes:
            print("  %-26s %-46s %s" % (name, rule, reason))


if __name__ == "__main__":
    main()
