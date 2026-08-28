/**
 * The settlement seam: what `settle()` in server.js does ten seconds after
 * the last slider move.
 *
 * Before this module existed, `settle()` called the legacy white-only
 * `remember()` unconditionally, so a nudge made against a colour-aware
 * factory profile was folded into the plain (lux, percent) curve instead of
 * into a colour-aware residual - the fit `remember()` maintains has no hue or
 * saturation axis, so a correction made in blue quietly became a correction
 * "in general" and the next white reading would drift by however far blue
 * differs from white (see CLAUDE.md, "Colour changes the level").
 *
 * Entirely offline: no timers, no network, no `currentLux()`, no `Date.now()`
 * - every input the seam needs arrives as an explicit argument, so the tests
 * describe exactly what the code did with exactly what it was given.
 */
import test from 'node:test';
import assert from 'node:assert/strict';
import { settleNudge } from '../../lib/settlement.js';

// A trivial, deterministic stand-in for the factory evaluator: "the factory
// wants white" (decades 0) everywhere, plus a colour term proportional to hue
// so tests can tell one colour's baseline from another's.
const factoryTarget = (lux, hue, sat) => (sat === 0 ? 0 : hue / 1000);

const decadesFromPercent = (percent) => 2.2 * Math.log10(percent / 100);

test('factory invalid: falls back to the legacy remember(), and only that', () => {
  const calls = [];
  const remember = (...args) => calls.push(args);
  const residuals = [];

  const result = settleNudge({
    factoryValid: false,
    lux: 3.2, percent: 55, hue: 200, sat: 80, maxPercent: 100,
    residuals, factoryTarget, remember
  });

  assert.equal(calls.length, 1);
  assert.deepEqual(calls[0], [3.2, 55, 200, 80]);
  assert.equal(residuals.length, 0, 'no residual is written on the legacy path');
  assert.equal(result.storedIn, 'legacy');
});

test('factory valid: never calls the legacy remember()', () => {
  let called = false;
  const remember = () => { called = true; };
  const residuals = [];

  settleNudge({
    factoryValid: true,
    lux: 1.0, percent: 50, hue: 90, sat: 100, maxPercent: 100,
    residuals, factoryTarget, remember
  });

  assert.equal(called, false);
});

test('factory valid: stores a colour-aware residual, in decades against the factory baseline', () => {
  const residuals = [];
  const lux = 1.0;
  const hue = 90;
  const sat = 100;
  const percent = 60;

  settleNudge({
    factoryValid: true,
    lux, percent, hue, sat, maxPercent: 100,
    residuals, factoryTarget, remember: () => assert.fail('must not be called')
  });

  assert.equal(residuals.length, 1);
  const [point] = residuals;
  const expectedDecades = decadesFromPercent(percent) - factoryTarget(lux, hue, sat);
  assert.ok(Math.abs(point.decades - expectedDecades) < 1e-9);
  assert.equal(point.lux, lux);
  assert.equal(point.hue, hue);
  assert.equal(point.sat, sat);
});

test('factory valid: a request at the ceiling is marked bound, one below it is not', () => {
  const atCeiling = [];
  settleNudge({
    factoryValid: true,
    lux: 5, percent: 100, hue: 10, sat: 50, maxPercent: 100,
    residuals: atCeiling, factoryTarget, remember: () => {}
  });
  assert.equal(atCeiling[0].bound, true);

  const belowCeiling = [];
  settleNudge({
    factoryValid: true,
    lux: 5, percent: 99, hue: 10, sat: 50, maxPercent: 100,
    residuals: belowCeiling, factoryTarget, remember: () => {}
  });
  assert.equal(belowCeiling[0].bound, false);
});

test('factory valid: white fit is left alone - only remember() ever mutates it', () => {
  // remember() is where the legacy curve lives (its points, its slope, its
  // offset). Proving the seam never mutates the curve is exactly proving it
  // never calls remember() on the factory-valid path, which the earlier test
  // already pins; this test names the intent explicitly so a future change
  // that starts calling remember() "just to be safe" fails here too.
  let rememberCalls = 0;
  const residuals = [];
  settleNudge({
    factoryValid: true,
    lux: 2, percent: 40, hue: 300, sat: 70, maxPercent: 100,
    residuals, factoryTarget,
    remember: () => { rememberCalls++; }
  });
  assert.equal(rememberCalls, 0);
});

