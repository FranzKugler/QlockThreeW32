/**
 * The cylindrical surface: the geometry, without a browser.
 *
 * The diagram below the brightness curve draws what the factory profile asks
 * for over two axes at once - ambient light and hue - and the only part of it
 * worth testing is the part that can be wrong without looking wrong: which
 * cell is where, whether the hue axis closes on itself, whether the drawing
 * stays inside the box it was given, and whether turning the view is the same
 * turn twice. A screenshot cannot answer any of those and a person looking at
 * a rotated 3D plot cannot either.
 *
 * **Why a cylinder.** Hue is an angle - it has no first and no last value, and
 * a straight axis has to cut it somewhere, which puts a seam through the
 * middle of a model that has none. Wrapped round, the seam is a joint rather
 * than an edge, and every test below that mentions "seam" is checking that
 * joint really closes rather than merely being drawn twice.
 *
 * So the projection is a plain module with no Svelte and no DOM, and this runs
 * it under `node --test`. The interaction helpers are in here too, for the same
 * reason: a drag that turns the wrong way, or a keyboard step that cannot get
 * back to where it started, is a bug with a number attached.
 */
import test from 'node:test';
import assert from 'node:assert/strict';

import {
  projectSurface, bandColour,
  luxRadius, hueAngle, percentHeight, projectCylindrical,
  DEFAULT_VIEW, INNER_RADIUS, TILT_MIN, TILT_MAX, ROTATE_STEP, TILT_STEP,
  normaliseView, dragView, keyView
} from '../../web/src/lib/surface3d.js';

/** A surface the shape the clock sends: six ambient levels, 24 hues. */
function surface(overrides = {}) {
  const lux = [0.02, 0.07, 0.15, 0.5, 1.9, 10];
  const hue = Array.from({ length: 24 }, (_, i) => i * 15);
  const percent = lux.map((_, row) =>
    hue.map((_, col) => 20 + row * 12 + (col % 4) * 2));
  return {
    valid: true,
    lux,
    hue,
    percent,
    limited: lux.map(() => hue.map(() => false)),
    bound: lux.map(() => hue.map(() => false)),
    minPercent: 20,
    maxPercent: 100,
    ...overrides
  };
}

const BOX = { width: 320, height: 240 };

/** Every view a drag or a key press can reach, coarsely. */
function views() {
  const out = [];
  for (let azimuth = 0; azimuth < 360; azimuth += 23) {
    for (const tilt of [TILT_MIN, 20, 45, TILT_MAX]) out.push({ azimuth, tilt });
  }
  return out;
}

const near = (a, b, epsilon = 1e-9) =>
  assert.ok(Math.abs(a - b) < epsilon, `${a} is not ${b}`);

// --- the three mappings, on their own -------------------------------------

test('the radius is log light, not light', () => {
  // 0.02 to 10 is a factor of five hundred. On a linear radius everything
  // below one lux - which is every evening this clock is read in - lands in
  // the innermost two per cent of the disc and cannot be told apart.
  const r = (lux) => luxRadius(lux, 0.1, 10);
  // A decade is a decade wherever it sits: 0.1 -> 1 must cover exactly as much
  // radius as 1 -> 10.
  near(r(1) - r(0.1), r(10) - r(1), 1e-12);
  // And the ends are the ends.
  near(r(10), INNER_RADIUS);
  near(r(0.1), 1);
});

test('the innermost ring is a ring, not the axis', () => {
  // A radius that started at zero would collapse the dimmest ambient level
  // onto a single point, and every cell of that row into a triangle with no
  // hue axis left. The dimmest room is the one the automatic is judged on.
  assert.ok(INNER_RADIUS > 0);
  assert.ok(INNER_RADIUS < 1);
});

test('the radius shrinks with the light, and never leaves the disc', () => {
  let previous = Infinity;
  for (const lux of [0.02, 0.07, 0.15, 0.5, 1.9, 10]) {
    const r = luxRadius(lux, 0.02, 10);
    assert.ok(r < previous, `${lux} lx did not move inwards`);
    assert.ok(r >= INNER_RADIUS && r <= 1, `${r} is off the disc`);
    previous = r;
  }
});

