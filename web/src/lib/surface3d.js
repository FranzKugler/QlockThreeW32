/**
 * surface3d
 * The cylindrical projection behind the colour-aware brightness diagram.
 *
 * The 2D curve above it answers one question - how bright at how much light -
 * and the factory model answers two, because what a percentage is worth
 * depends on the colour the face is showing. A second flat chart per hue would
 * be six charts nobody compares; one surface over both axes is the shape of
 * the thing being described.
 *
 * **Why a cylinder and not a box.** Hue is an angle. It has no first value and
 * no last one, so a straight axis has to cut it somewhere - and wherever the
 * cut goes, the drawing grows an edge the model does not have, with red on one
 * side and violet on the other and nothing saying they are neighbours. Wrapped
 * round, the seam becomes a joint: hue is the azimuth, ambient light is the
 * radius, and the percentage the model asks for is the height. What used to be
 * the one segment an implementation forgets is now the one segment that gives
 * itself away the moment the angle stops being periodic.
 *
 * **The radius is log light, and that is not a nicety.** The knots run from
 * 0.02 lx to 10 - a factor of five hundred - and on a linear radius every
 * evening this clock is actually read in lands inside the innermost two per
 * cent of the disc. The innermost ring is a ring rather than the axis
 * (`INNER_RADIUS`), because a radius starting at zero collapses the dimmest
 * ambient level onto a single point and every cell of that row into a triangle
 * with no hue left in it - and the dimmest room is the one the automatic gets
 * judged on.
 *
 * **No library.** Not a preference: a WebGL or Plotly bundle is hundreds of
 * kilobytes into a 3.5 MB partition that also holds the rest of this SPA, and
 * the clock has to work on a network with no internet at all, so nothing may
 * come from a CDN. What is actually needed is a rotation, an orthographic
 * projection and a painter's algorithm, which is this file.
 *
 * **The view turns, and there is always a way back.** A fixed view is one a
 * reader can compare against the one they saw yesterday, which is why
 * `DEFAULT_VIEW` exists and why both the button and the Home key return to it
 * exactly. A cylinder without rotation, though, hides half of itself behind
 * the other half, and no single angle answers "what does my hue do" for every
 * hue. Turning it is not decoration here; it is how the far side gets read.
 *
 * It is a plain module with no Svelte and no DOM so that `tests/web/` can run
 * it under `node --test`. The parts that can be wrong without looking wrong -
 * which cell is where, whether the seam closes, whether the drawing stays
 * inside its box at every angle it can be turned to, whether a drag left and a
 * drag right come back to the same place - are exactly the parts a screenshot
 * cannot check and a person looking at a rotated plot cannot either.
 */

/** Where the dimmest ambient ring sits, as a fraction of the outer radius. */
export const INNER_RADIUS = 0.22;

/**
 * The view the diagram opens at, and the one every reset returns to.
 *
 * Frozen because it is compared for equality: "is this the standard view"
 * decides whether the reset control is offered, and a default somebody had
 * mutated would answer that wrongly and silently.
 */
export const DEFAULT_VIEW = Object.freeze({ azimuth: 325, tilt: 26 });

/**
 * How far the camera may be lowered or raised.
 *
 * Below the floor the cylinder is a ribbon with no radius left in it; at ninety
 * degrees the height axis vanishes into the page and the whole third dimension
 * is gone. Both ends are useless views rather than merely ugly ones, so neither
 * is reachable.
 */
export const TILT_MIN = 10;
export const TILT_MAX = 80;

/** One press of an arrow key. */
export const ROTATE_STEP = 15;
export const TILT_STEP = 5;

/** How far a full-width drag turns the view. */
const DRAG_AZIMUTH = 360;
/** How far a full-height drag tilts it. */
const DRAG_TILT = 180;

// How far outside the surface the tick labels are laid. The fit below reserves
// room out to here rather than out to the rim, so a label is inside the box
// for the same reason a cell is.
const LABEL_RADIUS = 1.14;

// Points per floor ring. A circle in an orthographic projection is an ellipse,
// and 48 straight segments is under half a pixel of chord error at this size.
const RING_SEGMENTS = 48;

// Margins in the projected space. Room for the numbers along the two rulers.
const PAD = { left: 24, right: 14, top: 12, bottom: 16 };

