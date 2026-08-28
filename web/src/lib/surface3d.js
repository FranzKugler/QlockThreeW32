/**
 * surface3d
 * The isometric projection behind the colour-aware brightness diagram.
 *
 * The 2D curve above it answers one question - how bright at how much light -
 * and the factory model answers two, because what a percentage is worth
 * depends on the colour the face is showing. A second flat chart per hue would
 * be six charts nobody compares; one surface over both axes is the shape of
 * the thing being described.
 *
 * **No library.** Not a preference: a WebGL or Plotly bundle is hundreds of
 * kilobytes into a 3.5 MB partition that also holds the rest of this SPA, and
 * the clock has to work on a network with no internet at all, so nothing may
 * come from a CDN. What is actually needed is a projection and a painter's
 * algorithm, which is this file - and a deterministic isometric view a reader
 * can compare against yesterday's beats a rotatable one they cannot.
 *
 * It is a plain module with no Svelte and no DOM so that `tests/web/` can run
 * it under `node --test`. The parts that can be wrong without looking wrong -
 * which cell is where, whether the hue axis closes on itself, whether the
 * drawing stays inside its box - are exactly the parts a screenshot cannot
 * check and a person looking at an isometric plot cannot either.
 */

// The classic isometric pair: thirty degrees either side of the horizontal.
// Written out rather than computed from an angle, because these two numbers
// are the whole projection and naming them is clearer than deriving them.
const COS30 = Math.cos(Math.PI / 6);
const SIN30 = Math.sin(Math.PI / 6);

// How much of the box the height of the surface may use. The rest is the
// footprint, and a surface allowed the whole box turns into a wall.
const HEIGHT_SHARE = 0.42;

// Margins in the projected space, before scaling. Room for the tick labels
// along the two bottom edges.
const PAD = { left: 30, right: 8, top: 10, bottom: 22 };

/**
 * The colour of a hue band.
 *
 * The hue itself, which is the one honest choice here - the axis *is* hue, so
 * anything else would be a second encoding of a thing already encoded. The
 * lightness is fixed at a value that reads on both the light and the dark
 * theme; the surface sits on a card whose background changes and the bands
 * must not disappear into either.
 */
export function bandColour(hue) {
  const wrapped = ((Math.round(hue) % 360) + 360) % 360;
  return `hsl(${wrapped} 72% 52%)`;
}

/** Log light, floored the way the clock floors it. */
function logLux(lux) {
  return Math.log10(Math.max(lux, 1e-4));
}

/** Whether the response is a grid this can draw at all. */
function usable(surface) {
  if (!surface || surface.valid === false) return false;
  const { lux, hue, percent } = surface;
  if (!Array.isArray(lux) || !Array.isArray(hue) || !Array.isArray(percent)) return false;
  // Two of each: one ambient level is a line and one hue is a stick, and
  // neither is a surface.
  if (lux.length < 2 || hue.length < 2) return false;
  if (percent.length !== lux.length) return false;
  // Ragged is refused rather than half drawn. Half a surface looks like a
  // measurement with a hole in it, which is a different and much more alarming
  // thing than a response that did not arrive.
  return percent.every((row) => Array.isArray(row) && row.length === hue.length);
}

/**
 * Projects the surface into a box.
 *
 * `surface` is the answer from GET /luminance/surface: `lux` and `hue` are the
 * two axes, `percent` is a row per ambient level, and `limited`/`bound` are the
 * same shape again saying which cells are the gamut running out and which rest
 * on an observation that only ever said "at least this much".
 *
 * `options.point` is where the clock is right now, and it is drawn on the
 * surface rather than beside it: the number is already in the read-out, and
 * what the diagram adds is *where* on the model it sits.
 */