test('light outside the measured range is held on the disc', () => {
  // A room brighter than anything the profile knows is exactly the case worth
  // drawing, and a marker outside the diagram says nothing about where on the
  // model the clock is.
  near(luxRadius(5000, 0.02, 10), INNER_RADIUS);
  near(luxRadius(1e-9, 0.02, 10), 1);
  // Zero and nonsense do not produce NaN, which would poison a path string.
  assert.ok(Number.isFinite(luxRadius(0, 0.02, 10)));
});

test('hue is an angle, and the angle wraps', () => {
  near(hueAngle(0, 360), 0);
  near(hueAngle(90, 360), Math.PI / 2);
  near(hueAngle(180, 360), Math.PI);
  // 360 is 0 again. A hue axis that stopped would put a seam where the model
  // has none.
  near(hueAngle(360, 360), hueAngle(0, 360));
  near(hueAngle(-15, 360), hueAngle(345, 360));
  near(hueAngle(375, 360), hueAngle(15, 360));
});

test('percent is height, clamped to the regulated range', () => {
  near(percentHeight(20, 20, 100), 0);
  near(percentHeight(100, 20, 100), 1);
  near(percentHeight(60, 20, 100), 0.5);
  // Outside the range it stops rather than climbing out of the box.
  near(percentHeight(140, 20, 100), 1);
  near(percentHeight(0, 20, 100), 0);
});

// --- the view -------------------------------------------------------------

test('the same view projects the same way twice', () => {
  const a = projectCylindrical(0.7, 1.2, 0.4, DEFAULT_VIEW);
  const b = projectCylindrical(0.7, 1.2, 0.4, DEFAULT_VIEW);
  assert.deepEqual(a, b);
});

test('turning the view and turning the point are the same turn', () => {
  // The one invariant that says the rotation is a rotation: moving a point
  // thirty degrees round the cylinder must look exactly like turning the
  // camera thirty degrees the other way.
  const view = { azimuth: 40, tilt: 30 };
  const turned = { azimuth: 40 - 30, tilt: 30 };
  const a = projectCylindrical(0.8, hueAngle(90, 360), 0.5, view);
  const b = projectCylindrical(0.8, hueAngle(120, 360), 0.5, turned);
  near(a.x, b.x, 1e-12);
  near(a.y, b.y, 1e-12);
});

test('a whole turn of the view is no turn at all', () => {
  const a = projectCylindrical(0.6, 0.9, 0.3, { azimuth: 17, tilt: 33 });
  const b = projectCylindrical(0.6, 0.9, 0.3, { azimuth: 377, tilt: 33 });
  near(a.x, b.x, 1e-12);
  near(a.y, b.y, 1e-12);
});

test('up is up at every tilt the view can reach', () => {
  // y is up-positive in the projection's own space. More percent must mean
  // higher, whatever the camera is doing, or the diagram lies about the one
  // thing a reader assumes without being told.
  for (const view of views()) {
    const low = projectCylindrical(0.5, 1.0, 0.2, view);
    const high = projectCylindrical(0.5, 1.0, 0.8, view);
    assert.ok(high.y > low.y, `tilt ${view.tilt} put brighter lower`);
  }
});

test('depth is which of two points is further from the reader', () => {
  // Edge on, the far side of the cylinder is the one at positive y after the
  // turn; the painter's algorithm has nothing else to sort by.
  const flat = { azimuth: 0, tilt: TILT_MIN };
  const front = projectCylindrical(1, hueAngle(270, 360), 0.5, flat);
  const back = projectCylindrical(1, hueAngle(90, 360), 0.5, flat);
  assert.ok(back.depth > front.depth, 'the far side is not further away');
});

test('the view is wrapped and clamped rather than refused', () => {
  assert.deepEqual(normaliseView({ azimuth: 370, tilt: 30 }), { azimuth: 10, tilt: 30 });
  assert.deepEqual(normaliseView({ azimuth: -10, tilt: 30 }), { azimuth: 350, tilt: 30 });
  assert.equal(normaliseView({ azimuth: 0, tilt: 500 }).tilt, TILT_MAX);
  assert.equal(normaliseView({ azimuth: 0, tilt: -500 }).tilt, TILT_MIN);
  // Nonsense in is the default out, not NaN into a path string.
  assert.deepEqual(normaliseView({ azimuth: NaN, tilt: NaN }), DEFAULT_VIEW);
  assert.deepEqual(normaliseView(null), DEFAULT_VIEW);
});

