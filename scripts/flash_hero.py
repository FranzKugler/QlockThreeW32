"""
flash_hero
Generates the little panel graphic on the flashing page (docs/index.html),
replacing what used to be a monospace text mock-up.

Same idea as icon.py: draw the real English panel, eleven cells by ten rows,
with the words of one real sentence lit from the firmware's own word
positions (src/languages/Language_EN.cpp) rather than an invented layout -
so the graphic shows a state the clock can actually be in. "IT IS FIVE PAST
TWO" is the render() path for minutes/5 == 1 and hours == 2: IT, IS, FIVE,
PAST, and the hour word TWO.

Unlike the dark home-screen icons this is a light-mode graphic to match the
flashing page: a light grey ground with the unlit letters in white, as if
looking at the panel's own diffuser from the front, and the lit sentence in
the page's accent blue - the exact colour of the "Connect and install"
button, so the two read as one idea.

    pip install pillow
    python scripts/flash_hero.py

@author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
@version  1.0
@created  30.8.2026
@updated  30.8.2026
"""

import os
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("Pillow is missing - run: pip install pillow")

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_PATH = os.path.join(PROJECT_DIR, "docs", "panel-preview.png")

# The English panel, from LANGUAGE_ENGLISH in src/languages/Language_EN.cpp.
# One cell holds two characters at (9, 5): the O and its apostrophe share one
# milled opening on the real panel, same rule as every other language - see
# "A cell is not always one character" in CLAUDE.md. It plays no part in this
# sentence, but the grid is drawn whole, the same as the clock's own face.
GRID = [
    list("ITLISASAMPM"),
    list("ACQUARTERDC"),
    list("TWENTYFIVEX"),
    list("HALFSTENFTO"),
    list("PASTERUNINE"),
    list("ONESIXTHREE"),
    list("FOURFIVETWO"),
    list("EIGHTELEVEN"),
    list("SEVENTWELVE"),
    ["T", "E", "N", "S", "E", "O'", "C", "L", "O", "C", "K"],
]

# IT IS FIVE PAST TWO - five past two - straight from the WORDS table and
# render()'s minutes/5 == 1 branch:
#   IT    { 0, 0, "IT" }
#   IS    { 0, 3, "IS" }
#   FIVE  { 2, 6, "FIVE" }
#   PAST  { 4, 0, "PAST" }
#   TWO   { 6, 8, "TWO" }   (the hour word, hours=2, not a full hour)
LIT = {
    (0, 0), (0, 1),
    (0, 3), (0, 4),
    (2, 6), (2, 7), (2, 8), (2, 9),
    (4, 0), (4, 1), (4, 2), (4, 3),
    (6, 8), (6, 9), (6, 10),
}

# The page's own tokens (docs/index.html) - light mode only, since this is a
# still image and cannot follow prefers-color-scheme the way the page's CSS
# does. --bg, white, --accent.
BACKGROUND = (200, 200, 200)
MASK = (255, 255, 255)
LIT_COLOUR = (59, 110, 165)

# Same face as the app icons, for the same reason: the closest thing Windows
# ships to the panel's own geometric sans.
FONTS = [
    r"C:\Windows\Fonts\GOTHIC.TTF",
    r"C:\Windows\Fonts\bahnschrift.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
]

COLS, ROWS = 11, 10
CELL = 68
MARGIN = 36
WIDTH = MARGIN * 2 + CELL * COLS
HEIGHT = MARGIN * 2 + CELL * ROWS

# Drawn six times the target size and reduced, same reasoning as icon.py:
# asking the rasteriser for the final type size directly gives mush.
OVERSAMPLE = 6


def font_path():
    for candidate in FONTS:
        if os.path.isfile(candidate):
            return candidate
    sys.exit("No usable font found - tried:\n  " + "\n  ".join(FONTS))


def render():
    size = OVERSAMPLE
    image = Image.new("RGB", (WIDTH * size, HEIGHT * size), BACKGROUND)
    draw = ImageDraw.Draw(image)

    margin, cell = MARGIN * size, CELL * size
    path = font_path()
    font = ImageFont.truetype(path, int(cell * 0.72))
    apostrophe_font = ImageFont.truetype(path, int(cell * 0.5))

    for row, cells in enumerate(GRID):
        for column, glyph in enumerate(cells):
            colour = LIT_COLOUR if (row, column) in LIT else MASK
            draw.text(
                (margin + cell * (column + 0.5), margin + cell * (row + 0.5)),
                glyph,
                font=font if len(glyph) == 1 else apostrophe_font,
                fill=colour,
                anchor="mm",
            )

    return image.resize((WIDTH, HEIGHT), Image.LANCZOS)


def main():
    os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
    render().save(OUT_PATH, optimize=True)
    print("%-24s %4dx%-4d %6d bytes" % (
        os.path.basename(OUT_PATH), WIDTH, HEIGHT, os.path.getsize(OUT_PATH)
    ))


if __name__ == "__main__":
    main()
