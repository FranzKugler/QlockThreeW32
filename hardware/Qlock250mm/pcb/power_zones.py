r"""
power_zones
Lays the alternating +5V and GND copper pours on the front layer.

Run from KiCad's PCB editor, Tools -> Scripting Console:

    exec(open(r"c:\Users\Franz\Workspace\QlockThreeW32\hardware\Qlock250mm\pcb\power_zones.py").read())

The bands follow the LED lattice rather than fixed numbers, so they move with
the matrix if its pitch is ever changed:

  - A +5V band spans each *pair* of LED rows - rows 1+2, 3+4, and so on - and
    reaches out to x = 260 on the right, past the matrix.
  - A GND band fills the gap between two such pairs - between rows 2 and 3,
    4 and 5, ... - and reaches out to x = 60 on the left instead.
  - Two further GND bands, half a row pitch high, sit outside the first and the
    last row. Those two rows face outward, so no band between two pairs can
    reach their VSS pads; without these, 22 of the 110 LEDs and their
    capacitors would have no ground copper at all.

That gives five +5V bands and six GND bands, alternating, meeting exactly on
the row lines with no gap and no overlap.

The corners of the bands are the LED *centres*, not the outlines of their
packages: package corners would make neighbouring bands of different nets
overlap by 5 mm.

On the side where a band does *not* have its long reach it still has to cover
the pads it feeds, and a centre line alone does not do that. Odd rows sit at 0
deg and even rows at 180, so each pair has one VDD pad on the left of its LED
and one on the right; the same for the two VSS pads a GND band has to cover.
PAD_REACH therefore pushes the near edge past the outer pad edge:

  - +5V bands reach PAD_REACH to the left of the leftmost column,
  - GND bands reach PAD_REACH to the right of the rightmost column.

Keep an eye on the GND side. The capacitors sit 5 mm right of their LEDs, and
the nearest of their +5V pads starts only 0.575 mm past the LED pad that the
band must cover - so PAD_MARGIN has very little room to grow before the pour
runs into it.

The script removes its own +5V and GND pours on the front layer before adding
them again, so it can be run after a change without stacking duplicates. Pours
that carry a priority are left standing: this script never sets one, so a
priority is what tells a hand-drawn pour apart from a generated one - the two
strips along the left and right edge that tie the bands together are of that
kind. See DELETE_EXISTING and KEEP_PRIORITISED.

Removing is where this gets delicate, and the reason for the parking list in
drop_existing(). pcbnew's Remove() hands the C++ object to Python:

    def Remove(self, item):
        self.RemoveNative(item)
        if (not IsActionRunning()):
            item.thisown = 1

but there is no destructor registered for ZONE - KiCad says so itself, with a
"detected a memory leak of type 'ZONE *', no destructor found" on the console.
When the Python proxy is then collected, SWIG's type table is left damaged: from
that moment on FindNet() returns a bare SwigPyObject instead of a NETINFO_ITEM,
and so does LoadBoard(). The damage is to the interpreter, not to the board, and
the interpreter belongs to the running KiCad - so the run that removes the zones
still succeeds and the *next* one fails with

    AttributeError: 'SwigPyObject' object has no attribute 'GetNetCode'

and keeps failing until KiCad is restarted. Keeping a reference to every removed
zone for the life of the session avoids the collection and costs a few hundred
kilobytes. Delete() would be correct on the Python side, but it frees a board
item that the editor's view still points at, which is the worse trade in a GUI.

healthy_net() checks for the damage up front and says what to do about it,
rather than letting it surface as an AttributeError three frames down.

Note that changes made from the console do not always land on the undo stack.
Save the board before running this, so there is a known state to go back to.
"""
import pcbnew

# --- the lattice, matching place_matrix.py --------------------------------

DX = 1000.0 / 60.0
DY = DX * 11.0 / 10.0
CENTRE_X = 160.0
CENTRE_Y = 160.0
COLS = 11
ROWS = 10

RIGHT_EXTENT = 260.0        # +5V bands reach out to here
LEFT_EXTENT = 60.0          # GND bands reach out to here