test('the default view is one the clamps allow', () => {
  assert.deepEqual(normaliseView(DEFAULT_VIEW), DEFAULT_VIEW);
  assert.ok(DEFAULT_VIEW.tilt >= TILT_MIN && DEFAULT_VIEW.tilt <= TILT_MAX);
});

// --- dragging and typing --------------------------------------------------

test('a drag across a quarter of the width is a quarter turn', () => {
  const size = { width: 320, height: 240 };
  const turned = dragView({ azimuth: 100, tilt: 30 }, size.width / 4, 0, size);
  assert.equal(turned.azimuth, 100 - 90);
  assert.equal(turned.tilt, 30);
  // And the other way is the other way.
  assert.equal(dragView({ azimuth: 100, tilt: 30 }, -size.width / 4, 0, size).azimuth,
               100 + 90);
});

test('a drag past the seam wraps instead of stopping', () => {
  const size = { width: 360, height: 240 };
  const turned = dragView({ azimuth: 10, tilt: 30 }, size.width / 4, 0, size);
  assert.equal(turned.azimuth, 280);
});

test('dragging down looks down on the surface, and the tilt has ends', () => {
  const size = { width: 320, height: 240 };
  const down = dragView({ azimuth: 0, tilt: 30 }, 0, size.height / 4, size);
  assert.ok(down.tilt > 30, 'dragging down did not raise the camera');
  assert.equal(dragView({ azimuth: 0, tilt: 30 }, 0, 10 * size.height, size).tilt, TILT_MAX);
  assert.equal(dragView({ azimuth: 0, tilt: 30 }, 0, -10 * size.height, size).tilt, TILT_MIN);
});

test('a drag of nothing changes nothing', () => {
  const view = { azimuth: 123, tilt: 41 };
  assert.deepEqual(dragView(view, 0, 0, { width: 320, height: 240 }), view);
  // A zero-sized box is what a hidden tab measures as, and dividing by it
  // would put NaN into every coordinate on the page.
  assert.deepEqual(dragView(view, 5, 5, { width: 0, height: 0 }), view);
});

test('the arrow keys turn the view and can get back', () => {
  const start = { azimuth: 100, tilt: 30 };
  const left = keyView(start, 'ArrowLeft');
  assert.equal(left.handled, true);
  assert.equal(left.view.azimuth, 100 - ROTATE_STEP);
  const back = keyView(left.view, 'ArrowRight');
  assert.deepEqual(back.view, start, 'left then right is not where it started');

  const up = keyView(start, 'ArrowUp');
  assert.equal(up.view.tilt, 30 + TILT_STEP);
  assert.deepEqual(keyView(up.view, 'ArrowDown').view, start);
});

test('the arrow keys wrap and clamp like every other way in', () => {
  assert.equal(keyView({ azimuth: 0, tilt: 30 }, 'ArrowLeft').view.azimuth, 360 - ROTATE_STEP);
  assert.equal(keyView({ azimuth: 0, tilt: TILT_MAX }, 'ArrowUp').view.tilt, TILT_MAX);
  assert.equal(keyView({ azimuth: 0, tilt: TILT_MIN }, 'ArrowDown').view.tilt, TILT_MIN);
});

test('there is a way back to the view everyone else is looking at', () => {
  // A rotatable diagram is only comparable with yesterday's if it can be put
  // back. Home is the keyboard half of the reset button.
  for (const key of ['Home', '0']) {
    const reset = keyView({ azimuth: 211, tilt: 77 }, key);
    assert.equal(reset.handled, true);
    assert.deepEqual(reset.view, DEFAULT_VIEW, `${key} did not reset the view`);
  }
});

test('a key the diagram does not use is left to the page', () => {
  // `handled` is what decides whether the component calls preventDefault, so
  // a false here is Tab still moving focus and PageDown still scrolling.
  const view = { azimuth: 10, tilt: 20 };
  for (const key of ['Tab', 'Enter', 'PageDown', 'a']) {
    const answer = keyView(view, key);
    assert.equal(answer.handled, false, `${key} was swallowed`);
    assert.deepEqual(answer.view, view);
  }
});

