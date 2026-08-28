/**
 * "Put it back" has to be offered whenever there is something to put back.
 *
 * The surface below the brightness curve turns under a finger, and the way
 * back is a button that returns to exactly `DEFAULT_VIEW`. Whether that button
 * is offered was decided on the *read-out* - the two whole degrees printed
 * beside it - and a drag is not made of whole degrees. A finger that moves the
 * view to 325.4 / 26.4 leaves a picture nobody can reproduce, prints "325 / 26"
 * because that is what a read-out is for, and disables the one control that
 * would undo it. The diagram is then stuck a fraction of a degree from home
 * with no way of saying so.
 *
 * So the question is asked of the view the projection actually uses, not of
 * the numbers on screen, and it is asked in one place: a rounded comparison
 * written out at the call site is the same bug waiting to be written again.
 */
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';

import { DEFAULT_VIEW, isDefaultView } from '../../web/src/lib/surface3d.js';

const COMPONENT = fs.readFileSync(
  new URL('../../web/src/sections/LuminanceSurface.svelte', import.meta.url), 'utf8');

test('only the default view itself is the default view', () => {
  assert.equal(isDefaultView(DEFAULT_VIEW), true);
  assert.equal(isDefaultView({ ...DEFAULT_VIEW }), true);

  // The blocker: a drag lands here, the read-out rounds it to 325 / 26, and a
  // comparison against the read-out calls it home.
  assert.equal(isDefaultView({ azimuth: 325.4, tilt: 26.4 }), false);
  assert.equal(isDefaultView({ azimuth: 324.6, tilt: 25.6 }), false);
  // One axis is enough to be away from it.
  assert.equal(isDefaultView({ ...DEFAULT_VIEW, tilt: DEFAULT_VIEW.tilt + 0.4 }), false);
  assert.equal(isDefaultView({ ...DEFAULT_VIEW, azimuth: DEFAULT_VIEW.azimuth - 0.4 }), false);
});

test('it asks the normalised view, so a wrapped angle is still home', () => {
  // Azimuth is an angle and the drag keeps turning past 360; the view the
  // projection uses is the wrapped one, and that is the one to compare.
  assert.equal(isDefaultView({ ...DEFAULT_VIEW, azimuth: DEFAULT_VIEW.azimuth + 360 }), true);
  assert.equal(isDefaultView({ ...DEFAULT_VIEW, azimuth: DEFAULT_VIEW.azimuth - 720 }), true);
  // And nothing at all normalises to the default, so it is not "away from it".
  assert.equal(isDefaultView(null), true);
  assert.equal(isDefaultView({ azimuth: NaN, tilt: NaN }), true);
});

test('the reset button is not driven by the rounded read-out', () => {
  // Narrow on purpose: the arithmetic above is what is really under test, and
  // this is the one line that decides whether the component asks it. The
  // rounded pair still exists and is still what the read-out prints - it just
  // has no business deciding what is enabled.
  const decision = COMPONENT.match(/isDefault\s*=\s*\$derived\(([^;]*)\);/);
  assert.ok(decision, 'the component no longer derives isDefault');
  assert.match(decision[1], /isDefaultView\(/,
               'isDefault does not ask isDefaultView');
  assert.doesNotMatch(decision[1], /\bazimuth\b|\btilt\b/,
                      'isDefault still compares the rounded read-out values');
});