const DEG = Math.PI / 180;

const clamp = (value, low, high) => Math.min(high, Math.max(low, value));

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

/**
 * A lux value at the axis, to three significant figures.
 *
 * The knots come from LUM_SURFACE_LUX_ROWS - evenly spaced in log light, not
 * in round numbers - so `String(value)` gives doubles like
 * `0.4472135954999579`. Three significant figures is what a reader can
 * actually use here: the fourth digit was never meaningful in the first
 * place, since the point it labels is a sampling knot rather than a
 * measurement. `toPrecision` rather than `toFixed`, because the digit count
 * has to move with the magnitude - two decimals says as much at 0.02 as
 * none does at 10.
 */
export function formatLux(value) {
  const number = Number(value);
  if (!Number.isFinite(number) || number <= 0) return String(value);
  return number.toPrecision(3);
}

/** Log light, floored the way the clock floors it. */
function logLux(lux) {
  return Math.log10(Math.max(Number.isFinite(lux) ? lux : 0, 1e-4));
}

/**
 * Ambient light as a radius on the disc, in [INNER_RADIUS, 1], brightest in.
 *
 * Log light still sets the spacing, but the direction is deliberately reversed:
 * the brightest measured room is the inner ring and the darkest the rim.
 * Clamped rather than allowed off the edge: a room outside anything measured
 * is exactly the case worth drawing, and a ring off the diagram says nothing.
 */
export function luxRadius(lux, min, max) {
  const low = logLux(min);
  const span = logLux(max) - low;
  if (!(span > 0)) return 1;
  const along = 1 - clamp((logLux(lux) - low) / span, 0, 1);
  return INNER_RADIUS + along * (1 - INNER_RADIUS);
}

/** Hue as an azimuth in radians, periodic by construction. */
export function hueAngle(hue, period = 360) {
  const cycle = period || 360;
  const wrapped = (((Number(hue) || 0) % cycle) + cycle) % cycle;
  return (wrapped / cycle) * 2 * Math.PI;
}

/** The percentage the model asks for, as a height in [0, 1]. */
export function percentHeight(percent, low, high) {
  const span = high - low || 1;
  return clamp(((Number(percent) || 0) - low) / span, 0, 1);
}

/**
 * A cylindrical point, seen from `view`, in the projection's own units.
 *
 * `x` is across and `y` is **up** - not the screen's y, which grows downwards;
 * the flip happens once, where the box is applied, rather than in every
 * caller. `depth` is how far the point is from the reader, so the painter's
 * algorithm sorts on it descending.
 *
 * Orthographic and not perspective, deliberately: a perspective cylinder makes
 * the near cells bigger than the far ones, and the size of a cell here means
 * nothing at all. Two cells the same size that are the same size is worth more
 * than a picture that looks photographic.
 */
export function projectCylindrical(radius, angle, height, view) {
  const { azimuth, tilt } = normaliseView(view);
  const turned = angle + azimuth * DEG;
  const across = radius * Math.cos(turned);
  const along = radius * Math.sin(turned);
  const sin = Math.sin(tilt * DEG);
  const cos = Math.cos(tilt * DEG);
  return {
    x: across,
    y: along * sin + height * cos,
    depth: along * cos - height * sin
  };
}

/** A view with the azimuth wrapped and the tilt clamped, or the default. */
export function normaliseView(view) {
  const azimuth = Number(view?.azimuth);
  const tilt = Number(view?.tilt);
  if (!Number.isFinite(azimuth) || !Number.isFinite(tilt)) {
    return { azimuth: DEFAULT_VIEW.azimuth, tilt: DEFAULT_VIEW.tilt };
  }
  return { azimuth: ((azimuth % 360) + 360) % 360, tilt: clamp(tilt, TILT_MIN, TILT_MAX) };
}

/**
 * Whether a view is the default one, compared after normalising.
 *
 * The reset button asks this, and it has to ask it of the real numbers rather
 * than of the rounded ones the read-out shows: a view a fraction of a degree
 * away is not the default, and rounding would say it is.
 */
export function isDefaultView(view) {
  const normalised = normaliseView(view);
  return normalised.azimuth === DEFAULT_VIEW.azimuth && normalised.tilt === DEFAULT_VIEW.tilt;
}