# WS2812B pad geometry, for covering the pads on the near side.
LED_PAD_OFFSET_X = 2.45     # pad centre, from the LED centre
LED_PAD_HALF_W = 0.75       # half the pad width
PAD_MARGIN = 0.25           # how far the copper goes beyond the pad edge
PAD_REACH = LED_PAD_OFFSET_X + LED_PAD_HALF_W + PAD_MARGIN

# The GND bands have to reach past the capacitors of the rightmost column as
# well, or those sit outside the copper. A capacitor is CAP_OFFSET_X right of
# its LED and its outer pad edge is CAP_PAD_REACH beyond its own centre - taken
# from the 0402 at 0 deg, which is the widest of the orientations in use.
CAP_OFFSET_X = 5.0
CAP_PAD_REACH = 1.225
GND_REACH = CAP_OFFSET_X + CAP_PAD_REACH + PAD_MARGIN

# Rows 1 and 10 face outward, so no band between two pairs can reach their VSS
# pads. These two edge bands close that gap. Half the row pitch is enough to
# clear the pads and keeps them clearly subordinate to the main bands.
EDGE_BAND_FRACTION = 0.5

NET_HIGH = "+5V"
NET_LOW = "GND"
LAYER = pcbnew.F_Cu

# Drop existing pours on these two nets first, so re-running is idempotent.
DELETE_EXISTING = True
# ... but not the ones that were drawn by hand. This script never sets a
# priority, so a pour that has one did not come from here - the two strips that
# tie the bands together along the left and right edge are such pours. Set this
# False to have every +5V/GND pour on the front layer replaced.
KEEP_PRIORITISED = True
# Fill straight away. Set False to just place the outlines and press B yourself.
FILL_AFTER = True


