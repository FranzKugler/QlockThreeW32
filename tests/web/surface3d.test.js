/**
 * The isometric surface: the geometry, without a browser.
 *
 * The diagram below the brightness curve draws what the factory profile asks
 * for over two axes at once - ambient light and hue - and the only part of it
 * worth testing is the part that can be wrong without looking wrong: which
 * cell is where, whether the hue axis closes on itself, and whether the
 * drawing stays inside the box it was given. A screenshot cannot answer any of
 * those and a person looking at an isometric plot cannot either.
 *
 * So the projection is a plain module with no Svelte and no DOM, and this runs
 * it under `node --test`.
 */
import test from 'node:test';
import assert from 'node:assert/strict';

import { projectSurface, bandColour } from '../../web/src/lib/surface3d.js';

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

const BOX = { width: 320, height: 220 };

test('it projects every cell of the grid', () => {
  const drawn = projectSurface(surface(), BOX);
  // One quad per cell, and the hue axis closes on itself - the segment from
  // the last hue back to the first is a real segment and is exactly the one
  // an implementation forgets, because it is the only one whose far edge is
  // not the next column in the array.
  assert.equal(drawn.quads.length, (6 - 1) * 24);
});

test('the drawing stays inside the box it was given', () => {
  const drawn = projectSurface(surface(), BOX);
  for (const quad of drawn.quads) {
    for (const [x, y] of quad.points) {
      assert.ok(x >= 0 && x <= BOX.width, `x ${x} outside the box`);
      assert.ok(y >= 0 && y <= BOX.height, `y ${y} outside the box`);
    }
  }
});

test('brighter is higher on the screen', () => {
  // The one thing a reader assumes without being told. y grows downwards in
  // SVG, so more percent must mean less y.
  const dim = projectSurface(surface(), BOX);
  const bright = projectSurface(surface({
    percent: surface().percent.map((row) => row.map((v) => Math.min(100, v + 20)))
  }), BOX);
  assert.ok(bright.quads[0].points[0][1] < dim.quads[0].points[0][1]);
});

test('it is deterministic', () => {
  const a = projectSurface(surface(), BOX);
  const b = projectSurface(surface(), BOX);
  assert.deepEqual(a.quads, b.quads);
});

test('the seam is drawn, not implied', () => {
  const drawn = projectSurface(surface(), BOX);
  const seam = drawn.quads.filter((quad) => quad.wraps);
  assert.equal(seam.length, 6 - 1, 'one wrap quad per ambient step');
  // And it really joins the last hue to the first, rather than doubling the
  // last column back on itself.
  assert.equal(seam[0].hue, 345);
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
  const hue = 24;
  for (const [dRow, dCol] of [[0, 0], [0, 1], [1, 0], [1, 1]]) {
    const marked = surface();
    const row = 2 + dRow;
    const col = 5 + dCol;
    marked.limited[row][col] = true;
    marked.bound[row][col] = true;
    const drawn = projectSurface(marked, BOX);
    // The quad whose near corner is (2, 5) has to carry both flags whichever
    // of its four corners was the marked one.
    const quad = drawn.quads.find((one) => one.lux === marked.lux[2] && one.hue === 5 * 15);
    assert.ok(quad, `no quad at row 2, col 5 for corner ${dRow},${dCol}`);
    assert.equal(quad.limited, true, `limited missed at corner ${dRow},${dCol}`);
    assert.equal(quad.bound, true, `bound missed at corner ${dRow},${dCol}`);
  }
  assert.equal(hue, 24);
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

test('the axes carry readable numbers', () => {
  const drawn = projectSurface(surface(), BOX);
  assert.deepEqual(drawn.axes.lux.map((tick) => tick.label),
                   ['0.02', '0.07', '0.15', '0.5', '1.9', '10']);
  // Hue is labelled at the knots, not at every column: 24 labels on a 320 px
  // box is a grey smear.
  assert.deepEqual(drawn.axes.hue.map((tick) => tick.value),
                   [0, 60, 120, 180, 240, 300]);
  assert.deepEqual(drawn.axes.percent.map((tick) => tick.value), [20, 40, 60, 80, 100]);
});

test('the operating point is placed on the surface', () => {
  const drawn = projectSurface(surface(), { ...BOX, point: { lux: 0.5, hue: 240, percent: 68 } });
  assert.ok(drawn.point, 'there is a point');
  assert.ok(drawn.point.x >= 0 && drawn.point.x <= BOX.width);
  assert.ok(drawn.point.y >= 0 && drawn.point.y <= BOX.height);
});

test('a point outside the measured range is clamped onto it, and says so', () => {
  const drawn = projectSurface(surface(), { ...BOX, point: { lux: 5000, hue: 10, percent: 100 } });
  assert.equal(drawn.point.clamped, true);
  assert.ok(drawn.point.x <= BOX.width);
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