test('factory valid: sat 0 canonicalizes hue, so white nudges from any hue share one identity', () => {
  const residuals = [];
  // Observes what settleNudge actually asks the factory for, so the test
  // proves the canonicalisation happens before the lookup - not only that the
  // stored point ends up canonical, which a bug fixing the field up
  // afterwards would also pass.
  const seenHues = [];
  const observingFactoryTarget = (lux, hue, sat) => {
    seenHues.push(hue);
    return factoryTarget(lux, hue, sat);
  };

  settleNudge({
    factoryValid: true,
    lux: 4, percent: 45, hue: 10, sat: 0, maxPercent: 100,
    residuals, factoryTarget: observingFactoryTarget, remember: () => {},
    seconds: 1, capacity: 8
  });
  settleNudge({
    factoryValid: true,
    lux: 4.1, percent: 50, hue: 250, sat: 0, maxPercent: 100,
    residuals, factoryTarget: observingFactoryTarget, remember: () => {},
    seconds: 2, capacity: 8
  });

  assert.deepEqual(seenHues, [0, 0], 'the factory is asked about the canonical hue, not the wheel hue');

  // Same light (within the near-light ratio), same canonical identity
  // (sat 0 regardless of hue) - the second nudge replaces the first rather
  // than joining it as a second point.
  assert.equal(residuals.length, 1);
  assert.equal(residuals[0].hue, 0);
  assert.equal(residuals[0].sat, 0);
  assert.equal(residuals[0].lux, 4.1);
});

test('factory valid: at sat > 0, identity is exact hue+sat - nearby colours stay separate', () => {
  const residuals = [];

  settleNudge({
    factoryValid: true,
    lux: 2, percent: 50, hue: 120, sat: 90, maxPercent: 100,
    residuals, factoryTarget, remember: () => {}
  });
  // Same light, a colour one degree away: a different identity, so this must
  // not overwrite the first point.
  settleNudge({
    factoryValid: true,
    lux: 2.05, percent: 55, hue: 121, sat: 90, maxPercent: 100,
    residuals, factoryTarget, remember: () => {}
  });

  assert.equal(residuals.length, 2, 'nearby but distinct colours are not merged');
  assert.equal(residuals[0].hue, 120);
  assert.equal(residuals[1].hue, 121);
});

test('factory valid: near-light replacement only fires within the same exact colour identity', () => {
  const residuals = [];

  settleNudge({
    factoryValid: true,
    lux: 2, percent: 50, hue: 120, sat: 90, maxPercent: 100,
    residuals, factoryTarget, remember: () => {}
  });
  // Same light, same identity: replaces.
  settleNudge({
    factoryValid: true,
    lux: 2.1, percent: 52, hue: 120, sat: 90, maxPercent: 100,
    residuals, factoryTarget, remember: () => {}
  });
  assert.equal(residuals.length, 1);
  assert.equal(residuals[0].lux, 2.1);

  // Same light again, but a different identity: must not replace the
  // existing point, must join as its own.
  settleNudge({
    factoryValid: true,
    lux: 2.1, percent: 52, hue: 200, sat: 30, maxPercent: 100,
    residuals, factoryTarget, remember: () => {}
  });
  assert.equal(residuals.length, 2);

  // Light far away, same identity as the first: must not replace it either.
  settleNudge({
    factoryValid: true,
    lux: 50, percent: 80, hue: 120, sat: 90, maxPercent: 100,
    residuals, factoryTarget, remember: () => {}
  });
  assert.equal(residuals.length, 3);
});