/**
 * The view after a drag of `dx`, `dy` pixels across a box of `size`.
 *
 * Across the whole width is one whole turn, which is the only mapping that
 * needs no explaining: the far side of the cylinder is half a swipe away
 * wherever the finger starts. Dragging **down** raises the camera, the way
 * every other 3D viewer on the device does it - pulling the near edge towards
 * you tips the top into sight.
 *
 * A zero-sized box is what a tab that has never been shown measures as, and
 * dividing by it would put NaN into every coordinate on the page.
 */
export function dragView(view, dx, dy, size) {
  const width = size?.width;
  const height = size?.height;
  const current = normaliseView(view);
  if (!(width > 0) || !(height > 0)) return current;
  return normaliseView({
    azimuth: current.azimuth - (dx / width) * DRAG_AZIMUTH,
    tilt: current.tilt + (dy / height) * DRAG_TILT
  });
}

/**
 * The view after a key press, and whether the press was ours.
 *
 * `handled` is what decides whether the component calls `preventDefault`, so a
 * false here is Tab still moving focus and PageDown still scrolling the page.
 * Home is the keyboard half of the reset button; `0` is the same thing for a
 * keyboard that has no Home, which is most phone keyboards.
 */
export function keyView(view, key) {
  const current = normaliseView(view);
  switch (key) {
    case 'ArrowLeft':
      return { view: normaliseView({ ...current, azimuth: current.azimuth - ROTATE_STEP }), handled: true };
    case 'ArrowRight':
      return { view: normaliseView({ ...current, azimuth: current.azimuth + ROTATE_STEP }), handled: true };
    case 'ArrowUp':
      return { view: normaliseView({ ...current, tilt: current.tilt + TILT_STEP }), handled: true };
    case 'ArrowDown':
      return { view: normaliseView({ ...current, tilt: current.tilt - TILT_STEP }), handled: true };
    case 'Home':
    case '0':
      return { view: normaliseView(DEFAULT_VIEW), handled: true };
    default:
      return { view: current, handled: false };
  }
}

