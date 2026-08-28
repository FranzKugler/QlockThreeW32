/**
 * The settlement seam: what happens `LUM_SETTLE_MS` after the last slider
 * move, once it is decided a nudge is real and not a hand still on the
 * slider. See CLAUDE.md, "There is no remember this button" and "Colour
 * changes the level".
 *
 * Split out of server.js so it can be driven from explicit arguments rather
 * than from `currentLux()`, `Date.now()` and the module-level `curve`/
 * `residuals` it used to close over - a settlement is a pure decision given
 * what was asked and what the factory says about it, and testing that
 * decision should not need a running mock or a fake clock.
 *
 * Two regimes, and only one runs per call:
 *
 * - No factory profile: the clock (or the mock) has nothing better than the
 *   white-only curve, so the nudge is folded into it exactly as before -
 *   `remember()` is the legacy fit in server.js, untouched here.
 * - A factory profile: the white curve is left alone - a colour-aware
 *   correction has no business perturbing a fit that knows nothing about hue
 *   or saturation - and the nudge is turned into a *residual*: how far the
 *   requested output sits, in decades, from what the factory profile already
 *   predicts for that light and that colour.
 */

// LUM_SAME_LIGHT_RATIO in src/Luminance.h / server.js: a new point replaces a
// near neighbour rather than joining the ring, so repeated corrections in one
// room and one sitting do not crowd out everything else that was learned.
const NEAR_LIGHT_RATIO = 1.3;

// Hue is not a colour anyone chose once saturation is zero - it is whatever
// the wheel happened to be sitting at - so every white nudge is filed under
// one identity rather than under whichever hue was last dragged past.
const CANONICAL_HUE_AT_ZERO_SAT = 0;

// The inverse of the mock's/firmware's percent-from-decades map
// (`percent = round(100 * 10 ** (decades / 2.2))`), so a requested percentage
// can be compared against a factory target that is itself expressed in
// decades.
const decadesFromPercent = (percent) => 2.2 * Math.log10(percent / 100);

const sameIdentity = (a, hue, sat) => a.hue === hue && a.sat === sat;

const sameLight = (a, lux) => Math.max(lux / a.lux, a.lux / lux) <= NEAR_LIGHT_RATIO;

/**
 * Settle one nudge.
 *
 * @param {object} opts
 * @param {boolean} opts.factoryValid - whether a factory profile is installed
 * @param {number} opts.lux - the light the nudge was made in
 * @param {number} opts.percent - the brightness percentage that was asked for
 * @param {number} opts.hue - the hue the face was showing
 * @param {number} opts.sat - the saturation the face was showing
 * @param {number} opts.maxPercent - the regulated ceiling; asking for it or
 *   more means "at least this much", the same censoring the white curve gives
 *   a point at the top of its range
 * @param {Array<{lux:number, decades:number, hue:number, sat:number, bound:boolean, seconds:number}>} opts.residuals
 *   the colour-aware corrections, mutated in place when the factory is valid
 *   and left untouched otherwise
 * @param {(lux:number, hue:number, sat:number) => number} opts.factoryTarget -
 *   the factory profile's own baseline for this light and colour, in decades,
 *   before any user correction
 * @param {(lux:number, percent:number, hue:number, sat:number) => void} opts.remember -
 *   the legacy white-only fit; called only when there is no factory profile,
 *   and is the only thing here allowed to touch the legacy curve
 * @param {number} opts.seconds - when this nudge is being said, in whatever
 *   clock the caller uses (the mock's `process.uptime()`, the firmware's
 *   uptime). Explicit rather than read in here, so a settlement stays a pure
 *   function of its arguments and a test never has to fake a clock to pin it.
 * @param {number} opts.capacity - how many residuals the list may hold at
 *   once, mirroring `RESIDUAL_MAX`/`ResidualStore` on the clock
 *   (`LUM_USER_POINTS` in server.js). Enforced only on an append: a
 *   replacement updates a slot that already exists and cannot itself grow
 *   the list past it.
 * @returns {{storedIn: 'legacy'} | {storedIn: 'residual', point: object}}
 */
export function settleNudge({
  factoryValid, lux, percent, hue, sat, maxPercent,
  residuals, factoryTarget, remember, seconds, capacity
}) {
  if (!factoryValid) {
    remember(lux, percent, hue, sat);
    return { storedIn: 'legacy' };
  }

  const canonicalHue = sat === 0 ? CANONICAL_HUE_AT_ZERO_SAT : hue;
  const baseline = factoryTarget(lux, canonicalHue, sat);
  const decades = decadesFromPercent(percent) - baseline;
  const bound = percent >= maxPercent;
  const point = { lux, decades, hue: canonicalHue, sat, bound, seconds };

  // A replacement is only ever within the *same* colour identity: two
  // colours a degree apart are still two different statements about the
  // room, and merging them would be exactly the collapse the near-light rule
  // exists to prevent for light, done again for colour.
  const near = residuals.find((r) => sameIdentity(r, canonicalHue, sat) && sameLight(r, lux));

  if (near) {
    // Refreshed and moved to the end: oldest-first order (see ResidualStore)
    // is what capacity eviction relies on, so a refreshed point has to become
    // the newest again rather than staying in its old slot. Handed back by
    // reference: a caller reading `.point` afterwards must see the live
    // record that is actually sitting in `residuals`, not a copy that stops
    // matching the moment something else touches the store.
    residuals.splice(residuals.indexOf(near), 1);
    Object.assign(near, point);
    residuals.push(near);
    return { storedIn: 'residual', point: near };
  }

  residuals.push(point);
  // Oldest first, newest last (see ResidualStore.h) - an eleventh point
  // pushes the first one out rather than growing the list without bound.
  if (residuals.length > capacity) residuals.shift();

  return { storedIn: 'residual', point };
}
