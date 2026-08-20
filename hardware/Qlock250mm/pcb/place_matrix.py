r"""
place_matrix
Positions the word clock's LED matrix, its decoupling capacitors, and the four
corner LEDs that show the minutes between the five minute steps.

Run from KiCad's PCB editor, Tools -> Scripting Console:

    exec(open(r"c:\Users\Franz\Workspace\QlockThreeW32\hardware\Qlock250mm\pcb\place_matrix.py").read())

The layout is computed here rather than stored as a list, so the geometry is
readable and a change means editing one number:

  - 11 columns x 10 rows, serpentine, D1 bottom right, D110 top right.
  - Column pitch 1000/60 mm; row pitch is 11/10 of that, so the matrix comes
    out very nearly square (166.667 x 165.000 mm) on the 240 mm board.
  - Centred on the board centre, 160/160, which is where the board's own
    centre marker sits.
  - Every LED's 100 nF capacitor goes 5 mm to its right, same height, and
    carries the same number. It is turned so its ground pad faces the ground
    pour and its +5V pad the +5V pour - see CAP_ROTATION.
  - D111..D114 sit one grid step diagonally outside each corner of the matrix,
    so they land on the same lattice and stay symmetric about the centre.
  - Every reference is placed above its part, upright, on one line per row -
    see the label section.

Three groups, each placed all or nothing, so a half rearranged board cannot
happen. They are separate because they arrive at different times: the matrix is
on the board already, the corner capacitors came in with the netlist, and the
corner LEDs do not exist yet - they have to be added to the schematic and
imported first. A group that is not there yet is reported and skipped; it does
not hold up the others.

Nothing else is touched: the LED rotations stay as they are (the odd rows are
at 180 deg so the data chain runs DOUT -> DIN without detours), and C115..C120
are left alone - they belong to no particular LED.

Order matters in here: placing a footprint carries its texts along and turning
it turns them, so the labels are set last, after everything has stopped moving.

Note that changes made from the console do not always land on the undo stack.
Save the board before running this, so there is a known state to go back to.
"""
import pcbnew

# --- the geometry, all in millimetres -------------------------------------

DX = 1000.0 / 60.0          # column pitch, 16.666667
DY = DX * 11.0 / 10.0       # row pitch,    18.333333
CENTRE_X = 160.0
CENTRE_Y = 160.0
COLS = 11
ROWS = 10
CAP_OFFSET_X = 5.0          # capacitor sits this far right of its LED

# The corner LEDs, as whole grid steps from the matrix LED they sit beside.
# KiCad's y grows downward, so a positive row step moves *down* the board.
#   reference: (anchor, columns right, rows down)
CORNERS = {
    "D111": ("D1",   +1, +1),   # outside the bottom right corner
    "D112": ("D110", +1, -1),   # outside the top right
    "D113": ("D100", -1, -1),   # outside the top left
    "D114": ("D11",  -1, +1),   # outside the bottom left
}

# Which way a capacitor faces. The power pours run in bands: a +5V band spans
# each pair of LED rows and a GND band fills the gap between two pairs, so for
# any given row one net lies above it and the other below. Turning the
# capacitor so each of its pads points at its own pour is what keeps both ends
# connected - see power_zones.py, which lays those bands out.
#
# The 0402 has its +5V pad on the left and its ground pad on the right at 0
# deg. -90 puts ground at the bottom, +90 puts it at the top.
CAP_ROTATION = {0: -90.0, 180: +90.0}   # keyed by the rotation of its LED

# Set False to leave the capacitors turned however they are.
SET_CAP_ROTATION = True

# Where a reference sits, measured from the part's centre. KiCad's y grows
# downward, so a negative offset is upward on the board.
#
# 3.5 mm is what the bottom row of LEDs already had: it clears the 5 mm body by
# a millimetre. The capacitors take the same height rather than hugging their
# own 1.5 mm body, so each row of the matrix reads as one straight line of
# labels - "D14  C14" side by side - instead of a staircase. Change this one
# number to move them down onto their parts.
LED_LABEL_OFFSET_Y = -3.5
CAP_LABEL_OFFSET_Y = -3.5

# The LEDs are labelled about their centre, the capacitors by their left edge,
# so the C sits flush with the left boundary of the part it belongs to.
LED_LABEL_ALIGN_LEFT = False
CAP_LABEL_ALIGN_LEFT = True

# Set False to leave every reference where it is.
SET_LABELS = True


def to_point_nm(x_nm, y_nm):
    """A board position in nanometres. VECTOR2I since KiCad 7, else wxPoint."""
    try:
        return pcbnew.VECTOR2I(int(x_nm), int(y_nm))
    except AttributeError:
        return pcbnew.wxPoint(int(x_nm), int(y_nm))


