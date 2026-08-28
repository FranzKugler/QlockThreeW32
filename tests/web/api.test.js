/**
 * The REST contract, against the mock that stands in for the clock.
 *
 * Changing this API means touching four places - the firmware handler,
 * `server.js`, `web/src/lib/api.js` and the proxy list in `vite.config.js` -
 * and the project has already been bitten once by a *behaviour* drifting while
 * every field name still matched. So this checks the shape the screen depends
 * on, on the half that can be run without a clock, and the firmware half is
 * pinned by the golden vectors instead.
 *
 * The mock is started on a port of its own, so a mock somebody left running
 * does not turn a failure here into a mystery.
 */
import test, { after, before } from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';

const PORT = 8123;
const BASE = `http://127.0.0.1:${PORT}`;
let mock;

async function get(path) {
  const res = await fetch(BASE + path);
  assert.equal(res.status, 200, `${path} answered ${res.status}`);
  return res.json();
}

async function post(path, body, expect = 200) {
  const res = await fetch(BASE + path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  });
  assert.equal(res.status, expect, `${path} answered ${res.status}`);
  return res.json();
}

before(async () => {
  mock = spawn(process.execPath, ['server.js'], {
    env: { ...process.env, QLOCK_MOCK_PORT: String(PORT) },
    stdio: 'ignore'
  });
  for (let attempt = 0; attempt < 60; attempt++) {
    try {
      await fetch(`${BASE}/currentState`);
      // The writes below are behind expert mode, exactly as they are on the
      // clock, so the tests have to unlock it the way a person does. A fresh
      // clock has no password and keeps the first one offered.
      await post('/expert', { password: 'a-test-password' });
      return;
    } catch {
      await new Promise((resolve) => setTimeout(resolve, 100));
    }
  }
  throw new Error('the mock did not come up');
});

test('the writes really are behind the lock', async () => {
  // Not incidental to the tests above: the read is open because looking at a
  // curve is a diagnosis, and the writes are not because editing one is a
  // different act. Locking it again proves the guard is the reason the rest of
  // this file had to unlock.
  await post('/expert', { off: true });
  await post('/luminance', { factoryRestore: true }, 403);
  await get('/luminance');            // still readable
  await get('/luminance/surface');    // and so is the diagram
  await post('/expert', { password: 'a-test-password' });
});

after(() => mock?.kill());

test('/luminance names the profile and says whether it may be acted on', async () => {
  const { factory } = await get('/luminance');
  assert.equal(factory.valid, true);
  assert.equal(factory.profileId, 'current-diffuser-before-mask-6levels-6hues');
  assert.equal(factory.stackId, 'current-diffuser-before-mask');
  assert.match(factory.checksum, /^[0-9a-f]{64}$/);
});

test('and carries the axes the diagram needs, but not the measurement', async () => {
  const { factory } = await get('/luminance');
  assert.deepEqual(factory.hueKnots, [0, 60, 120, 180, 240, 300]);
  assert.equal(factory.huePeriod, 360);
  assert.equal(factory.luxKnots.length, factory.levels);
  // The grid itself belongs to the surface endpoint: this response is polled
  // once a second and the measurement never changes.
  assert.equal(factory.levelsData, undefined);
  assert.equal(factory.residuals, undefined);
});

test('the two monotonicity answers are reported apart', async () => {
  const { factory } = await get('/luminance');
  // The reviewed profile's observations contradict themselves at one hue and
  // the grid that was shipped rises everywhere all the same. Reading one for
  // the other makes the clock announce a fault it does not have.
  assert.equal(factory.observationsMonotone, false);
  assert.equal(factory.gridMonotone, true);
});

test('the profile does not hide what it is worth', async () => {
  const { factory } = await get('/luminance');
  assert.equal(factory.acceptanceMet, false);
  assert.equal(factory.maxError, 15);
  assert.equal(factory.worstHue, 240);
});

test('the target says which layers produced it', async () => {
  const { target } = await get('/luminance');
  assert.ok(['legacy', 'factory', 'factory+user'].includes(target.source));
  assert.equal(typeof target.percent, 'number');
  assert.equal(typeof target.factory, 'number');
  assert.equal(typeof target.bias, 'number');
  for (const flag of ['limited', 'bound', 'clamped']) {
    assert.equal(typeof target[flag], 'boolean', flag);
  }
});

test('corrections at the same light in different colours both survive', async () => {
  const { user } = await get('/luminance');
  const near = user.residuals.filter((one) => one.lux > 0.4 && one.lux < 0.5);
  assert.equal(near.length, 2, 'both are kept');
  assert.notEqual(near[0].hue, near[1].hue);
  // The colour is part of the key rather than decoration beside it. This is
  // exactly what the old white-only ring could not express.
  assert.ok(near.every((one) => Number.isFinite(one.decades)));
});

test('the surface is a grid with both axes and both flags', async () => {
  const surface = await get('/luminance/surface');
  assert.equal(surface.valid, true);
  assert.equal(surface.sat, 100);
  assert.equal(surface.hue.length, 360 / 15);
  assert.equal(surface.percent.length, surface.lux.length);
  for (const row of surface.percent) assert.equal(row.length, surface.hue.length);
  assert.equal(surface.limited.length, surface.lux.length);
  assert.equal(surface.bound.length, surface.lux.length);
  // Hue starts at 0 and stops short of the period: the last column joins back
  // to the first, and repeating 0 at the end would draw the seam twice.
  assert.equal(surface.hue[0], 0);
  assert.ok(surface.hue[surface.hue.length - 1] < 360);
});

test('every percentage on the surface is inside the regulated range', async () => {
  const surface = await get('/luminance/surface');
  for (const row of surface.percent) {
    for (const value of row) {
      assert.ok(value >= surface.minPercent && value <= surface.maxPercent,
                `${value} outside ${surface.minPercent}..${surface.maxPercent}`);
    }
  }
});

test('one correction can be forgotten by its place', async () => {
  const before = (await get('/luminance')).user.residuals;
  const after = await post('/luminance', { forgetResidual: 0 });
  assert.equal(after.user.residuals.length, before.length - 1);
  assert.equal(after.user.residuals[0].hue, before[1].hue);
});

test('and a place that is not there is refused rather than guessed at', async () => {
  await post('/luminance', { forgetResidual: 99 }, 404);
});

test('the factory restore clears the corrections and keeps the coupling', async () => {
  const before = await get('/light');
  const after = await post('/luminance', { factoryRestore: true });
  assert.equal(after.user.residuals.length, 0);
  assert.equal(after.points.length, 0);
  assert.equal(after.factory.matched, true);
  // The coupling is a measurement of this clock's own optics, not of anybody's
  // taste, and twenty minutes to redo. It must survive.
  const light = await get('/light');
  assert.equal(light.coupled, before.coupled);
});