/** Whether the response is a grid this can draw at all. */
function usable(surface) {
  if (!surface || surface.valid === false) return false;
  const { lux, hue, percent } = surface;
  if (!Array.isArray(lux) || !Array.isArray(hue) || !Array.isArray(percent)) return false;
  // Two of each: one ambient level is a ring and one hue is a spoke, and
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
 * `options.view` is where the reader has turned it to; leaving it out gives
 * `DEFAULT_VIEW`, so a caller that never rotates behaves exactly as one that
 * cannot.
 *
 * `options.point` is where the clock is right now, and it is drawn on the
 * surface rather than beside it: the number is already in the read-out, and
 * what the diagram adds is *where* on the model it sits.
 *
 * `options.points` is Table 2 - what the owner has taught the clock - each
 * entry `{lux, hue, percent, bound}` already converted to a percentage on
 * this same sat-100 surface by the firmware (see WebRoutes.cpp), because the
 * conversion from a taught decades-residual to a percentage is the inversion
 * `FactoryProfile::evaluate` already does and a second implementation here
 * would be the thing this project's own rule warns against. A white point
 * (drawn nowhere on a diagram whose one axis is hue) is left out by the
 * caller, not filtered here.
 */
export function projectSurface(surface, options = {}) {
  const width = options.width ?? 320;
  const height = options.height ?? 240;
  const view = normaliseView(options.view ?? DEFAULT_VIEW);
  const empty = {
    empty: true, width, height, view, quads: [],
    floor: { rings: [], spokes: [] },
    axes: { lux: [], hue: [], percent: [] }, point: null, points: [],
    range: { min: 0, max: 100 }
  };
  if (!usable(surface)) return empty;

  const { lux, hue, percent } = surface;
  const limited = surface.limited ?? [];
  const bound = surface.bound ?? [];
  const low = options.minPercent ?? surface.minPercent ?? 20;
  const high = options.maxPercent ?? surface.maxPercent ?? 100;
  const period = options.huePeriod ?? 360;

  const luxLow = lux[0];
  const luxHigh = lux[lux.length - 1];

  // The fit. The projected drawing spans 2 * LABEL_RADIUS across, and
  // LABEL_RADIUS * 2 * sin(tilt) + cos(tilt) up and down - the disc seen at an
  // angle, plus the one unit of height standing on it. Computed rather than
  // measured off the points, so it is an upper bound for every cell at every
  // view and the drawing cannot walk out of the box at some angle nobody
  // happened to screenshot.
  const sin = Math.sin(view.tilt * DEG);
  const cos = Math.cos(view.tilt * DEG);
  const roomX = width - PAD.left - PAD.right;
  const roomY = height - PAD.top - PAD.bottom;
  const spanX = 2 * LABEL_RADIUS;
  const spanY = 2 * LABEL_RADIUS * sin + cos;
  const scale = Math.min(roomX / spanX, roomY / spanY);
  const originX = PAD.left + roomX / 2;
  const originY = PAD.top + (roomY - spanY * scale) / 2 + (LABEL_RADIUS * sin + cos) * scale;

  /** Cylindrical coordinates to the screen, y flipped once and here only. */
  const at = (radius, angle, heightUnit) => {
    const p = projectCylindrical(radius, angle, heightUnit, view);
    return [originX + p.x * scale, originY - p.y * scale];
  };
  const depthAt = (radius, angle, heightUnit) =>
    projectCylindrical(radius, angle, heightUnit, view).depth;

  const r = (row) => luxRadius(lux[row], luxLow, luxHigh);
  const a = (col) => hueAngle(hue[col], period);
  const h = (value) => percentHeight(value, low, high);

  /** A cell flag is the OR of the four grid corners the quad is drawn between. */
  const corner = (grid, row, col, next) =>
    Boolean(grid?.[row]?.[col]) || Boolean(grid?.[row]?.[next])
    || Boolean(grid?.[row + 1]?.[col]) || Boolean(grid?.[row + 1]?.[next]);

  const quads = [];
  for (let row = 0; row + 1 < lux.length; row++) {
    for (let col = 0; col < hue.length; col++) {
      const next = (col + 1) % hue.length;
      const wraps = next === 0;
      // The far edge of the wrap segment is the first hue again, one whole
      // period along. Written as `angle(0) + 2pi` rather than as `angle(0)`
      // so the arithmetic is the same arithmetic every other segment does -
      // and because a cosine is periodic, the two land on the same pixel,
      // which is what makes the joint a joint rather than a coincidence.
      const nearAngle = a(col);
      const farAngle = wraps ? a(0) + 2 * Math.PI : a(next);

      const corners = [
        at(r(row), nearAngle, h(percent[row][col])),
        at(r(row), farAngle, h(percent[row][next])),
        at(r(row + 1), farAngle, h(percent[row + 1][next])),
        at(r(row + 1), nearAngle, h(percent[row + 1][col]))
      ];

      quads.push({
        points: corners,
        d: `M${corners.map(([x, y]) => `${x.toFixed(2)} ${y.toFixed(2)}`).join('L')}Z`,
        hue: hue[col],
        colour: bandColour(hue[col]),
        percent: percent[row][col],
        lux: lux[row],
        // The ring this cell sits on, so a test can say out loud that one
        // ambient level is one radius - the cylinder's whole claim, and a
        // thing that would read as a circle while being a spiral.
        radius: r(row),
        wraps,
        // Both flags travel, because they say different things: `limited` is
        // the colour running out of slider, which is the gamut and not a
        // fault, and `bound` is a corner of the measurement that only ever
        // said "at least". A cell can be both.
        //
        // **All four corners**, and the two at `next` are the ones that get
        // forgotten. A quad spans two ambient levels *and* two hues; reading
        // only the pair at `col` marks the patch to one side of a limited
        // corner and leaves the other clean, which draws the boundary half a
        // cell away from where the gamut actually runs out and does it
        // silently. On the wrap segment `next` is column 0, so the seam reads
        // its far corners from the first column rather than from one past the
        // last.
        limited: corner(limited, row, col, next),
        bound: corner(bound, row, col, next),
        // Painter's order: how far the middle of the cell is from the reader.
        // Sorted descending below, so the far side is drawn first and the near
        // side over it - which on a closed cylinder is not a refinement, it is
        // the difference between a surface and a tangle.
        depth: (depthAt(r(row), nearAngle, h(percent[row][col]))
                + depthAt(r(row), farAngle, h(percent[row][next]))
                + depthAt(r(row + 1), farAngle, h(percent[row + 1][next]))
                + depthAt(r(row + 1), nearAngle, h(percent[row + 1][col]))) / 4
      });
    }
  }
  quads.sort((one, other) => other.depth - one.depth);

  // The floor: one ring per ambient level and a spoke per labelled hue, on the
  // plane the surface stands on. It is what makes the log radius legible -
  // without it the rings are only implied by where the colour bands bend, and
  // "which circle is one lux" has no answer on the screen.
  const rings = lux.map((value, row) => {
    const points = [];
    for (let step = 0; step < RING_SEGMENTS; step++) {
      points.push(at(r(row), (step / RING_SEGMENTS) * 2 * Math.PI, 0));
    }
    return {
      lux: value,
      d: `M${points.map(([x, y]) => `${x.toFixed(2)} ${y.toFixed(2)}`).join('L')}Z`
    };
  });

  // The ray the light ruler is laid along: the one pointing straight at the
  // reader, which is where the turn puts sin(angle + azimuth) at -1. It moves
  // with the view on purpose - a fixed ray would rotate into the far side of
  // the cylinder and put the numbers behind the surface.
  const frontAngle = -Math.PI / 2 - view.azimuth * DEG;
  // And the left silhouette, for the percentage ruler, so the two rulers are
  // a quarter turn apart at every view rather than on top of each other.
  const sideAngle = Math.PI - view.azimuth * DEG;

  const spokes = [];
  const hueTicks = [];
  for (let degrees = 0; degrees < period; degrees += 60) {
    const angle = hueAngle(degrees, period);
    const [ix, iy] = at(INNER_RADIUS, angle, 0);
    const [ox, oy] = at(1, angle, 0);
    spokes.push({ value: degrees, d: `M${ix.toFixed(2)} ${iy.toFixed(2)}L${ox.toFixed(2)} ${oy.toFixed(2)}` });
    const [x, y] = at(LABEL_RADIUS, angle, 0);
    hueTicks.push({
      value: degrees, label: `${degrees}°`, x, y,
      colour: bandColour(degrees),
      // So the component can fade the labels on the far side. They are still
      // drawn: a rim that lost half its numbers when turned would read as a
      // drawing error rather than as depth.
      depth: depthAt(LABEL_RADIUS, angle, 0)
    });
  }

  const axes = {
    lux: lux.map((value, row) => {
      const [x, y] = at(r(row), frontAngle, 0);
      return { value, label: formatLux(value), x, y, depth: depthAt(r(row), frontAngle, 0) };
    }),
    // Labelled at the knots the model was measured at, not at every column:
    // 24 labels round a 320 px disc is a grey smear.
    hue: hueTicks,
    percent: []
  };
  for (let value = Math.ceil(low / 20) * 20; value <= high; value += 20) {
    const [x, y] = at(LABEL_RADIUS, sideAngle, h(value));
    axes.percent.push({ value, label: `${value}`, x, y });
  }

  /**
   * One marker, projected the same way a cell corner is.
   *
   * Shared between `options.point` (one, the clock's own) and every entry of
   * `options.points` (Table 2), because both are "a light, a hue and a
   * percentage, shown on the surface" and the clamping rule - off the
   * measured range is exactly the case worth showing, so the marker sits on
   * the rim and admits it rather than floating in the margin - applies to
   * either the same way.
   */
  const markerAt = (spec) => {
    if (!spec || !Number.isFinite(spec.lux) || !Number.isFinite(spec.percent)) return null;
    const clamped = spec.lux < luxLow || spec.lux > luxHigh;
    const angle = hueAngle(spec.hue, period);
    const radius = luxRadius(spec.lux, luxLow, luxHigh);
    const [px, py] = at(radius, angle, h(spec.percent));
    return { x: px, y: py, depth: depthAt(radius, angle, h(spec.percent)), clamped, ...spec };
  };

  const point = markerAt(options.point);
  const points = Array.isArray(options.points)
    ? options.points.map(markerAt).filter(Boolean)
    : [];

  return {
    empty: false, width, height, view, quads,
    floor: { rings, spokes }, axes, point, points,
    range: { min: low, max: high }
  };
}
