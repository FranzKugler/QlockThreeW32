#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
panels.scad, from the panels the firmware actually renders.

The letter grid used to be written out a second time in body.scad, where
nothing checked it. The firmware verifies at every boot that the letters under
each word really spell that word (Languages::selfCheck), and a hand-copied
array in a SCAD file gets none of that - it can drift for years and only show
up as a milled panel that reads wrong. So the SCAD side stops keeping its own
copy and reads this one.

Run after changing a panel:

    python scripts/panels.py

The output is committed, the same way zones.json and the icons are: OpenSCAD
has no way to run this, and a 3D print should not depend on a Python that
happens to be installed. It carries no timestamp, so regenerating without a
change leaves the file byte-identical and a diff only ever shows a letter that
moved.

What is parsed, and why parsing beats compiling here: each language is one
`extern const Language X = { "code", "name", "locale", { ten rows }, ... }`,
and the rows take exactly two shapes - ten string literals, or ten references
into a named array in the same file, which is how the four German dialects
share one panel. Anything else raises rather than guesses.
"""

import io
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SOURCE = os.path.join(ROOT, "src", "languages")
TARGET = os.path.join(ROOT, "hardware", "Qlock250mm", "3dprint", "panels.scad")

PANEL_ROWS = 10
PANEL_COLS = 11

STRING = re.compile(r'"((?:[^"\\]|\\.)*)"')
ROW_ARRAY = re.compile(r"(\w+)\s*\[\s*PANEL_ROWS\s*\]\s*=\s*\{")
DEFINITION = re.compile(r"extern\s+const\s+Language\s+(\w+)\s*=\s*\{")
INDEXED = re.compile(r"(\w+)\s*\[\s*\d+\s*\]")


def unescape(text):
    """The panels use no escapes; anything else is a surprise worth stopping for."""
    if chr(92) in text:
        raise ValueError("escape sequence in a panel string: %r" % text)
    return text


def braced(source, opening):
    """The text inside the braces that start at `opening`, braces balanced."""
    depth = 0
    for i in range(opening, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:i]
    raise ValueError("unbalanced braces from offset %d" % opening)


def row_arrays(source):
    """The named row arrays in one file, e.g. ROWS[PANEL_ROWS] in Language_DE."""
    found = {}
    for match in ROW_ARRAY.finditer(source):
        body = braced(source, match.end() - 1)
        found[match.group(1)] = [unescape(s) for s in STRING.findall(body)]
    return found


def languages_in(path):
    """Every Language defined in one file: symbol, code, name, locale, rows."""
    source = io.open(path, encoding="utf-8").read()
    arrays = row_arrays(source)
    out = []

    for match in DEFINITION.finditer(source):
        symbol = match.group(1)
        body = braced(source, match.end() - 1)

        inner = body.index("{")
        head = STRING.findall(body[:inner])
        if len(head) != 3:
            raise ValueError("%s: expected code, name and locale, found %r" % (symbol, head))
        code, name, locale = [unescape(s) for s in head]

        rows_text = braced(body, inner)
        rows = [unescape(s) for s in STRING.findall(rows_text)]
        if not rows:
            # The German dialects point into a shared array instead.
            names = set(INDEXED.findall(rows_text))
            if len(names) != 1:
                raise ValueError("%s: cannot make out the rows from %r" % (symbol, rows_text))
            rows = arrays[names.pop()]

        if len(rows) != PANEL_ROWS:
            raise ValueError("%s: %d rows, expected %d" % (symbol, len(rows), PANEL_ROWS))
        for number, row in enumerate(rows):
            if len(row) != PANEL_COLS:
                raise ValueError(
                    "%s row %d: %d characters, expected %d - %r"
                    % (symbol, number, len(row), PANEL_COLS, row))

        out.append({"symbol": symbol, "code": code, "name": name, "locale": locale,
                    "rows": rows, "file": os.path.basename(path)})
    return out


def table_order(path):
    """The LANGUAGE_* symbols in the order of the numbers stored in NVS."""
    source = io.open(path, encoding="utf-8").read()
    body = braced(source, source.index("{", source.index("TABLE[LANGUAGE_COUNT]")))
    return re.findall(r"&\s*(\w+)", body)


def group(languages):
    """One entry per distinct panel, in the order the languages first use it."""
    panels = []
    for language in languages:
        for panel in panels:
            if panel["rows"] == language["rows"]:
                panel["languages"].append(language)
                break
        else:
            stem = language["file"].replace("Language_", "").replace(".cpp", "")
            panels.append({"name": "panel_" + stem,
                           "rows": language["rows"],
                           "languages": [language]})
    return panels


def render(languages, panels):
    """panels.scad: one variable per distinct panel, then the table over them."""
    out = [
        "/*",
        " * The clock's letter panels, generated by scripts/panels.py out of the",
        " * language files the firmware renders with. Do not edit by hand: a change",
        " * here would be a panel the clock does not have.",
        " *",
        " * Eleven columns by ten rows, row 0 at the top and column 0 on the left,",
        " * which is how the firmware indexes them too.",
        " */",
        "",
    ]

    for panel in panels:
        # The four German dialects are one panel; writing it out once rather
        # than four times is the whole reason these are grouped.
        out.append("// " + ", ".join(l["name"] for l in panel["languages"]))
        out.append(panel["name"] + " = [")
        for i, row in enumerate(panel["rows"]):
            cells = ", ".join('"%s"' % c for c in row)
            out.append("    [%s]%s" % (cells, "," if i + 1 < len(panel["rows"]) else ""))
        out.append("];")
        out.append("")

    out.append("// Every language the firmware knows, in the order of its LANGUAGE_*")
    out.append("// numbers - which are stored in NVS and must keep their values.")
    out.append("panels = [")
    width = max(len(l["code"]) for l in languages) + 3
    for i, language in enumerate(languages):
        name = next(p["name"] for p in panels if p["rows"] == language["rows"])
        out.append("    [%-*s %s]%s   // %s"
                   % (width, '"%s",' % language["code"], name,
                      "," if i + 1 < len(languages) else "", language["name"]))
    out.append("];")
    out.append("")
    out.append('// The letters of one language, e.g. panel("de-DE"). A code that is not')
    out.append("// in the list gives undef, which OpenSCAD complains about loudly enough.")
    out.append("function panel(code) = [for (p = panels) if (p[0] == code) p[1]][0];")
    out.append("")

    return "\n".join(out)


def main():
    files = sorted(os.path.join(SOURCE, f) for f in os.listdir(SOURCE)
                   if f.startswith("Language_") and f.endswith(".cpp"))

    found = {}
    for path in files:
        for language in languages_in(path):
            found[language["symbol"]] = language

    order = table_order(os.path.join(SOURCE, "Languages.cpp"))
    missing = [s for s in order if s not in found]
    if missing:
        raise SystemExit("in the table but in no file: " + ", ".join(missing))
    extra = [s for s in found if s not in order]
    if extra:
        raise SystemExit("defined but not in the table: " + ", ".join(extra))

    languages = [found[s] for s in order]
    panels = group(languages)
    text = render(languages, panels)

    before = None
    if os.path.exists(TARGET):
        before = io.open(TARGET, encoding="utf-8", newline="").read()
    io.open(TARGET, "w", encoding="utf-8", newline="\n").write(text)

    print("%d languages over %d panels -> %s"
          % (len(languages), len(panels), os.path.relpath(TARGET, ROOT)))
    for panel in panels:
        print("  %-12s %s" % (panel["name"],
                              ", ".join(l["code"] for l in panel["languages"])))
    print("unchanged" if before == text else "written")


if __name__ == "__main__":
    main()