test('factory valid: a replacement returns the stored object, not a temporary, and refreshes it in place', () => {
  const residuals = [];

  const first = settleNudge({
    factoryValid: true,
    lux: 2, percent: 50, hue: 120, sat: 90, maxPercent: 100,
    residuals, factoryTarget, remember: () => {},
    seconds: 100, capacity: 8
  });
  assert.equal(first.point, residuals[0], 'the point returned on insertion is the object actually stored');

  const stored = residuals[0];
  const second = settleNudge({
    factoryValid: true,
    lux: 2.1, percent: 60, hue: 120, sat: 90, maxPercent: 100,
    residuals, factoryTarget, remember: () => {},
    seconds: 200, capacity: 8
  });

  // Still one point - it was a replacement, not an append.
  assert.equal(residuals.length, 1);
  // Same object identity as what is actually in the list: a caller that reads
  // `.point` back must see the live, mutated record, not a throwaway copy
  // that happens to hold the same values the moment it was returned.
  assert.equal(second.point, stored, 'replacement must return the stored object, not a temporary');
  assert.equal(residuals[0], stored);

  // And the decades, light and age are the *new* nudge's, not stale from the
  // first one that was replaced.
  const expectedDecades = decadesFromPercent(60) - factoryTarget(2.1, 120, 90);
  assert.ok(Math.abs(residuals[0].decades - expectedDecades) < 1e-9);
  assert.equal(residuals[0].lux, 2.1);
  assert.equal(residuals[0].seconds, 200, 'a replacement refreshes the recorded age, it does not keep the old one');
});

test('factory valid: a new residual carries the seconds it was explicitly given', () => {
  const residuals = [];

  settleNudge({
    factoryValid: true,
    lux: 3, percent: 50, hue: 10, sat: 50, maxPercent: 100,
    residuals, factoryTarget, remember: () => {},
    seconds: 4242, capacity: 8
  });

  assert.equal(residuals[0].seconds, 4242);
});

test('factory valid: a refreshed near-neighbour moves to the end, so eviction still targets the true oldest', () => {
  const residuals = [];
  const capacity = 2;

  // A (hue 0) is the oldest point.
  const a = settleNudge({
    factoryValid: true,
    lux: 1, percent: 50, hue: 0, sat: 80, maxPercent: 100,
    residuals, factoryTarget, remember: () => {},
    seconds: 1, capacity
  });
  // B (hue 40) joins as the second point.
  settleNudge({
    factoryValid: true,
    lux: 2, percent: 50, hue: 40, sat: 80, maxPercent: 100,
    residuals, factoryTarget, remember: () => {},
    seconds: 2, capacity
  });

  // A is refreshed - same identity and near light - which must move it to
  // the end of the oldest-first order, not merely update it in place.
  const refreshed = settleNudge({
    factoryValid: true,
    lux: 1.05, percent: 55, hue: 0, sat: 80, maxPercent: 100,
    residuals, factoryTarget, remember: () => {},
    seconds: 3, capacity
  });
  assert.equal(refreshed.point, a.point, 'the replacement is the same stored object as the original A');

  // Now C (hue 200, far from both) appends and pushes capacity. Since A was
  // refreshed most recently, B - not the refreshed A - is the true oldest and
  // must be the one evicted.
  settleNudge({
    factoryValid: true,
    lux: 50, percent: 60, hue: 200, sat: 30, maxPercent: 100,
    residuals, factoryTarget, remember: () => {},
    seconds: 4, capacity
  });

  assert.equal(residuals.length, capacity);
  assert.deepEqual(residuals.map((r) => r.hue), [0, 200], 'B was evicted; refreshed A survives');
  assert.equal(residuals[0], refreshed.point, 'the surviving A is still the very same stored object');
});

test('factory valid: the residual list is bounded to capacity, evicting the oldest first', () => {
  const residuals = [];
  const capacity = 3;

  // Five distinct colour identities (different hues), so every call appends
  // rather than replaces - this is purely about the capacity bound, not the
  // near-neighbour rule already covered above.
  for (let i = 0; i < 5; i++) {
    settleNudge({
      factoryValid: true,
      lux: 1 + i, percent: 50, hue: i * 40, sat: 80, maxPercent: 100,
      residuals, factoryTarget, remember: () => {},
      seconds: i, capacity
    });
  }

  assert.equal(residuals.length, capacity, 'the list never grows past capacity');
  // The two oldest (hue 0 and hue 40, seconds 0 and 1) were pushed out;
  // the three most recent remain, oldest-first.
  assert.deepEqual(residuals.map((r) => r.hue), [80, 120, 160]);
  assert.deepEqual(residuals.map((r) => r.seconds), [2, 3, 4]);
});