def build_bands():
    """[(netname, x1, y1, x2, y2), ...] with y1 the top edge (smaller y)."""
    left = CENTRE_X - (COLS - 1) / 2.0 * DX
    right = CENTRE_X + (COLS - 1) / 2.0 * DX
    bottom = CENTRE_Y + (ROWS - 1) / 2.0 * DY

    def row_y(index):
        return bottom - index * DY

    bands = []
    for pair in range(ROWS // 2):
        # Two LED rows carry the +5V band between their centre lines; the left
        # edge goes out far enough to sit under both of their VDD pads.
        bands.append((NET_HIGH, left - PAD_REACH, row_y(2 * pair + 1),
                      RIGHT_EXTENT, row_y(2 * pair)))
    for gap in range(ROWS // 2 - 1):
        # The space between one pair and the next carries GND, reaching right
        # far enough to sit under both of the VSS pads that face it and under
        # the capacitors beyond them.
        bands.append((NET_LOW, LEFT_EXTENT, row_y(2 * gap + 2),
                      right + GND_REACH, row_y(2 * gap + 1)))

    # The two edge bands, outside the first and the last row.
    edge = DY * EDGE_BAND_FRACTION
    bands.append((NET_LOW, LEFT_EXTENT, row_y(0),
                  right + GND_REACH, row_y(0) + edge))
    bands.append((NET_LOW, LEFT_EXTENT, row_y(ROWS - 1) - edge,
                  right + GND_REACH, row_y(ROWS - 1)))
    return bands


def report_damage(what):
    """Says what a bare SwigPyObject means and what to do about it."""
    print("ABBRUCH - die Python-Anbindung dieser KiCad-Sitzung ist beschaedigt:")
    print("  %s ist kein brauchbares Objekt mehr, nur noch ein roher Zeiger." % what)
    print("  Ursache ist ein frueherer Lauf, der Zonen entfernt hat; die")
    print("  Platine selbst ist in Ordnung.")
    print("  KiCad neu starten, dann laeuft es wieder. Diese Fassung des")
    print("  Skripts loest den Schaden nicht mehr aus.")


def usable(obj, *methods):
    """True when SWIG handed back the real class and not a bare pointer."""
    return all(hasattr(obj, name) for name in methods)


def healthy_net(board, netname):
    """The net, None when it does not exist, False when SWIG is damaged."""
    net = board.FindNet(netname)
    if net is None:
        return None
    return net if usable(net, "GetNetCode") else False


def keepalive():
    """The parking list for removed zones, one per KiCad session.

    Hung off the pcbnew module rather than kept in a global of this script:
    a global would be rebound on the next run, the old list would be collected,
    and the very thing this exists to prevent would happen then instead.
    """
    store = getattr(pcbnew, "_qlock_removed_zones", None)
    if store is None:
        store = []
        pcbnew._qlock_removed_zones = store
    return store


def zone_priority(zone):
    """Called GetPriority() before KiCad 7."""
    getter = getattr(zone, "GetAssignedPriority", None) or zone.GetPriority
    return getter()


def drop_existing(board):
    """Removes the pours this script owns. Returns (dropped, kept)."""
    dropped, kept = 0, 0
    for zone in list(board.Zones()):
        if zone.GetLayer() != LAYER or zone.GetNetname() not in (NET_HIGH, NET_LOW):
            continue
        if KEEP_PRIORITISED and zone_priority(zone) != 0:
            kept += 1
            continue
        board.Remove(zone)
        keepalive().append(zone)         # must outlive the session, see above
        dropped += 1
    return dropped, kept


def add_zone(board, net, x1, y1, x2, y2):
    """A rectangular pour on an already resolved net."""
    zone = pcbnew.ZONE(board)
    zone.SetLayer(LAYER)
    zone.SetNetCode(net.GetNetCode())

    outline = zone.Outline()
    outline.NewOutline()
    for x, y in ((x1, y1), (x2, y1), (x2, y2), (x1, y2)):
        outline.Append(pcbnew.FromMM(x), pcbnew.FromMM(y))

    board.Add(zone)
    return zone


def main():
    board = pcbnew.GetBoard()

    # The board comes first: in a damaged session GetBoard() itself returns a
    # bare pointer, and then every check below would raise before it could say
    # anything useful.
    if not usable(board, "FindNet", "Zones", "Add"):
        report_damage("pcbnew.GetBoard()")
        return

    # Both nets are resolved before anything is touched. Doing it per band, as
    # this used to, meant the first lookup happened after the removals - and
    # that is exactly the lookup a damaged binding fails on.
    nets = {}
    for name in (NET_HIGH, NET_LOW):
        net = healthy_net(board, name)
        if net is None:
            print("ABBRUCH - Netz '%s' gibt es auf dieser Platine nicht." % name)
            return
        if net is False:
            report_damage("FindNet('%s')" % name)
            return
        nets[name] = net

    if DELETE_EXISTING:
        dropped, kept = drop_existing(board)
        if dropped:
            print("%d vorhandene Flaeche(n) auf %s/%s entfernt."
                  % (dropped, NET_HIGH, NET_LOW))
        if kept:
            print("%d Flaeche(n) mit eigener Prioritaet stehen gelassen "
                  "(von Hand gezeichnet)." % kept)

    made = []
    for netname, x1, y1, x2, y2 in build_bands():
        add_zone(board, nets[netname], x1, y1, x2, y2)
        made.append((netname, x1, y1, x2, y2))

    print("%d Flaechen angelegt: %d x %s, %d x %s"
          % (len(made),
             sum(1 for b in made if b[0] == NET_HIGH), NET_HIGH,
             sum(1 for b in made if b[0] == NET_LOW), NET_LOW))
    print("Pad-Ueberdeckung: %.2f mm ueber die Spaltenmitte hinaus "
          "(Pad endet bei %.2f, Rand %.2f)"
          % (PAD_REACH, LED_PAD_OFFSET_X + LED_PAD_HALF_W, PAD_MARGIN))
    print("")
    print("Netz    x von      bis        y von      bis")
    for netname, x1, y1, x2, y2 in sorted(made, key=lambda b: -b[2]):
        print("%-6s %9.4f %9.4f  %9.4f %9.4f" % (netname, x1, x2, y1, y2))

    if FILL_AFTER:
        print("")
        print("fuelle ...")
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
        print("gefuellt.")

    pcbnew.Refresh()
    print("")
    print("Jetzt in KiCad speichern (Strg+S).")


main()