// --- the surface ----------------------------------------------------------

test('it projects every cell of the grid', () => {
  const drawn = projectSurface(surface(), BOX);
  // One quad per cell, and the hue axis closes on itself - the segment from
  // the last hue back to the first is a real segment and is exactly the one
  // an implementation forgets, because it is the only one whose far edge is
  // not the next column in the array.
  assert.equal(drawn.quads.length, (6 - 1) * 24);
});

test('the drawing stays inside the box at every view', () => {
  for (const view of views()) {
    const drawn = projectSurface(surface(), { ...BOX, view });
    for (const quad of drawn.quads) {
      for (const [x, y] of quad.points) {
        assert.ok(x >= 0 && x <= BOX.width, `x ${x} outside the box at ${JSON.stringify(view)}`);
        assert.ok(y >= 0 && y <= BOX.height, `y ${y} outside the box at ${JSON.stringify(view)}`);
      }
    }
  }
});

test('every cell at one ambient level is at one radius', () => {
  // The cylinder's whole claim: a ring is a lighting condition. If two cells
  // of a row sat at different radii the reader would be looking at a spiral
  // and reading it as a circle.
  const drawn = projectSurface(surface(), BOX);
  const byLux = new Map();
  for (const quad of drawn.quads) {
    const seen = byLux.get(quad.lux);
    if (seen === undefined) byLux.set(quad.lux, quad.radius);
    else near(quad.radius, seen, 1e-12);
  }
  assert.equal(byLux.size, 5, 'a quad per ambient step, less the outermost');
});

test('the rings move inwards as the light grows', () => {
  const drawn = projectSurface(surface(), BOX);
  const radii = surface().lux.slice(0, -1)
    .map((lux) => drawn.quads.find((quad) => quad.lux === lux).radius);
  for (let i = 1; i < radii.length; i++) {
    assert.ok(radii[i] < radii[i - 1], `ring ${i} did not move inward`);
  }
});

test('brighter is higher on the screen', () => {
  // y grows downwards in SVG, so more percent must mean less y - and it has to
  // be the same cell being compared, which sorting by depth does not promise.
  const at = (grid) => {
    const drawn = projectSurface(surface({ percent: grid }), BOX);
    const quad = drawn.quads.find((one) => one.lux === 0.5 && one.hue === 120);
    return quad.points[0][1];
  };
  const dim = surface().percent;
  const bright = dim.map((row) => row.map((v) => Math.min(100, v + 20)));
  assert.ok(at(bright) < at(dim));
});

test('it is deterministic', () => {
  const a = projectSurface(surface(), BOX);
  const b = projectSurface(surface(), BOX);
  assert.deepEqual(a.quads, b.quads);
  // Including the view it was not given: the default has to be a constant,
  // not something read off the clock.
  assert.deepEqual(a.view, DEFAULT_VIEW);
});

test('turning the view moves the drawing', () => {
  // The other half of determinism: a view argument that quietly did nothing
  // would pass every test above.
  const still = projectSurface(surface(), { ...BOX, view: DEFAULT_VIEW });
  const turned = projectSurface(surface(), {
    ...BOX, view: { azimuth: DEFAULT_VIEW.azimuth + 90, tilt: DEFAULT_VIEW.tilt }
  });
  assert.notDeepEqual(still.quads[0].points, turned.quads[0].points);
  assert.deepEqual(turned.view, normaliseView({
    azimuth: DEFAULT_VIEW.azimuth + 90, tilt: DEFAULT_VIEW.tilt
  }));
});

test('the seam closes on itself rather than being drawn twice', () => {
  const drawn = projectSurface(surface(), BOX);
  const seam = drawn.quads.filter((quad) => quad.wraps);
  assert.equal(seam.length, 6 - 1, 'one wrap quad per ambient step');
  assert.equal(seam[0].hue, 345);

  // And it really joins: the far edge of the seam quad has to land exactly on
  // the near edge of the hue-0 quad at the same ambient level. Drawn as a
  // straight axis this is the segment that gets forgotten; drawn as a
  // cylinder it is the segment that gives the whole thing away if the angle
  // is not periodic.
  for (const quad of seam) {
    const first = drawn.quads.find((one) => one.lux === quad.lux && one.hue === 0);
    near(quad.points[1][0], first.points[0][0], 1e-9);
    near(quad.points[1][1], first.points[0][1], 1e-9);
    near(quad.points[2][0], first.points[3][0], 1e-9);
    near(quad.points[2][1], first.points[3][1], 1e-9);
  }
});

