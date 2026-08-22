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

# One cell of the panel is usually one character, but not always: a milled O'
# is one hole in the sheet, and the apostrophe attaches to the character before
# it. Write the plain ASCII one; U+2019 and U+2032 are taken as the same thing
# so that a panel pasted in from elsewhere fails on the word it disagrees with
# rather than on a mystifying cell count. Language.h states the rule.
MARKS = "'’′"

STRING = re.compile(r'"((?:[^"\\]|\\.)*)"')
ROW_ARRAY = re.compile(r"(\w+)\s*\[\s*PANEL_ROWS\s*\]\s*=\s*\{")
DEFINITION = re.compile(r"extern\s+const\s+Language\s+(\w+)\s*=\s*\{")
INDEXED = re.compile(r"(\w+)\s*\[\s*\d+\s*\]")
WORD_ARRAY = re.compile(r"const\s+Word\s+WORDS\s*\[\s*\]\s*=\s*\{")
WORD_ENTRY = re.compile(r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*"((?:[^"\\]|\\.)*)"\s*\}')
ENUM = re.compile(r"\benum\s*\{")
DRAWN_ROW = re.compile(r"^\s*\*\s*(\d)\s+(\S+)", re.MULTILINE)


def split_cells(text):
    """The panel cells of a row, with an apostrophe riding on the one before."""
    cells = []
    for character in text:
        if character in MARKS and cells:
            cells[-1] += character
        else:
            cells.append(character)
    return cells


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


def words_in(source):
    """The (row, col, text) of every entry in the file's WORDS array."""
    match = WORD_ARRAY.search(source)
    if match is None:
        raise ValueError("no WORDS array")
    body = braced(source, match.end() - 1)
    return [(int(r), int(c), unescape(t)) for r, c, t in WORD_ENTRY.findall(body)]


def enum_names(source):
    """
    The names in the file's first enum, which index into WORDS by position.

    If the two ever get out of step every face.light() in that language points
    at the wrong word, and nothing says so - the code still compiles and the
    clock still lights letters, just the wrong ones. Worth counting.
    """
    match = ENUM.search(source)
    if match is None:
        return None
    body = re.sub(r"//.*", "", braced(source, match.end() - 1))
    return [name.strip() for name in body.replace("\n", " ").split(",") if name.strip()]


def drawn_rows(source):
    """
    The panel as the header comment draws it, or None if there is no drawing.

    Only accepted as a drawing when all ten rows are there in order, so that a
    stray "* 3 something" elsewhere in a comment cannot be mistaken for one.
    """
    found = DRAWN_ROW.findall(source)
    if len(found) != PANEL_ROWS:
        return None
    if [int(n) for n, _ in found] != list(range(PANEL_ROWS)):
        return None
    return [row for _, row in found]


def check_file(language, source):
    """
    What the firmware cannot check for itself: the drawing in the header
    comment, and the enum against the words it indexes.

    The drawing is a comment, so nothing has ever forced it to be true - and it
    has quietly disagreed with the rows three times now, once for a whole year.
    It is the first thing anybody reads when adding a word, which is exactly
    why a wrong one is expensive.
    """
    problems = []

    drawn = drawn_rows(source)
    if drawn is None:
        problems.append("%s has no panel drawing in its header comment"
                        % language["code"])
    else:
        for number, (drawing, row) in enumerate(zip(drawn, language["rows"])):
            if drawing != row:
                problems.append('%s row %d is drawn "%s" but reads "%s"'
                                % (language["code"], number, drawing, row))

    names = enum_names(source)
    if names is None:
        problems.append("%s has no enum over its words" % language["code"])
    elif len(names) != len(language["words"]):
        problems.append("%s has %d enum names for %d words"
                        % (language["code"], len(names), len(language["words"])))

    return problems


def check_words(language):
    """
    The same check the firmware runs at every boot (Languages::selfCheck), run
    here as well because this project has no host C++ compiler and a panel that
    disagrees with its own words must not reach a milling machine.
    """
    problems = []
    for row, col, text in language["words"]:
        wanted = split_cells(text)
        if row >= PANEL_ROWS or col + len(wanted) > PANEL_COLS:
            problems.append('%s "%s" runs off the panel at %d,%d'
                            % (language["code"], text, row, col))
            continue
        under = "".join(language["grid"][row][col:col + len(wanted)])
        if under != text:
            problems.append('%s at %d,%d says "%s" but the panel reads "%s"'
                            % (language["code"], row, col, text, under))
    return problems


def languages_in(path):
    """Every Language defined in one file: symbol, code, name, locale, rows."""
    source = io.open(path, encoding="utf-8").read()
    arrays = row_arrays(source)
    words = words_in(source)
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

        grid = [split_cells(row) for row in rows]
        for number, cells in enumerate(grid):
            if len(cells) != PANEL_COLS:
                raise ValueError(
                    "%s row %d: %d cells, expected %d - %r"
                    % (symbol, number, len(cells), PANEL_COLS, rows[number]))
            if cells[0] in MARKS:
                raise ValueError("%s row %d starts with an apostrophe, which "
                                 "has nothing to attach to" % (symbol, number))

        out.append({"symbol": symbol, "code": code, "name": name, "locale": locale,
                    "rows": rows, "grid": grid, "words": words, "source": source,
                    "file": os.path.basename(path)})
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
            # One element per cell, not per character: an O and its apostrophe
            # are one hole in the sheet and have to be cut as one.
            cells = ", ".join('"%s"' % c for c in split_cells(row))
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

    # The German dialects share a file, so its drawing and enum are checked
    # once rather than four times.
    problems = []
    seen = set()
    for language in languages:
        problems += check_words(language)
        if language["file"] not in seen:
            seen.add(language["file"])
            problems += check_file(language, language["source"])
    if problems:
        raise SystemExit("the language files do not agree with themselves:\n  "
                         + "\n  ".join(problems))

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