export function projectSurface(surface, options = {}) {
  const width = options.width ?? 320;
  const height = options.height ?? 220;
  const empty = {
    empty: true, width, height, quads: [], wires: [],
    axes: { lux: [], hue: [], percent: [] }, point: null,
    range: { min: 0, max: 100 }
  };
  if (!usable(surface)) return empty;

  const { lux, hue, percent } = surface;
  const limited = surface.limited ?? [];
  const bound = surface.bound ?? [];
  const low = options.minPercent ?? surface.minPercent ?? 20;
  const high = options.maxPercent ?? surface.maxPercent ?? 100;

  const xs = lux.map(logLux);
  const span = xs[xs.length - 1] - xs[0] || 1;
  // Hue closes on itself, so the axis runs a whole period rather than to the
  // last column: the segment from the last hue back to the first is as real as
  // any other and is the one that gets forgotten.
  const period = options.huePeriod ?? 360;

  const depth = width - PAD.left - PAD.right;
  const rise = height * HEIGHT_SHARE;
  // The isometric footprint is a rhombus of width (u + v) * cos30, so the
  // scale that makes it fit is half the room across.
  const scale = depth / (2 * COS30);
  const originX = PAD.left + depth / 2;
  const originY = height - PAD.bottom - (depth / 2) * SIN30;

  const at = (u, v, w) => {
    const clampedW = Math.min(1, Math.max(0, w));
    return [
      originX + (u - v) * COS30 * scale,
      originY + (u + v - 1) * SIN30 * scale - clampedW * rise
    ];
  };

  const u = (row) => (xs[row] - xs[0]) / span;
  const v = (col) => (hue[col] % period) / period;
  const w = (value) => (value - low) / (high - low || 1);

  /** A cell flag is the OR of the four grid corners the quad is drawn between. */
  const corner = (grid, row, col, next) =>
    Boolean(grid?.[row]?.[col]) || Boolean(grid?.[row]?.[next])
    || Boolean(grid?.[row + 1]?.[col]) || Boolean(grid?.[row + 1]?.[next]);

  const quads = [];
  for (let row = 0; row + 1 < lux.length; row++) {
    for (let col = 0; col < hue.length; col++) {
      const next = (col + 1) % hue.length;
      const wraps = next === 0;
      // The far edge of the wrap segment is the first column again, one whole
      // period along - not the first column where it is stored, or the quad
      // folds back across the whole diagram.
      const farV = wraps ? 1 : v(next);

      const corners = [
        at(u(row), v(col), w(percent[row][col])),
        at(u(row), farV, w(percent[row][next])),
        at(u(row + 1), farV, w(percent[row + 1][next])),
        at(u(row + 1), v(col), w(percent[row + 1][col]))
      ];

      quads.push({
        points: corners,
        d: `M${corners.map(([x, y]) => `${x.toFixed(2)} ${y.toFixed(2)}`).join('L')}Z`,
        hue: hue[col],
        colour: bandColour(hue[col]),
        percent: percent[row][col],
        lux: lux[row],
        wraps,
        // Both flags travel, because they say different things: `limited` is
        // the colour running out of slider, which is the gamut and not a
        // fault, and `bound` is a corner of the measurement that only ever
        // said "at least". A cell can be both.
        //
        // **All four corners**, and the two at `next` are the ones that get
        // forgotten. A quad spans two ambient levels *and* two hues; reading
        // only the pair at `col` marks the patch to the left of a limited
        // corner and leaves the one to its right clean, which draws the
        // boundary half a cell away from where the gamut actually runs out and
        // does it silently. On the wrap segment `next` is column 0, so the
        // seam reads its far corners from the first column rather than from
        // one past the last.
        limited: corner(limited, row, col, next),
        bound: corner(bound, row, col, next),
        // Painter's order: the cell nearest the reader is drawn last. With an
        // isometric view that is simply the largest (u + v).
        depth: u(row) + v(col)
      });
    }
  }
  quads.sort((a, b) => a.depth - b.depth);

  // The wireframe over the top. Drawn as well as the fill rather than instead
  // of it: the fill carries the hue and the wire carries the shape, and on a
  // small screen the fill alone reads as a flat patchwork.
  const wires = [];
  for (let row = 0; row < lux.length; row++) {
    const line = [];
    for (let col = 0; col <= hue.length; col++) {
      const index = col % hue.length;
      const alongV = col === hue.length ? 1 : v(index);
      line.push(at(u(row), alongV, w(percent[row][index])));
    }
    wires.push({
      kind: 'lux',
      lux: lux[row],
      d: `M${line.map(([x, y]) => `${x.toFixed(2)} ${y.toFixed(2)}`).join('L')}`
    });
  }

  const axes = {
    lux: lux.map((value, row) => {
      const [x, y] = at(u(row), 0, 0);
      return { value, label: String(value), x, y };
    }),
    // Labelled at the knots the model was measured at, not at every column:
    // 24 labels on a 320 px box is a grey smear.
    hue: [],
    percent: []
  };
  for (let degrees = 0; degrees < period; degrees += 60) {
    const [x, y] = at(1, degrees / period, 0);
    axes.hue.push({ value: degrees, label: `${degrees}°`, x, y, colour: bandColour(degrees) });
  }
  for (let value = Math.ceil(low / 20) * 20; value <= high; value += 20) {
    const [x, y] = at(0, 0, w(value));
    axes.percent.push({ value, label: `${value}`, x, y });
  }

  let point = null;
  if (options.point && Number.isFinite(options.point.lux)) {
    const x = logLux(options.point.lux);
    const raw = (x - xs[0]) / span;
    // Clamped onto the surface rather than drawn off it. A room outside
    // everything anybody measured is exactly the case worth showing, and a
    // marker floating in the margin says nothing about where on the model the
    // clock is - so it sits on the edge and admits it.
    const clamped = raw < 0 || raw > 1;
    const [px, py] = at(
      Math.min(1, Math.max(0, raw)),
      (((options.point.hue % period) + period) % period) / period,
      w(options.point.percent)
    );
    point = { x: px, y: py, clamped, ...options.point };
  }

  return { empty: false, width, height, quads, wires, axes, point,
           range: { min: low, max: high } };
}