test('the seam closes at every view, not only the default one', () => {
  for (const view of views()) {
    const drawn = projectSurface(surface(), { ...BOX, view });
    const seam = drawn.quads.find((quad) => quad.wraps && quad.lux === 0.5);
    const first = drawn.quads.find((one) => one.lux === 0.5 && one.hue === 0);
    near(seam.points[1][0], first.points[0][0], 1e-9);
    near(seam.points[1][1], first.points[0][1], 1e-9);
  }
});

test('limited and bound cells are marked rather than hidden', () => {
  const marked = surface();
  marked.limited[5][8] = true;
  marked.bound[4][2] = true;
  const drawn = projectSurface(marked, BOX);
  assert.ok(drawn.quads.some((quad) => quad.limited));
  assert.ok(drawn.quads.some((quad) => quad.bound));
  // A cell that is both is not counted twice: the two say different things
  // and the drawing has to be able to show both.
  assert.ok(drawn.quads.every((quad) => typeof quad.limited === 'boolean'));
});

test('a quad is marked when any of its four corners is', () => {
  // A quad spans two ambient levels *and* two hues, so it has four corners and
  // the flag has to be the OR of all four. Reading only the two at `col` marks
  // the patch to the left of a limited corner and leaves the one to its right
  // clean - which draws a boundary half a cell away from where the gamut
  // actually runs out, and does it silently.
  for (const [dRow, dCol] of [[0, 0], [0, 1], [1, 0], [1, 1]]) {
    const marked = surface();
    marked.limited[2 + dRow][5 + dCol] = true;
    marked.bound[2 + dRow][5 + dCol] = true;
    const drawn = projectSurface(marked, BOX);
    const quad = drawn.quads.find((one) => one.lux === marked.lux[2] && one.hue === 5 * 15);
    assert.ok(quad, `no quad at row 2, col 5 for corner ${dRow},${dCol}`);
    assert.equal(quad.limited, true, `limited missed at corner ${dRow},${dCol}`);
    assert.equal(quad.bound, true, `bound missed at corner ${dRow},${dCol}`);
  }
});

test('and the wrap quad reads its far corners from the first column', () => {
  // The far edge of the seam segment is hue 0, which is column 0 - not column
  // 24, which does not exist. A flag set there has to reach the wrap quad.
  const marked = surface();
  marked.limited[3][0] = true;
  const drawn = projectSurface(marked, BOX);
  const seam = drawn.quads.find((quad) => quad.wraps && quad.lux === marked.lux[3]);
  assert.ok(seam);
  assert.equal(seam.limited, true);
});

test('the near cells are drawn last', () => {
  // Painter's algorithm, and it is the only thing standing between this and a
  // surface whose far side is drawn over its near side.
  const drawn = projectSurface(surface(), BOX);
  for (let i = 1; i < drawn.quads.length; i++) {
    assert.ok(drawn.quads[i].depth <= drawn.quads[i - 1].depth,
              'the quads are not sorted from the back');
  }
});

test('the axes carry readable numbers', () => {
  const drawn = projectSurface(surface(), BOX);
  assert.deepEqual(drawn.axes.lux.map((tick) => tick.label),
                   ['0.02', '0.07', '0.15', '0.5', '1.9', '10']);
  // Hue is labelled at the knots, not at every column: 24 labels round a
  // 320 px disc is a grey smear.
  assert.deepEqual(drawn.axes.hue.map((tick) => tick.value),
                   [0, 60, 120, 180, 240, 300]);
  assert.deepEqual(drawn.axes.percent.map((tick) => tick.value), [20, 40, 60, 80, 100]);
});

test('the light labels sit on the side facing the reader', () => {
  // They are laid along one radius, and a radius that rotated into the far
  // side of the cylinder would put the numbers behind the surface. Whichever
  // way the view is turned, the labelled ray has to be the near one.
  for (const view of views()) {
    const drawn = projectSurface(surface(), { ...BOX, view });
    const darkOuter = drawn.axes.lux[0];
    const brightInner = drawn.axes.lux[drawn.axes.lux.length - 1];
    assert.ok(darkOuter.depth < brightInner.depth,
              `the light axis points away from the reader at ${JSON.stringify(view)}`);
  }
});

