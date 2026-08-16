"""
icon
Generates the home screen icons of the web UI, for iOS and for Android.

It draws the clock's own face reading "ES IST HALB ACHT": the eleven by ten
letter grid on black, with the words of that sentence lit and the rest of the
letters left at the dim grey unlit acrylic has. The lit cells below are the
positions the firmware's own bit masks set, so the icon shows a state the clock
can actually be in rather than an invented one.

Run it after changing anything here; the result is committed, so neither the
web build nor CI needs Python or the font:

    pip install pillow
    python scripts/icon.py

@author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
@version  2.1
@created  16.8.2026
@updated  16.8.2026
"""

import os
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("Pillow is missing - run: pip install pillow")

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PUBLIC_DIR = os.path.join(PROJECT_DIR, "web", "public")

# The front panel, matching the listing in src/Woerter_DE.h. The lit words come
# from the masks in that header; the letters that are never lit are filler and
# only this listing knows them.
GRID = [
    "ESKISTAFÜNF",
    "ZEHNZWANZIG",
    "DREIVIERTEL",
    "VORFUNKNACH",
    "HALBAELFÜNF",
    "EINSXAMZWEI",
    "DREIAUJVIER",
    "SECHSNLACHT",
    "SIEBENZWÖLF",
    "ZEHNEUNKUHR",
]

# ES IST HALB ACHT - half past seven - from the masks themselves:
#   DE_ESIST   matrix[0] |= 0b1101110000000000  -> columns 0,1 and 3,4,5
#   DE_HALB    matrix[4] |= 0b1111000000000000  -> columns 0..3
#   DE_H_ACHT  matrix[7] |= 0b0000000111100000  -> columns 7..10
LIT = {
    (0, 0), (0, 1), (0, 3), (0, 4), (0, 5),
    (4, 0), (4, 1), (4, 2), (4, 3),
    (7, 7), (7, 8), (7, 9), (7, 10),
}

BACKGROUND = (13, 13, 13)
UNLIT = (74, 74, 74)
LIT_COLOUR = (245, 244, 240)

# Century Gothic is the closest thing Windows ships to the geometric sans on the
# real panel. Only the rendered image is distributed, never the font itself.
FONTS = [
    r"C:\Windows\Fonts\GOTHIC.TTF",
    r"C:\Windows\Fonts\bahnschrift.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
]

# Drawn ten times the target size and reduced: asking the rasteriser for 14 px
# type directly gives mush, reducing clean 140 px type does not.
OVERSAMPLE = 10

# Margin around the letter block, as a fraction of the edge.
#
# 0.105 leaves the letters clear of the squircle iOS masks the icon with, which
# takes about 22 % out of each corner. Android's maskable icons are stricter:
# a launcher may crop to any shape inside the middle 80 %, so anything that has
# to survive lives within the safe zone and the margin has to grow to match.
# Both are supplied as plain squares - rounding here would only be rounded a
# second time.
MARGIN_RATIO = 0.105
MASKABLE_MARGIN_RATIO = 0.20

# What each file is for:
#   apple-touch-icon  iOS home screen, the only size current iPhones use
#   icon-192          Android home screen
#   icon-512          Android splash screen and app listings
#   icon-512-maskable declared separately in the manifest, see above
OUTPUTS = [
    ("apple-touch-icon.png", 180, MARGIN_RATIO),
    ("icon-192.png", 192, MARGIN_RATIO),
    ("icon-512.png", 512, MARGIN_RATIO),
    ("icon-512-maskable.png", 512, MASKABLE_MARGIN_RATIO),
]


def font_path():
    for candidate in FONTS:
        if os.path.isfile(candidate):
            return candidate
    sys.exit("No usable font found - tried:\n  " + "\n  ".join(FONTS))


def render(size, margin_ratio):
    image = Image.new("RGB", (size, size), BACKGROUND)
    draw = ImageDraw.Draw(image)

    margin = size * margin_ratio
    span = size - 2 * margin
    pitch_x, pitch_y = span / 11, span / 10
    font = ImageFont.truetype(font_path(), int(pitch_x * 0.78))

    for row, letters in enumerate(GRID):
        for column, letter in enumerate(letters):
            draw.text(
                (margin + pitch_x * (column + 0.5), margin + pitch_y * (row + 0.5)),
                letter,
                font=font,
                fill=LIT_COLOUR if (row, column) in LIT else UNLIT,
                anchor="mm",
            )

    return image


def main():
    os.makedirs(PUBLIC_DIR, exist_ok=True)

    for name, size, margin_ratio in OUTPUTS:
        path = os.path.join(PUBLIC_DIR, name)
        icon = render(size * OVERSAMPLE, margin_ratio).resize((size, size), Image.LANCZOS)
        icon.save(path, optimize=True)
        print("%-24s %4dx%-4d %6d bytes" % (name, size, size, os.path.getsize(path)))


if __name__ == "__main__":
    main()