def to_point(x_mm, y_mm):
    """A board position in millimetres."""
    return to_point_nm(pcbnew.FromMM(x_mm), pcbnew.FromMM(y_mm))


def build_matrix():
    """Reference -> (x, y). Row 0 is the bottom row; KiCad's y grows downward."""
    left = CENTRE_X - (COLS - 1) / 2.0 * DX
    bottom = CENTRE_Y + (ROWS - 1) / 2.0 * DY

    targets = {}
    for row in range(ROWS):
        for step in range(COLS):
            number = 11 * row + 1 + step
            # Even rows run right to left, odd rows left to right.
            column = step if row % 2 else (COLS - 1 - step)
            x = left + column * DX
            y = bottom - row * DY
            targets["D%d" % number] = (x, y)
            targets["C%d" % number] = (x + CAP_OFFSET_X, y)
    return targets


def led_rotation(number):
    """0 deg on the odd rows, 180 on the even ones - matching the data chain."""
    return 0 if ((number - 1) // COLS) % 2 == 0 else 180


def build_corners(matrix):
    """The corner LEDs and their capacitors, derived from the matrix.

    Taken from the computed matrix rather than from the placed footprints, so
    the capacitors can be set before their LEDs exist.
    """
    leds, caps = {}, {}
    for ref, (anchor, dcol, drow) in CORNERS.items():
        x = matrix[anchor][0] + dcol * DX
        y = matrix[anchor][1] + drow * DY
        leds[ref] = (x, y)
        caps["C" + ref[1:]] = (x + CAP_OFFSET_X, y)
    return leds, caps


def apply(board, targets, label):
    """Places a group, or none of it. Returns True when it was placed."""
    found, missing = {}, []
    for ref in sorted(targets):
        footprint = board.FindFootprintByReference(ref)
        if footprint is None:
            missing.append(ref)
        else:
            found[ref] = footprint

    if missing:
        print("%-28s uebersprungen, nicht auf der Platine: %s"
              % (label, ", ".join(missing)))
        return False

    for ref, footprint in found.items():
        footprint.SetPosition(to_point(*targets[ref]))
    print("%-28s %d Bauteile gesetzt." % (label, len(found)))
    return True


def h_align(name):
    """GR_TEXT_H_ALIGN_* was called GR_TEXT_HJUSTIFY_* before KiCad 7."""
    for prefix in ("GR_TEXT_H_ALIGN_", "GR_TEXT_HJUSTIFY_"):
        value = getattr(pcbnew, prefix + name, None)
        if value is not None:
            return value
    raise AttributeError("no horizontal alignment constant for " + name)


def left_edge_nm(footprint):
    """The left edge of a part as it is drawn - its courtyard, else its pads.

    Taken from the footprint rather than from a table of body sizes, because
    the capacitors are turned 90 deg: their 3 mm long side runs vertically, so
    the width that matters here is not the one the datasheet leads with.
    """
    boxes = [item.GetBoundingBox() for item in footprint.GraphicalItems()
             if item.GetLayerName() in ("F.CrtYd", "B.CrtYd")]
    if not boxes:
        boxes = [pad.GetBoundingBox() for pad in footprint.Pads()]
    if not boxes:
        return footprint.GetPosition().x
    return min(box.GetLeft() for box in boxes)


def place_label(footprint, offset_y_mm, align_left):
    """Puts the reference above the part, upright. Returns True if it moved.

    Position and angle are set in board coordinates, not as an offset from the
    footprint: an offset would be turned with the part, which is exactly how
    the labels of the 180 deg rows ended up underneath their LEDs and upside
    down.
    """
    field = footprint.Reference()
    centre = footprint.GetPosition()
    wanted_x = left_edge_nm(footprint) if align_left else centre.x
    wanted_y = centre.y + pcbnew.FromMM(offset_y_mm)
    wanted_align = h_align("LEFT" if align_left else "CENTER")

    before = (field.GetPosition().x, field.GetPosition().y,
              field.GetTextAngle().AsDegrees(), field.GetHorizJustify())

    field.SetTextAngle(pcbnew.EDA_ANGLE(0.0, pcbnew.DEGREES_T))
    try:
        field.SetKeepUpright(False)
    except AttributeError:
        pass                                   # gone in newer KiCad builds
    field.SetHorizJustify(wanted_align)
    field.SetPosition(to_point_nm(wanted_x, wanted_y))

    landed = field.GetPosition()
    off_by = max(abs(landed.x - wanted_x), abs(landed.y - wanted_y))
    moved = (round(before[0]) != round(landed.x)
             or round(before[1]) != round(landed.y)
             or round(before[2]) % 360 != 0
             or before[3] != wanted_align)
    return moved, off_by


def label_group(board, refs, offset_y_mm, align_left, caption):
    """Labels what is there and reports what is not."""
    moved, missing, worst = 0, [], 0
    for ref in refs:
        footprint = board.FindFootprintByReference(ref)
        if footprint is None:
            missing.append(ref)
            continue
        changed, off_by = place_label(footprint, offset_y_mm, align_left)
        moved += 1 if changed else 0
        worst = max(worst, off_by)
    done = len(refs) - len(missing)
    print("%-28s %3d beschriftet, davon %3d veraendert.%s"
          % (caption, done, moved,
             "" if not missing else "  fehlen: %d" % len(missing)))
    return worst


def main():
    board = pcbnew.GetBoard()
    matrix = build_matrix()
    corner_leds, corner_caps = build_corners(matrix)

    placed_matrix = apply(board, matrix, "Matrix D1-D110, C1-C110:")
    placed_leds = apply(board, corner_leds, "Ecken-LEDs D111-D114:")
    placed_caps = apply(board, corner_caps, "Ecken-Cs C111-C114:")

    if SET_CAP_ROTATION and placed_matrix:
        turned, disagree = 0, []
        for number in range(1, COLS * ROWS + 1):
            cap = board.FindFootprintByReference("C%d" % number)
            led = board.FindFootprintByReference("D%d" % number)
            if cap is None:
                continue
            expected = led_rotation(number)
            if led is not None and round(led.GetOrientationDegrees()) % 360 != expected:
                disagree.append("D%d" % number)
            wanted = CAP_ROTATION[expected]
            if round(cap.GetOrientationDegrees() - wanted) % 360 != 0:
                cap.SetOrientationDegrees(wanted)
                turned += 1
        print("Kondensatoren gedreht:        %d (die uebrigen standen schon richtig)."
              % turned)
        if disagree:
            print("  ACHTUNG: LED-Drehung weicht vom Reihenmuster ab: %s"
                  % ", ".join(disagree[:10]))

    if SET_LABELS:
        matrix_leds = ["D%d" % n for n in range(1, COLS * ROWS + 1)]
        matrix_caps = ["C%d" % n for n in range(1, COLS * ROWS + 1)]
        corner_refs = sorted(CORNERS, key=lambda r: int(r[1:]))
        worst = max(
            label_group(board, matrix_leds + corner_refs,
                        LED_LABEL_OFFSET_Y, LED_LABEL_ALIGN_LEFT,
                        "Beschriftung LEDs:"),
            label_group(board, matrix_caps + ["C" + r[1:] for r in corner_refs],
                        CAP_LABEL_OFFSET_Y, CAP_LABEL_ALIGN_LEFT,
                        "Beschriftung Kondensatoren:"),
        )
        # Read back rather than trust the setters: a footprint field can store
        # its position relative to the part, and then an absolute set lands
        # somewhere else entirely.
        print("  groesste Abweichung vom Sollpunkt: %.4f mm"
              % (worst / 1000000.0))

    pcbnew.Refresh()

    print("")
    print("X-Raster %.6f mm, Y-Raster %.6f mm" % (DX, DY))
    if placed_matrix:
        leds = [matrix["D%d" % n] for n in range(1, 111)]
        xs = sorted({round(p[0], 3) for p in leds})
        ys = sorted({round(p[1], 3) for p in leds})
        print("Matrix %.4f x %.4f mm, Mitte %.3f / %.3f, %d Spalten x %d Zeilen"
              % (xs[-1] - xs[0], ys[-1] - ys[0],
                 (xs[0] + xs[-1]) / 2.0, (ys[0] + ys[-1]) / 2.0, len(xs), len(ys)))
    print("")
    for ref in sorted(corner_leds, key=lambda r: int(r[1:])):
        cap = "C" + ref[1:]
        print("%s %9.4f / %9.4f%s     %s %9.4f / %9.4f%s   neben %s"
              % (ref, corner_leds[ref][0], corner_leds[ref][1],
                 "" if placed_leds else " (offen)",
                 cap, corner_caps[cap][0], corner_caps[cap][1],
                 "" if placed_caps else " (offen)",
                 CORNERS[ref][0]))
    print("")
    print("C115-C120 nicht angefasst - sie gehoeren zu keiner LED.")
    if placed_matrix or placed_leds or placed_caps:
        print("Jetzt in KiCad speichern (Strg+S).")


main()