test('the floor draws one ring per ambient level', () => {
  const drawn = projectSurface(surface(), BOX);
  assert.equal(drawn.floor.rings.length, 6);
  assert.equal(drawn.floor.spokes.length, 6);
  for (const ring of drawn.floor.rings) {
    assert.ok(ring.d.startsWith('M'));
    assert.ok(ring.d.endsWith('Z'), 'a ring that does not close is not a ring');
  }
});

test('the operating point is placed on the surface', () => {
  const drawn = projectSurface(surface(), { ...BOX, point: { lux: 0.5, hue: 240, percent: 68 } });
  assert.ok(drawn.point, 'there is a point');
  assert.ok(drawn.point.x >= 0 && drawn.point.x <= BOX.width);
  assert.ok(drawn.point.y >= 0 && drawn.point.y <= BOX.height);
  assert.equal(drawn.point.clamped, false);
});

test('the operating point lands where its own cell is', () => {
  // The strongest statement available about the point: sitting it exactly on
  // a grid knot has to put it exactly on that knot's corner, or the marker is
  // describing a different model from the surface underneath it.
  const model = surface();
  const drawn = projectSurface(model, {
    ...BOX, point: { lux: model.lux[3], hue: 120, percent: model.percent[3][8] }
  });
  const quad = drawn.quads.find((one) => one.lux === model.lux[3] && one.hue === 120);
  near(drawn.point.x, quad.points[0][0], 1e-9);
  near(drawn.point.y, quad.points[0][1], 1e-9);
});

test('a point outside the measured range is clamped onto it, and says so', () => {
  const drawn = projectSurface(surface(), { ...BOX, point: { lux: 5000, hue: 10, percent: 100 } });
  assert.equal(drawn.point.clamped, true);
  assert.ok(drawn.point.x <= BOX.width);
  assert.ok(Number.isFinite(drawn.point.y));
});

test('the point wraps round the seam like everything else', () => {
  const a = projectSurface(surface(), { ...BOX, point: { lux: 0.5, hue: 0, percent: 60 } });
  const b = projectSurface(surface(), { ...BOX, point: { lux: 0.5, hue: 360, percent: 60 } });
  near(a.point.x, b.point.x, 1e-9);
  near(a.point.y, b.point.y, 1e-9);
});

test('no point asked for is no point drawn', () => {
  assert.equal(projectSurface(surface(), BOX).point, null);
});

test('a surface with nothing in it draws nothing and does not throw', () => {
  for (const empty of [
    null,
    { valid: false },
    { valid: true, lux: [], hue: [], percent: [] },
    { valid: true, lux: [1], hue: [0], percent: [[50]] }
  ]) {
    const drawn = projectSurface(empty, BOX);
    assert.equal(drawn.quads.length, 0);
    assert.equal(drawn.empty, true);
    // The component reads the view off the answer to show the read-out, so an
    // empty one still has to carry a whole view rather than undefined.
    assert.deepEqual(drawn.view, DEFAULT_VIEW);
    assert.deepEqual(drawn.floor, { rings: [], spokes: [] });
  }
});

test('a ragged grid is refused rather than half drawn', () => {
  const ragged = surface();
  ragged.percent[2] = ragged.percent[2].slice(0, 3);
  const drawn = projectSurface(ragged, BOX);
  assert.equal(drawn.empty, true);
  // Half a surface looks like a measurement with a hole in it, which is a
  // different and much more alarming thing than a response that did not
  // arrive.
  assert.equal(drawn.quads.length, 0);
});

test('the band colour is the hue itself, at a lightness both themes can show', () => {
  assert.equal(bandColour(0), 'hsl(0 72% 52%)');
  assert.equal(bandColour(240), 'hsl(240 72% 52%)');
  // Wrapped rather than clamped: 360 is 0, and a colour wheel that stopped
  // would put a seam where the model has none.
  assert.equal(bandColour(360), bandColour(0));
  assert.equal(bandColour(-30), bandColour(330));
});
