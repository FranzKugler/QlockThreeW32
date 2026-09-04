/**
 * server
 * Mock of the clock's REST API, so the web UI can be developed without
 * hardware. Mirrors the endpoints in `src/main .cpp`; the initial values are
 * the defaults from the Settings constructor in `src/Settings.cpp`.
 *
 *   npm run mock            # this server on :8080
 *   npm run mock -- portal  # ...in setup-portal mode instead, see Portal.h
 *   npm run dev             # Vite dev server, proxies the API routes here
 *
 * It also serves data/ statically, so a production build can be checked
 * against the mock by opening http://localhost:8080 directly.
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 */
import express from 'express';
import fs from 'node:fs';
import path from 'node:path';
import { settleNudge } from './lib/settlement.js';

const app = express();

app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// Moved ahead of every route: a request for a real file must be served
// exactly as it would be from data/ regardless of what runs after this,
// portal-mode gating included.
app.use(express.static('data'));

// `npm run mock -- portal` simulates the setup portal instead of normal
// operation - see src/Portal.h. The two are mutually exclusive on the clock
// itself: an open access point is not somewhere to expose the rest of the
// API, so main .cpp registers either the portal's routes or the others and
// never both. Placed after the static middleware above (so the built
// portal.html and its bundle still load) and before every other route
// (so nothing else answers) - a mistake here now fails in `npm run dev` too,
// rather than only on real hardware.
const PORTAL_MODE = process.argv.includes('portal');
app.use((req, res, next) => {
  // Reachable exactly when the mode matches: the portal's own routes only in
  // portal mode, everything else only outside it - the same "never both" the
  // firmware gets from registering one set of handlers or the other.
  if (PORTAL_MODE === req.path.startsWith('/portal/')) return next();
  res.status(404).json({ error: 'fsNotFound' });
});

/*
 * Expert mode, the same contract the firmware implements.
 *
 * Registered here rather than beside the other routes on purpose: the guard is
 * middleware, and Express only runs middleware that was added before the route
 * it should cover. Put at the end it would let every /ota and /log request
 * straight through, and the dev UI would behave nothing like the device.
 *
 * The mock always has its reset window open - it "powers on" when node starts -
 * so the way back is reachable without restarting anything.
 */
const EXPERT_GRACE_MS = 5 * 60 * 1000;
const EXPERT_MIN_PASSWORD = 6;
const EXPERT_MAX_FAILURES = 5;

const expertBootAt = Date.now();
const expert = { password: null, on: false, failures: 0, lockoutUntil: 0 };

const expertGrace = () =>
  Math.max(0, Math.round((EXPERT_GRACE_MS - (Date.now() - expertBootAt)) / 1000));

const expertLockedOut = () => Date.now() < expert.lockoutUntil;

const expertState = () => ({
  enrolled: expert.password !== null,
  unlocked: expert.on,
  grace: expertGrace(),
  lockedOut: expertLockedOut()
});

// Everything behind the lock, in one list. Registered before the routes it
// covers - added at the end it would let every request through and the dev UI
// would behave nothing like the device.
for (const prefix of ['/log', '/ota', '/fs', '/nvs']) {
  app.use(prefix, (req, res, next) => {
    if (expert.on) return next();
    res.status(403).json({ error: 'expertLocked' });
  });
}

/**
 * The lock, for a route where it depends on the method.
 *
 * GET /luminance is open and POST /luminance is not, and the same split runs
 * through POST /light, whose coupling branches are guarded while the {reset}
 * beside them is not. Looking at a brightness curve is what somebody does when
 * the automatic feels wrong; changing it is a different act.
 */
const guarded = (res) => {
  if (expert.on) return false;
  res.status(403).json({ error: 'expertLocked' });
  return true;
};

app.get('/expert', (req, res) => res.json(expertState()));

app.post('/expert', (req, res) => {
  const body = req.body || {};

  if (body.off) {
    expert.on = false;
    return res.json(expertState());
  }

  if (body.reset) {
    if (expertGrace() === 0) return res.status(403).json({ error: 'expertNoGrace' });
    expert.password = null;
    expert.on = false;
    expert.failures = 0;
    expert.lockoutUntil = 0;
    return res.json(expertState());
  }

  if (expertLockedOut()) return res.status(429).json({ error: 'expertLockedOut' });

  const password = String(body.password ?? '');

  if (expert.password === null) {
    if (password.length < EXPERT_MIN_PASSWORD) {
      return res.status(403).json({ error: 'expertPasswordShort' });
    }
    expert.password = password;
    expert.on = true;
    console.log(`/expert password set (${password.length} characters)`);
    return res.json(expertState());
  }

  if (password !== expert.password) {
    if (++expert.failures >= EXPERT_MAX_FAILURES) {
      expert.lockoutUntil = Date.now() + EXPERT_GRACE_MS;
    }
    return res.status(403).json({ error: 'expertWrongPassword' });
  }

  expert.failures = 0;
  expert.lockoutUntil = 0;
  expert.on = true;
  res.json(expertState());
});

const state = {
  display: 1,
  hue: 0,
  sat: 0,
  lum: 50,
  automaticLum: false,
  language: 2,
  cornerColor: 0,
  cornerDirection: 1,

  // Name the clock answers to; also reported by /wifi, where it is edited.
  hostname: 'QlockThreeW32',

  ntpServer: 'pool.ntp.org',
  useDs: true,
  // Which entry of zones.json the rules below were filled from; empty once
  // they are edited by hand. The clock stores this as a label only.
  tzZone: 'Europe/Berlin',
  tzName: 'CET',
  tzWeek: 0,
  tzDoW: 1,
  tzMonth: 10,
  tzHour: 3,
  tzOffset: 60,
  tzDsName: 'CEST',
  tzDsWeek: 0,
  tzDsDoW: 1,
  tzDsMonth: 3,
  tzDsHour: 2,
  tzDsOffset: 120
};

/** Copies the given keys from the request body into the mock state. */
function accept(keys) {
  return (req, res) => {
    console.log(req.path, req.body);
    for (const key of keys) {
      if (req.body[key] !== undefined) state[key] = req.body[key];
    }
    res.json({ msg: '' });
  };
}

app.get('/currentState', (req, res) => {
  console.log('/currentState', state);
  res.json(state);
});

app.post('/display', accept(['display']));
// The firmware reads this as an int but reports it back as a bool, so coerce
// here too rather than echoing the 1/0 the UI sends.
app.post('/autoluminance', (req, res) => {
  console.log('/autoluminance', req.body);
  if (req.body.automaticLum !== undefined) {
    state.automaticLum = Boolean(req.body.automaticLum);
  }
  res.json({ msg: '' });
});
// Note: like the firmware, /color does not touch automaticLum.
/*
 * Colour and brightness.
 *
 * The brightness slider means two different things. With the automatic off it
 * is the brightness, stored. With it on it is "at this light, I want this
 * much": the automatic steps aside and the pair is kept LUM_SETTLE_MS after
 * the last move - the timer is on this side in the firmware too, because a tab
 * closed mid-adjustment must not lose the point. The stored manual brightness
 * is left alone, so switching the automatic off gives it back.
 */
app.post('/color', (req, res) => {
  console.log('/color', req.body);
  if (req.body.hue !== undefined) state.hue = req.body.hue;
  if (req.body.sat !== undefined) state.sat = req.body.sat;

  if (req.body.lum !== undefined) {
    if (state.automaticLum) {
      // The colour travels with the brightness: what is being described is the
      // face the person was looking at, and ten seconds is long enough to have
      // changed it. Nothing reads it yet - see Luminance::Point.
      nudge = {
        percent: Math.min(curve.maxPercent, Math.max(curve.minPercent, req.body.lum)),
        hue: state.hue,
        sat: state.sat,
        at: Date.now() + LUM_SETTLE_MS
      };
    } else {
      state.lum = req.body.lum;
    }
  }
  res.json({ msg: '' });
});
/*
 * Language and the corner options.
 *
 * Once the clock is set up - meaning an expert password has been chosen - the
 * language may only move within the panel it already has, because the letters
 * are milled once and no setting changes them. A clock with no password is
 * still being set up and keeps the full list.
 *
 * The mock enforces it too, or the dev UI would let through what the device
 * refuses, which is the one thing a mock must not do. LANGUAGES is defined
 * further down; the handler only runs long after that.
 */
app.post('/configuration', (req, res, next) => {
  const wanted = req.body.language;
  if (expert.password !== null && !expert.on && wanted !== undefined && wanted !== state.language) {
    const have = LANGUAGES.find((l) => l.value === state.language);
    const to = LANGUAGES.find((l) => l.value === wanted);
    if (have && (!to || to.panel !== have.panel)) {
      return res.status(403).json({ error: 'languageNotOnPanel' });
    }
  }
  return accept(['language', 'cornerColor', 'cornerDirection'])(req, res, next);
});
app.post(
  '/timezone',
  accept([
    'ntpServer',
    'useDs',
    'tzZone',
    'tzName',
    'tzWeek',
    'tzDoW',
    'tzMonth',
    'tzHour',
    'tzOffset',
    'tzDsName',
    'tzDsWeek',
    'tzDsDoW',
    'tzDsMonth',
    'tzDsHour',
    'tzDsOffset'
  ])
);

// ---------------------------------------------------------------- WiFi ----
// Simulates the firmware's behaviour: an async scan, and a network switch that
// falls back to the previous network when the password is wrong. Use the
// password "wrong" to exercise the failure path.

const wifi = {
  connected: true,
  ssid: 'Heimnetz',
  ip: '192.168.1.42',
  rssi: -54,
  mac: '84:F7:03:AA:BB:CC',
  switching: false,
  error: '',
  errorDetail: ''
};

// Two entries share an SSID on purpose: a dual-band router answers a scan
// once per radio, and that used to crash the network list.
const FAKE_NETWORKS = [
  { ssid: 'Heimnetz', rssi: -54, secure: true },
  { ssid: 'Heimnetz', rssi: -66, secure: true },
  { ssid: '', rssi: -70, secure: true },
  { ssid: 'Heimnetz-Gast', rssi: -61, secure: true },
  { ssid: 'FRITZ!Box 7590', rssi: -72, secure: true },
  { ssid: 'Nachbar-WLAN', rssi: -83, secure: true },
  { ssid: 'Freifunk', rssi: -88, secure: false }
];

let scanFinishedAt = 0;

/**
 * The web app manifest, built rather than served as a file for the same reason
 * the firmware builds it: it carries the clock's name, which is per clock.
 */
app.get('/manifest.webmanifest', (req, res) => {
  res.type('application/manifest+json').json({
    name: state.hostname,
    short_name: state.hostname,
    start_url: '/',
    display: 'standalone',
    background_color: '#0d0d0d',
    theme_color: '#0d0d0d',
    icons: [
      { src: '/icon-192.png', sizes: '192x192', type: 'image/png' },
      { src: '/icon-512.png', sizes: '512x512', type: 'image/png' },
      { src: '/icon-512-maskable.png', sizes: '512x512', type: 'image/png', purpose: 'maskable' }
    ]
  });
});

/**
 * What the light sensor sees. `present: false` is the normal case on a clock
 * with no sensor fitted, and the colour tab then hides the automatic section
 * entirely - set it to false here to develop against that.
 */
/*
 * The automatic brightness, as far as the mock needs it.
 *
 * The firmware keeps up to ten (lux, percent) pairs in NVS and fits a line
 * through them in log light; see src/Luminance.h, which is where the rules
 * are. This repeats the shape of it so the colour tab and the #luminance
 * screen can be worked on without a clock - the same least squares, the same
 * refusal to fit a slope through points that sit on top of each other, and the
 * same 20..100 clamp.
 *
 * It is not a second implementation to keep in step with the firmware: nothing
 * here has to agree with the clock to the last percent, and if the two ever
 * disagree the clock is right.
 */
const LUM_POINTS = 10;
const LUM_MIN_PERCENT = 20;
const LUM_MAX_PERCENT = 100;
const LUM_SETTLE_MS = 10000;
const LUM_SAME_LIGHT_RATIO = 1.3;
const LUM_FIT_MIN_DECADES = 0.6;

const DEFAULT_LINE = { lowLux: 0.3, lowPercent: 20, highLux: 9.0, highPercent: 100 };

const logLux = (lux) => Math.log10(Math.max(lux, 0.01));

// Both ends of the regulated range live with the curve, as they do on the
// clock: how dim a face is still readable depends on the panel in front of
// it, so 20 % was one clock's answer rather than a constant.
const curve = { slope: 0, offset: 0, fitted: false, points: [],
                minPercent: LUM_MIN_PERCENT, maxPercent: LUM_MAX_PERCENT };

function defaultLine() {
  const lo = logLux(DEFAULT_LINE.lowLux);
  const hi = logLux(DEFAULT_LINE.highLux);
  curve.slope = (DEFAULT_LINE.highPercent - DEFAULT_LINE.lowPercent) / (hi - lo);
  curve.offset = DEFAULT_LINE.lowPercent - curve.slope * lo;
  curve.fitted = false;
}

function fit() {
  if (curve.points.length === 0) return defaultLine();

  // A point at the top of the range is censored: the slider had nothing more
  // to offer, so "100 %" means "at least 100 %" and least squares reading it
  // as an equality drags the bright end down. Left out - unless leaving them
  // out leaves fewer than two, where a poor line beats no line.
  curve.points.forEach((p) => {
    p.used = p.percent < curve.maxPercent;
  });
  let inFit = curve.points.filter((p) => p.used);
  if (inFit.length < 2) {
    curve.points.forEach((p) => {
      p.used = true;
    });
    inFit = curve.points;
  }

  const xs = inFit.map((p) => logLux(p.lux));
  const ys = inFit.map((p) => p.percent);
  const meanX = xs.reduce((a, b) => a + b, 0) / xs.length;
  const meanY = ys.reduce((a, b) => a + b, 0) / ys.length;

  let canFitSlope = Math.max(...xs) - Math.min(...xs) >= LUM_FIT_MIN_DECADES;
  if (canFitSlope) {
    let top = 0;
    let bottom = 0;
    xs.forEach((x, i) => {
      top += (x - meanX) * (ys[i] - meanY);
      bottom += (x - meanX) ** 2;
    });
    const candidate = bottom > 0 ? top / bottom : 0;
    // Darker room, brighter clock is never wanted, so a slope of zero or less
    // is refused and the old one stands.
    if (candidate > 0) curve.slope = candidate;
    else canFitSlope = false;
  }

  curve.offset = meanY - curve.slope * meanX;
  curve.fitted = canFitSlope;
}

function brightnessForLux(lux) {
  const raw = curve.slope * logLux(lux) + curve.offset;
  return Math.min(curve.maxPercent, Math.max(curve.minPercent, Math.round(raw)));
}

/*
 * A new point replaces a near neighbour rather than joining the queue, so ten
 * corrections made in one evening cannot push the daylight point out and
 * collapse the line onto a single lighting condition.
 */
function remember(lux, percent, hue, sat) {
  const near = curve.points.find(
    (p) => Math.max(lux / p.lux, p.lux / lux) <= LUM_SAME_LIGHT_RATIO
  );
  const point = { lux, percent, hue, sat, seconds: Math.round(process.uptime()) };
  if (near) Object.assign(near, point);
  else {
    curve.points.push(point);
    if (curve.points.length > LUM_POINTS) curve.points.shift();
  }
  fit();
}

defaultLine();

// Roughly a lit living room seen through a dark front panel, drifting slowly
// so the tab has something to react to.
const currentLux = () => 7.4 + Math.sin(Date.now() / 20000) * 2;

// The nudge being waited out, if any. The timer lives on this side in the
// firmware too - a browser tab closed mid-adjustment must not lose the point.
let nudge = null;

/* ==========================================================================
 * The colour-aware factory model
 *
 * The clock reads it out of `/factory-luminance.json` in its filesystem image
 * and evaluates it in C++; this reads the very same file - `web/public/` is
 * what Vite copies into that image, so there is exactly one copy of the
 * numbers and no chance of a mock quietly describing a different model.
 *
 * What is *not* mirrored is the evaluator. Inverting the gamma, the
 * per-channel scaling and the measured drive table over the integers of the
 * regulated range is the one thing the golden vectors exist to pin down, and a
 * second implementation of it in JavaScript would be a second thing to keep in
 * step - which is precisely the mistake this project has already made once,
 * when the mock and the firmware disagreed about the intercept for weeks. So
 * the surface here is a plausible shape with the real axes on it: enough to
 * develop the diagram against, and honest about being a stand-in.
 * ======================================================================== */

// Resolved from this file rather than from the working directory: the mock is
// started from the repo root today and a relative path would break the day it
// is not.
const FACTORY_FILE = new URL('./web/public/factory-luminance.json', import.meta.url);

function loadFactory() {
  try {
    const document = JSON.parse(fs.readFileSync(FACTORY_FILE, 'utf8'));
    const payload = document.payload;
    return {
      valid: true,
      error: '',
      profileId: payload.profileId,
      stackId: payload.stackId,
      checksum: payload.sourceChecksum,
      huePeriod: payload.huePeriod,
      cone: payload.cone,
      nose: payload.nose,
      blue: payload.blue,
      percentRange: payload.percentRange,
      status: payload.status,
      // Table 1 - the measurements the shipped nose and blue's line were
      // fitted from - carried along so this mock can refit them together
      // with a taught correction the same way ResidualStore::refit() does,
      // rather than leaving "what did teaching it change" unanswerable in
      // development.
      points: payload.points ?? []
    };
  } catch (err) {
    // A clock whose image predates the model behaves exactly as it did before
    // it existed, and that branch has to be visible in development too.
    return { valid: false, error: 'factoryMissing' };
  }
}

const factory = loadFactory();

// Which profile the corrections below were learned against. On the clock this
// is a string in NVS; here it is the same string in memory, and the factory
// restore is what sets it.
let recordedProfile = factory.valid ? factory.checksum : '';

// Which profile the corrections below were learned on. Separate from the line
// above on purpose: `recordedProfile` is "which profile is this clock on",
// written once, and this is "were these corrections said about it". On a clock
// that has never corrected anything nothing disagrees, which is not the same
// as "no" - read the other way round, a brand new clock shows a warning about
// a mismatch it does not have.
let residualsProfile = factory.valid ? factory.checksum : '';

/*
 * The corrections, in decades of emitted light rather than in percent.
 *
 * Two of them at the same light in different colours, because that is the case
 * the old white-only ring could not express and the one the screen has to be
 * able to draw: neither replaces the other.
 */
let residuals = factory.valid
  ? [
      { lux: 0.42, decades: 0.061, hue: 120, sat: 90, seconds: 4210 },
      { lux: 0.44, decades: -0.088, hue: 240, sat: 100, seconds: 6890 },
      { lux: 6.1, decades: 0.024, hue: 120, sat: 90, seconds: 9120 }
    ]
  : [];

// LUM_SURFACE_HUE_STEP in src/Luminance.h. The knots are 60 degrees apart and
// what is interesting is between them - the seam at 300 back to 0 above all.
const LUM_SURFACE_HUE_STEP = 15;

// LUM_SURFACE_LUX_ROWS in src/Luminance.h. The cone has no ambient rows of
// its own - it is a straight line - so this many are sampled evenly in log
// light between what the fit was built from.
const LUM_SURFACE_LUX_ROWS = 9;

const LUM_USER_POINTS = 8;

// RESIDUAL_SHADOW_HUE / RESIDUAL_SHADOW_LUX in src/ResidualStore.h - the
// sphere a taught point shadows a factory one within, and the same "same
// light" ratio a near-neighbour replaces under. log10(1.3) both times.
const RESIDUAL_SHADOW_HUE = 30;
const RESIDUAL_SHADOW_LUX = Math.log10(1.3);

const hueDistance = (a, b, period) => {
  const gap = Math.abs(a - b) % period;
  return gap > period / 2 ? period - gap : gap;
};

/**
 * The nose's three coefficients by ordinary least squares, mirroring
 * ResidualStore.cpp's solveNose() - a0 + a1*cos + b1*sin against `targets`.
 * `null` below three points or on a singular system, the same refusal.
 */
function solveNose(rows, targets) {
  const n = rows.length;
  if (n < 3) return null;
  const a = [[0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]];
  for (let p = 0; p < n; p++) {
    for (let i = 0; i < 3; i++) {
      a[i][3] += rows[p][i] * targets[p];
      for (let j = 0; j < 3; j++) a[i][j] += rows[p][i] * rows[p][j];
    }
  }
  for (let col = 0; col < 3; col++) {
    let pivot = col;
    for (let r = col + 1; r < 3; r++) {
      if (Math.abs(a[r][col]) > Math.abs(a[pivot][col])) pivot = r;
    }
    if (Math.abs(a[pivot][col]) < 1e-12) return null;
    if (pivot !== col) { const t = a[col]; a[col] = a[pivot]; a[pivot] = t; }
    const scale = a[col][col];
    for (let j = col; j < 4; j++) a[col][j] /= scale;
    for (let r = 0; r < 3; r++) {
      if (r === col) continue;
      const factor = a[r][col];
      for (let j = col; j < 4; j++) a[r][j] -= factor * a[col][j];
    }
  }
  return [a[0][3], a[1][3], a[2][3]];
}

/** A straight line by ordinary least squares - solveLine() in ResidualStore.cpp. */
function solveLine(xs, ys) {
  const n = xs.length;
  if (n < 2) return null;
  const meanX = xs.reduce((s, x) => s + x, 0) / n;
  const meanY = ys.reduce((s, y) => s + y, 0) / n;
  let covariance = 0, variance = 0;
  for (let i = 0; i < n; i++) {
    const dx = xs[i] - meanX;
    covariance += dx * (ys[i] - meanY);
    variance += dx * dx;
  }
  if (variance < 1e-12) return null;
  const slope = covariance / variance;
  return [slope, meanY - slope * meanX];
}

/**
 * Table 1 and Table 2, refit together - the stand-in for ResidualStore::refit().
 * A taught point (not white, not a bound) shadows a factory point inside its
 * sphere; whatever survives, split by distance from blue's own hue, is fitted
 * again by ordinary least squares. Seeded from the factory's own numbers, the
 * same fallback the firmware uses when a half cannot be refit.
 */
function refit() {
  const { nose, blue, huePeriod, points } = factory;
  const fit = { noseA0: nose.a0, noseA1: nose.a1, noseB1: nose.b1,
                blueSlope: blue.slope, blueOffset: blue.offset };

  const shadowed = points.map((p) =>
    residuals.some((r) => {
      if (r.sat === 0) return false;
      const dh = hueDistance(p.hue, r.hue, huePeriod) / RESIDUAL_SHADOW_HUE;
      const dx = (p.logLux - logLux(r.lux)) / RESIDUAL_SHADOW_LUX;
      return dh * dh + dx * dx < 1;
    }));

  const noseRows = [], noseTargets = [], blueXs = [], blueYs = [];
  const inBlue = (hue) => hueDistance(hue, blue.hue, huePeriod) < blue.blendHalfWidth;
  points.forEach((p, i) => {
    if (shadowed[i]) return;
    if (inBlue(p.hue)) { blueXs.push(p.logLux); blueYs.push(p.residual); return; }
    const rad = (p.hue * Math.PI) / 180;
    noseRows.push([1, Math.cos(rad), Math.sin(rad)]);
    noseTargets.push(p.residual);
  });
  for (const r of residuals) {
    if (r.sat === 0 || r.bound) continue;
    const x = logLux(r.lux);
    if (inBlue(r.hue)) { blueXs.push(x); blueYs.push(r.decades); continue; }
    const rad = (r.hue * Math.PI) / 180;
    noseRows.push([1, Math.cos(rad), Math.sin(rad)]);
    noseTargets.push(r.decades);
  }

  const noseSolved = solveNose(noseRows, noseTargets);
  if (noseSolved) [fit.noseA0, fit.noseA1, fit.noseB1] = noseSolved;
  const blueSolved = solveLine(blueXs, blueYs);
  if (blueSolved) [fit.blueSlope, fit.blueOffset] = blueSolved;
  return fit;
}

/** The raised-cosine weight blue's own line carries at this hue - see
 *  FactoryProfile::blueWeight(). 1 at blue's own hue, 0 at and beyond
 *  blendHalfWidth degrees either side of it. */
function blueWeight(hue) {
  const { blue, huePeriod } = factory;
  const gap = Math.abs(hue - blue.hue) % huePeriod;
  const distance = gap > huePeriod / 2 ? huePeriod - gap : gap;
  if (distance >= blue.blendHalfWidth) return 0;
  return 0.5 * (1 + Math.cos((Math.PI * distance) / blue.blendHalfWidth));
}

/**
 * The stand-in surface: the real cone and hue nose, turned into a percentage
 * by a monotone map rather than by the firmware's inversion. See the note
 * above for why it is not the real one.
 *
 * `fit` overrides the nose and blue's line - the factory's own by default, or
 * whatever refit() last made of Table 1 and Table 2 together, so this one
 * function draws both the uncorrected surface and the taught one instead of
 * a bias bolted on afterwards.
 */
function factoryPercent(lux, hue, sat, fit = factory.nose && factory.blue
  ? { noseA0: factory.nose.a0, noseA1: factory.nose.a1, noseB1: factory.nose.b1,
      blueSlope: factory.blue.slope, blueOffset: factory.blue.offset }
  : null) {
  if (!factory.valid) return null;
  const { cone } = factory;
  const x = logLux(lux);
  const clamped = x < cone.logLuxMin || x > cone.logLuxMax;

  const white = cone.slope * x + cone.offset;
  const rad = (hue * Math.PI) / 180;
  const noseValue = fit.noseA0 + fit.noseA1 * Math.cos(rad) + fit.noseB1 * Math.sin(rad);
  const blueLine = fit.blueSlope * x + fit.blueOffset;
  const weight = blueWeight(hue);
  const residual = (noseValue + weight * (blueLine - noseValue))
    * Math.min(1, Math.max(0, sat / 100));

  // The map from decades to percent. Not the firmware's; monotone, so the
  // shape of the surface is right even though the numbers are a stand-in.
  const target = white + residual;
  const percent = Math.round(100 * 10 ** (target / 2.2));
  const { min, max } = factory.percentRange;
  return {
    percent: Math.min(max, Math.max(min, percent)),
    target,
    limited: percent > max,
    clamped
  };
}

/**
 * Where a taught point sits on the same stand-in surface, at saturation 100 -
 * Table 1's own coordinate, and the one factoryPercent() draws the diagram
 * at. Not `factoryPercent(lux, hue, 100)`: that would ask the model what it
 * thinks a hue is worth and add the taught decades on top, counting the
 * model's own opinion twice. This instead puts the decades that were
 * actually taught directly on the cone, which is what "where does this
 * statement land" has to mean.
 */
function percentForTaught(lux, decades) {
  if (!factory.valid) return null;
  const { cone, percentRange } = factory;
  const white = cone.slope * logLux(lux) + cone.offset;
  const percent = Math.round(100 * 10 ** ((white + decades) / 2.2));
  return Math.min(percentRange.max, Math.max(percentRange.min, percent));
}

/*
 * Ten seconds after the last move, decide what the nudge just taught.
 *
 * With no factory profile this is exactly what it always was: fold the point
 * into the white-only curve. With one installed, the white curve has no hue
 * or saturation axis to be corrected on, so the nudge becomes a colour-aware
 * residual instead - see lib/settlement.js, which is what makes that decision
 * testable without a clock or a fake timer.
 */
function settle() {
  if (nudge && Date.now() >= nudge.at) {
    settleNudge({
      factoryValid: factory.valid,
      lux: currentLux(),
      percent: nudge.percent,
      hue: nudge.hue,
      sat: nudge.sat,
      maxPercent: curve.maxPercent,
      residuals,
      factoryTarget: (lux, hue, sat) => factoryPercent(lux, hue, sat).target,
      remember,
      seconds: Math.round(process.uptime()),
      capacity: LUM_USER_POINTS
    });
    nudge = null;
  }
}

/** The body of GET /light, shared with the POST which answers the same shape. */
/*
 * A simulated calibration, so the screen's branches can be worked on without a
 * clock: the phases in order, a progress bar that fills, and a room that is
 * sometimes too bright to measure in.
 *
 * It is not a model of anything. The real run measures 110 cells against a
 * light sensor; there is no sensor here and no display, and a mock of one
 * measuring a mock of the other would tell nobody anything. What can be got
 * wrong on this side is the *sequence* - a bar that never fills, a phase name
 * off by one, an error that vanishes after one poll - and that is what this
 * exercises.
 *
 * Set QLOCK_MOCK_CALIB_BRIGHT=1 to make it refuse, which is the branch that is
 * otherwise only reachable by opening the curtains.
 */
const IDLE_CALIBRATION = {
  running: false, phase: 0, done: 0, total: 0,
  ambient: 0.004, rung: 0, kept: 0, error: '', maxAmbient: 1.0
};
let calibration = { ...IDLE_CALIBRATION };

// Phase, frames, and roughly how long the real one takes over it.
const CALIB_PHASES = [
  [1, 1, 1000],     // ambient
  [2, 31, 9000],    // rung
  [3, 117, 32000],  // cells
  [4, 31, 9000],    // channels
  [5, 14, 4000],    // drive
  [6, 1, 500]       // storing
];

function startCalibration() {
  const tooBright = process.env.QLOCK_MOCK_CALIB_BRIGHT === '1';
  calibration = { ...IDLE_CALIBRATION, running: true, cancelled: false };

  if (tooBright) {
    calibration.ambient = 6.9;
    setTimeout(() => {
      calibration = { ...calibration, running: false, phase: 8, error: 'calibTooBright' };
    }, 1200);
    return;
  }

  let at = 0;
  const enter = () => {
    if (calibration.cancelled) {
      calibration = { ...calibration, running: false, phase: 8, error: 'calibCancelled' };
      return;
    }
    if (at >= CALIB_PHASES.length) {
      calibration = { ...calibration, running: false, phase: 7, kept: 10, rung: 4, error: '' };
      return;
    }
    const [phase, total, ms] = CALIB_PHASES[at++];
    calibration = { ...calibration, phase, total, done: 0 };
    const tick = setInterval(() => {
      if (calibration.cancelled || calibration.done >= total) {
        clearInterval(tick);
        enter();
        return;
      }
      calibration = { ...calibration, done: calibration.done + 1 };
    }, Math.max(20, ms / total));
  };
  enter();
}

function lightState() {
  settle();
  const lux = currentLux();
  return {
    sensor: 'VEML7700',
    present: true,
    available: true,
    lux,
    raw: 7.1 + Math.sin(Date.now() / 3000) * 3,
    // What the clock subtracted as its own face, and how many cells its map
    // describes. Zero here on purpose: there is no display to couple into a
    // sensor that does not exist, and a mock of one would be a made-up number
    // in the one place the real value is the whole point.
    display: 0,
    coupled: 0,
    slope: curve.slope,
    offset: curve.offset,
    fitted: curve.fitted,
    brightness: brightnessForLux(lux),
    // The clock shows the nudge outright while one is being waited out, so
    // the mock has to as well: `applied` is the number somebody compares
    // against the wall, and it must not quietly follow the curve instead.
    applied: nudge ? nudge.percent : brightnessForLux(lux),
    minPercent: curve.minPercent,
    maxPercent: curve.maxPercent,
    taught: curve.points.length,
    adjusting: nudge !== null
  };
}

app.get('/light', (req, res) => res.json(lightState()));

/**
 * The only thing left to write about the curve: throw it away.
 *
 * There is nothing to set any more. It is taught by moving the brightness
 * slider while the automatic is on, which arrives at POST /color.
 */
app.post('/light', (req, res) => {
  // The coupling map is measured on real hardware by scripts/lab.py and has
  // no meaning here, but it must not 400 either - a script pointed at the mock
  // should fail on the measurement, not on the upload.
  if (req.body.coupling) return res.json(lightState());
  if (req.body.couplingReset) {
    calibration = { ...IDLE_CALIBRATION };
    return res.json(lightState());
  }
  if (req.body.calibrate) {
    if (guarded(res)) return;
    if (calibration.running) return res.status(409).json({ error: 'calibBusy' });
    startCalibration();
    return res.json(lightState());
  }
  if (req.body.calibrateAbort) {
    if (guarded(res)) return;
    calibration.cancelled = true;
    return res.json(lightState());
  }
  if (req.body.reset) {
    curve.points = [];
    nudge = null;
    defaultLine();
    return res.json(lightState());
  }
  res.status(400).json({ error: 'calibrationRange' });
});

/** Everything the automatic is thinking, for the brightness screen. */
app.get('/luminance', (req, res) => res.json(luminanceState()));

/**
 * Shared by the GET and by both writes, which answer with the curve as it now
 * stands so the screen redraws from what the clock says.
 *
 * A 307 redirect was the first idea and is a trap: it re-sends the POST to the
 * same path, which is the handler that issued it.
 */
function luminanceState() {
  settle();
  const lux = currentLux();
  const colour = factoryState(lux);
  return {
    lux,
    raw: 7.1 + Math.sin(Date.now() / 3000) * 3,
    // No display and no sensor here, so nothing couples into anything. Zero
    // cells is the honest answer and it is also the branch worth seeing in
    // development: it is what an uncalibrated clock shows.
    display: 0,
    coupled: 0,
    calibration: { ...calibration },
    available: true,
    // The average a point would be taught from, which is not the one the
    // regulator runs on. No sensor here to differ, so they are the same number
    // - the field exists so the screen has it.
    teaching: lux,
    slope: curve.slope,
    offset: curve.offset,
    fitted: curve.fitted,
    // The plain white curve alone - kept for the geek section's comparison,
    // and not what the clock actually shows once a factory profile is
    // loaded. `target.percent` below is that number; see brightnessToApply()
    // in main .cpp, which never looks at this once a profile answers.
    brightness: brightnessForLux(lux),
    // The clock shows the nudge outright while one is being waited out; once
    // it has settled, `applied` follows the colour-aware target exactly as
    // brightnessToApply() does, not the plain white curve - conflating the
    // two is the exact bug this mock exists to catch before the firmware
    // ships it.
    applied: nudge ? nudge.percent : colour.target.percent,
    minPercent: curve.minPercent,
    maxPercent: curve.maxPercent,
    capacity: LUM_POINTS,
    settleMs: LUM_SETTLE_MS,
    uptime: Math.round(process.uptime()),
    adjusting: nudge !== null,
    ...(nudge ? { wanted: nudge.percent } : {}),
    points: curve.points.map((p) => ({
      ...p, used: p.used !== false, curve: brightnessForLux(p.lux)
    })),
    ...colour
  };
}

/**
 * The colour-aware half: which profile is installed, what it is worth, and
 * what it asks for right now in the colour the face is showing.
 *
 * Three separate things because their remedies are separate - see
 * describeFactory() in src/WebRoutes.cpp, which this mirrors field for field.
 */
function factoryState(lux) {
  // What the factory shipped, alone - and what refit() makes of it together
  // with whatever has been taught. Two calls rather than one bias bolted on
  // afterwards, the same change ResidualStore::refit() made to the firmware:
  // a correction moves the model itself, and how much depends on whether it
  // fell close enough to a factory point to replace it.
  const plain = factoryPercent(lux, state.hue, state.sat);
  const haveLearned = residuals.some((r) => r.sat > 0);
  const fit = haveLearned ? refit() : null;
  const asked = haveLearned ? factoryPercent(lux, state.hue, state.sat, fit) : plain;

  const percent = asked
    ? Math.min(curve.maxPercent, Math.max(curve.minPercent, asked.percent))
    : brightnessForLux(lux);

  return {
    factory: {
      valid: factory.valid,
      error: factory.error,
      profileId: factory.profileId ?? '',
      stackId: factory.stackId ?? '',
      checksum: factory.checksum ?? '',
      // The fit itself is monotone by construction (FactoryProfile::valid());
      // this is provenance about the raw observations it was built from.
      observationsMonotone: factory.status?.monotone ?? true,
      acceptanceMet: factory.status?.acceptanceMet ?? false,
      maxError: factory.status?.maxError ?? -1,
      worstHue: factory.status?.worstHue ?? -1,
      recorded: recordedProfile,
      matched: residuals.length === 0 || residualsProfile === factory.checksum,
      huePeriod: factory.huePeriod ?? 360,
      coneSlope: factory.cone?.slope ?? 0,
      coneOffset: factory.cone?.offset ?? 0,
      luxMin: factory.cone ? 10 ** factory.cone.logLuxMin : 0,
      luxMax: factory.cone ? 10 ** factory.cone.logLuxMax : 0,
      blueHue: factory.blue?.hue ?? 240,
      blendHalfWidth: factory.blue?.blendHalfWidth ?? 0,
      // The shipped nose and blue's line themselves - see `user.fit` below
      // for what teaching made of them.
      noseA0: factory.nose?.a0 ?? 0,
      noseA1: factory.nose?.a1 ?? 0,
      noseB1: factory.nose?.b1 ?? 0,
      blueSlope: factory.blue?.slope ?? 0,
      blueOffset: factory.blue?.offset ?? 0
    },
    target: {
      percent,
      factory: plain ? plain.percent : brightnessForLux(lux),
      bias: haveLearned && plain ? asked.target - plain.target : 0,
      hue: state.hue,
      sat: state.sat,
      limited: Boolean(asked?.limited),
      bound: false,
      clamped: Boolean(asked?.clamped),
      source: !factory.valid ? 'legacy' : (haveLearned ? 'factory+user' : 'factory')
    },
    user: {
      capacity: LUM_USER_POINTS,
      residuals: residuals.map((one) => ({
        ...one,
        ...(one.sat > 0 ? { percent: percentForTaught(one.lux, one.decades) } : {})
      })),
      // Present only once something has actually been refit from a colour
      // correction - Luminance::learnedFit()'s own gate, mirrored here so a
      // fit that quietly repeats the factory numbers does not masquerade as
      // "something was learned".
      ...(fit ? { fit } : {})
    }
  };
}

/**
 * The surface the diagram draws, at full saturation.
 *
 * Fetched once rather than polled: it changes when the filesystem image
 * changes. Same shape as the firmware's - three parallel grids rather than an
 * object per cell, so `curl` output lines up and the response stays small.
 */
app.get('/luminance/surface', (req, res) => {
  if (!factory.valid) {
    return res.json({ valid: false, error: factory.error });
  }
  const hue = [];
  for (let degrees = 0; degrees < factory.huePeriod; degrees += LUM_SURFACE_HUE_STEP) {
    hue.push(degrees);
  }
  const { logLuxMin, logLuxMax } = factory.cone;
  const lux = [];
  for (let i = 0; i < LUM_SURFACE_LUX_ROWS; i += 1) {
    const logLux = logLuxMin + ((logLuxMax - logLuxMin) * i) / (LUM_SURFACE_LUX_ROWS - 1);
    lux.push(10 ** logLux);
  }
  const percent = [];
  const limited = [];
  for (const one of lux) {
    percent.push(hue.map((degrees) => factoryPercent(one, degrees, 100).percent));
    limited.push(hue.map((degrees) => factoryPercent(one, degrees, 100).limited));
  }
  res.json({
    valid: true,
    error: '',
    profileId: factory.profileId,
    checksum: factory.checksum,
    sat: 100,
    minPercent: curve.minPercent,
    maxPercent: curve.maxPercent,
    lux,
    hue,
    percent,
    limited
  });
});


/**
 * The two writes on the brightness screen: drop one point, or drop them all.
 *
 * Behind the lock while the GET above is not - see guarded().
 */
app.post('/luminance', (req, res) => {
  if (guarded(res)) return;
  const body = req.body || {};

  if (body.reset) {
    curve.points = [];
    nudge = null;
    defaultLine();
    return res.json(luminanceState());
  }

  if (body.minPercent !== undefined || body.maxPercent !== undefined) {
    const low = Number(body.minPercent ?? curve.minPercent);
    const high = Number(body.maxPercent ?? curve.maxPercent);
    if (!(low >= 1 && high <= 100 && high >= low + 5)) {
      return res.status(400).json({ error: 'lumRange' });
    }
    curve.minPercent = low;
    curve.maxPercent = high;
    // Which points count as censored depends on where the ceiling is, so the
    // line has to be worked out again rather than only stored.
    fit();
    return res.json(luminanceState());
  }

  if (Number.isInteger(body.forget)) {
    if (body.forget < 0 || body.forget >= curve.points.length) {
      return res.status(404).json({ error: 'lumNoSuchPoint' });
    }
    // Spliced out rather than swapped away: the order is the order things
    // happened, and the fit anchors on the newest point.
    curve.points.splice(body.forget, 1);
    fit();
    return res.json(luminanceState());
  }

  // One colour correction, by its place in `user.residuals`.
  if (Number.isInteger(body.forgetResidual)) {
    if (body.forgetResidual < 0 || body.forgetResidual >= residuals.length) {
      return res.status(404).json({ error: 'lumNoSuchPoint' });
    }
    residuals.splice(body.forgetResidual, 1);
    return res.json(luminanceState());
  }

  /*
   * Back to the factory baseline: the corrections go, the coupling stays.
   *
   * Refused when there is no valid profile to restore *to* - otherwise
   * "restore" would quietly mean "delete", which is the one thing this button
   * must not do behind a word that promises the opposite.
   */
  if (body.factoryRestore) {
    if (!factory.valid) {
      return res.status(409).json({
        error: 'factoryUnavailable',
        errorDetail: 'there is no valid factory profile to restore to'
      });
    }
    residuals = [];
    curve.points = [];
    nudge = null;
    defaultLine();
    recordedProfile = factory.checksum;
    residualsProfile = factory.checksum;
    return res.json(luminanceState());
  }

  res.status(400).json({ error: 'calibrationRange' });
});

/*
 * The face.
 *
 * The firmware reads this off its own frame buffer; the mock has none, so it
 * renders the German panel from the wall clock time - enough to see the layout
 * and the corners move while working on the colour tab, and not an attempt to
 * be a second renderer. Only German, and only the standard dialect: the point
 * here is the geometry, and the real thing is one flash away.
 */
const PANEL_DE = [
  'ESKISTAFÜNF',
  'ZEHNZWANZIG',
  'DREIVIERTEL',
  'VORFUNKNACH',
  'HALBAELFÜNF',
  'EINSXAMZWEI',
  'DREIAUJVIER',
  'SECHSNLACHT',
  'SIEBENZWÖLF',
  'ZEHNEUNKUHR'
];

// row, column, text - the same three things the firmware's Word carries.
const W = {
  ES: [0, 0, 2], IST: [0, 3, 3], VOR: [3, 0, 3], NACH: [3, 7, 4], UHR: [9, 8, 3],
  FUENF: [0, 7, 4], ZEHN: [1, 0, 4], VIERTEL: [2, 4, 7], ZWANZIG: [1, 4, 7],
  HALB: [4, 0, 4],
  H: [
    [8, 6, 5], [5, 0, 3], [5, 7, 4], [6, 0, 4], [6, 7, 4], [4, 7, 4], [7, 0, 5],
    [8, 0, 6], [7, 7, 4], [9, 3, 4], [9, 0, 4], [4, 5, 3]
  ]
};

/** A FastLED hue (0..255) as #rrggbb, near enough for the mock. */
function hueHex(hue) {
  const h = (hue / 255) * 360;
  const c = 1;
  const x = 1 - Math.abs(((h / 60) % 2) - 1);
  const rgb =
    h < 60 ? [c, x, 0] :
    h < 120 ? [x, c, 0] :
    h < 180 ? [0, c, x] :
    h < 240 ? [0, x, c] :
    h < 300 ? [x, 0, c] : [c, 0, x];
  return '#' + rgb.map((p) => Math.round(p * 255).toString(16).padStart(2, '0')).join('');
}

function panelState() {
  const now = new Date();
  const grid = PANEL_DE.map(() => Array(11).fill('.'));
  const light = ([row, col, len]) => {
    for (let i = 0; i < len; i++) grid[row][col + i] = '#';
  };

  light(W.ES);
  light(W.IST);

  const step = Math.floor(now.getMinutes() / 5);
  let hours = now.getHours();
  let full = false;

  if (step === 0) full = true;
  else if (step === 1) { light(W.FUENF); light(W.NACH); }
  else if (step === 2) { light(W.ZEHN); light(W.NACH); }
  else if (step === 3) { light(W.VIERTEL); light(W.NACH); }
  else if (step === 4) { light(W.ZWANZIG); light(W.NACH); }
  else if (step === 5) { light(W.FUENF); light(W.VOR); light(W.HALB); hours++; }
  else if (step === 6) { light(W.HALB); hours++; }
  else if (step === 7) { light(W.FUENF); light(W.NACH); light(W.HALB); hours++; }
  else if (step === 8) { light(W.ZWANZIG); light(W.VOR); hours++; }
  else if (step === 9) { light(W.VIERTEL); light(W.VOR); hours++; }
  else if (step === 10) { light(W.ZEHN); light(W.VOR); hours++; }
  else { light(W.FUENF); light(W.VOR); hours++; }

  if (full) light(W.UHR);
  light(W.H[hours % 12]);

  // Reading order - top left, top right, bottom right, bottom left - which is
  // also clockwise, so the corners light in that order one way and 1,0,3,2 the
  // other. Same two sequences as Renderer::setCorners and cornerHue(); this
  // used to run 3,0,1,2, written before the mapping was established against a
  // real clock and never revisited.
  const remainder = now.getMinutes() % 5;
  const order = state.cornerDirection ? [0, 1, 2, 3] : [1, 0, 3, 2];
  const corners = [false, false, false, false];
  for (let i = 0; i < remainder; i++) corners[order[i]] = true;

  // With the coloured corners on, each lit corner shows a hue of its own: the
  // newest walks the wheel with the seconds, the ones before it sit at cyan.
  // The firmware reads these off the driver; the mock works them out from the
  // wall clock, and converts the hue with a plain HSV rather than FastLED's
  // rainbow - close enough to develop the layout against, and not a claim to
  // be the same colour.
  let cornerColors;
  if (state.cornerColor) {
    cornerColors = ['', '', '', ''];
    for (let i = 0; i < remainder; i++) {
      const hue = i + 1 === remainder ? (3 * now.getSeconds()) % 256 : 180;
      cornerColors[order[i]] = hueHex(hue);
    }
  }

  const on = grid.map((row) => row.join(''));
  const text = on
    .map((row, r) =>
      [...row]
        .map((cell, c) => (cell === '#' ? PANEL_DE[r][c] : ' '))
        .join('')
        .split(/\s+/)
        .filter(Boolean)
        .join(' ')
    )
    .filter(Boolean)
    .join(' ');

  return {
    ...(cornerColors ? { cornerColors } : {}),
    language: 0,
    code: 'de-DE',
    name: 'Deutsch',
    uiLocale: 'de',
    mode: state.display,
    rows: PANEL_DE,
    on,
    corners,
    text
  };
}

/*
 * The languages the clock can render.
 *
 * The firmware builds this out of src/languages/, where the names live; the
 * mock repeats them, because it has no language files to read. It is the one
 * place in this file that has to be updated when a language is added - the
 * web UI no longer has any such place at all, which was the point.
 */
const LANGUAGES = [
  { value: 0, code: 'de-DE', name: 'Deutsch', uiLocale: 'de', panel: 0 },
  { value: 1, code: 'de-SW', name: 'Schwäbisch', uiLocale: 'de', panel: 0 },
  { value: 2, code: 'de-BA', name: 'Bayrisch', uiLocale: 'de', panel: 0 },
  { value: 3, code: 'de-SA', name: 'Sächsisch', uiLocale: 'de', panel: 0 },
  { value: 4, code: 'de-CH', name: 'Schwiizerdütsch', uiLocale: 'de', panel: 4 },
  { value: 5, code: 'en', name: 'English', uiLocale: 'en', panel: 5 },
  { value: 6, code: 'fr', name: 'Français', uiLocale: 'fr', panel: 6 },
  { value: 7, code: 'it', name: 'Italiano', uiLocale: 'it', panel: 7 },
  { value: 8, code: 'nl', name: 'Nederlands', uiLocale: 'nl', panel: 8 },
  { value: 9, code: 'es', name: 'Español', uiLocale: 'es', panel: 9 }
];

/*
 * `panel` groups the languages cut into the same sheet of letters - the number
 * of the first language using it. The firmware works this out by comparing the
 * panels themselves; the mock has no panels but the German one, so the groups
 * are written out. Only the German four share.
 */

app.get('/languages', (req, res) => res.json(LANGUAGES));

app.get('/panel', (req, res) => res.json(panelState()));

app.get('/wifi', (req, res) => res.json({ ...wifi, hostname: state.hostname }));

/**
 * Renames the clock. Reduces the request to a DNS label exactly as the
 * firmware does, and answers with what was stored rather than what was asked
 * for, so the UI can be developed against the trimming behaviour.
 */
app.post('/hostname', (req, res) => {
  const clean = String(req.body.hostname ?? '')
    .replace(/[^A-Za-z0-9-]/g, '')
    .slice(0, 32)
    .replace(/^-+|-+$/g, '');

  if (!clean) {
    res.status(400).json({ error: 'hostnameInvalid' });
    return;
  }

  // The clock restarts to take the name on, and says so; the mock only
  // reports it, so the UI's wait-for-it-to-come-back path can be exercised.
  const restarting = clean !== state.hostname;
  state.hostname = clean;
  console.log('/hostname ->', clean, restarting ? '(restarting)' : '(unchanged)');
  res.json({ hostname: clean, restarting });
});

app.get('/wifi/scan', (req, res) => {
  const now = Date.now();
  if (!scanFinishedAt) {
    scanFinishedAt = now + 2500; // pretend a scan takes a moment
  }
  if (now < scanFinishedAt) {
    return res.json({ scanning: true });
  }
  scanFinishedAt = 0;
  res.json({ scanning: false, networks: FAKE_NETWORKS });
});

app.post('/wifi', (req, res) => {
  console.log('/wifi', { ...req.body, password: '***' });
  const { ssid, password } = req.body;
  if (!ssid) return res.status(400).json({ error: 'ssid missing' });

  const previous = { ssid: wifi.ssid, ip: wifi.ip };
  wifi.switching = true;
  wifi.error = '';
  wifi.errorDetail = '';
  res.json({ msg: '' });

  setTimeout(() => {
    if (password === 'wrong') {
      // A code, not a sentence - the UI translates it. See lib/errors.js.
      wifi.error = 'wifiConnect';
      wifi.errorDetail = ssid;
      wifi.ssid = previous.ssid;
      wifi.ip = previous.ip;
    } else {
      wifi.ssid = ssid;
      wifi.ip = '192.168.1.' + (40 + Math.floor(Math.random() * 20));
      wifi.rssi = -50 - Math.floor(Math.random() * 30);
    }
    wifi.switching = false;
    console.log('/wifi switch finished:', wifi.error || `now on ${wifi.ssid}`);
  }, 4000);
});

/* ------------------------------------------------------------ portal ---- */
/* The setup portal's own endpoints - see src/Portal.h and PORTAL_MODE above.
 * `/portal/scan` answers in the same shape as `/wifi/scan` on purpose: one
 * Svelte component (NetworkList.svelte) draws both. Connect with the
 * password `wrong` to exercise the failure path, same convention as `/wifi`
 * above. */

const portal = { state: 'idle', ssid: '', ip: '', error: '', errorDetail: '' };
let portalScanFinishedAt = 0;

app.get('/portal/status', (req, res) =>
  res.json({
    portal: true,
    apName: state.hostname,
    hostname: state.hostname,
    state: portal.state,
    ssid: portal.ssid,
    ip: portal.ip,
    error: portal.error,
    errorDetail: portal.errorDetail
  })
);

app.get('/portal/scan', (req, res) => {
  const now = Date.now();
  if (!portalScanFinishedAt) {
    portalScanFinishedAt = now + 2500;
  }
  if (now < portalScanFinishedAt) {
    return res.json({ scanning: true });
  }
  portalScanFinishedAt = 0;
  res.json({ scanning: false, networks: FAKE_NETWORKS });
});

app.post('/portal/connect', (req, res) => {
  const { ssid, password } = req.body ?? {};
  if (!ssid) return res.status(400).json({ error: 'wifiNoSsid' });
  if (portal.state === 'connecting') return res.status(409).json({ error: 'portalBusy' });

  portal.ssid = ssid;
  portal.state = 'connecting';
  portal.error = '';
  portal.errorDetail = '';
  portal.ip = '';
  console.log('/portal/connect', { ssid, password: '***' });
  res.json({ state: portal.state, ssid: portal.ssid });

  setTimeout(() => {
    if (password === 'wrong') {
      portal.state = 'failed';
      portal.error = 'wifiConnect';
      portal.errorDetail = ssid;
    } else {
      portal.state = 'connected';
      portal.ip = '192.168.1.' + (40 + Math.floor(Math.random() * 20));
    }
    console.log('/portal/connect finished:', portal.error || `connected as ${portal.ip}`);
  }, 4000);
});

// ----------------------------------------------------------------- OTA ----
// Simulates a firmware update from the browser: the upload is throttled to a
// plausible WiFi rate so the progress bar can be judged, and afterwards the
// mock plays dead for a moment like a rebooting clock. Name a file *bad*.bin
// to exercise the failure path.

const UPLOAD_RATE = 400 * 1024; // bytes/s, roughly what the ESP32 manages
const REBOOT_MS = 6000;

const ota = {
  firmwareVersion: '2.0.0',
  fsVersion: '2.0.0',
  sketchSize: 1287856,
  freeSpace: 6553600,
  error: '',
  errorDetail: '',
  partition: 'app0',
  channel: 0, // 0 = stable, 1 = edge
  autoUpdate: false,
  checkInterval: 24,
  state: 'idle',
  progress: 0,
  availableVersion: '',
  availableNotes: '',
  updateAvailable: false,
  lastCheck: -1
};

// What the two channels would offer. Stable is one patch level ahead of what
// is installed; edge is a `git describe` string, which is deliberately not
// comparable - the firmware only asks whether it differs.
const CHANNELS = [
  { version: '2.0.1', notes: 'Fixes the corner LEDs running backwards in Swiss German.' },
  { version: '2.0.0-7-g4f2a1c9', notes: 'Rolling build of the latest commit on main.' }
];

let rebootUntil = 0;

/** Recomputes the derived fields the firmware also derives. */
function refreshOta() {
  ota.updateAvailable =
    ota.availableVersion !== '' &&
    (ota.availableVersion !== ota.firmwareVersion || ota.availableVersion !== ota.fsVersion);
  return ota;
}

app.get('/ota/status', (req, res) => {
  if (Date.now() < rebootUntil) return res.status(503).json({ error: 'rebooting' });
  res.json(refreshOta());
});

app.get('/ota/check', (req, res) => {
  const offer = CHANNELS[ota.channel] ?? CHANNELS[0];
  ota.availableVersion = offer.version;
  ota.availableNotes = offer.notes;
  ota.lastCheck = 0;
  refreshOta();
  ota.state = ota.updateAvailable ? 'available' : 'idle';
  console.log('/ota/check ->', ota.availableVersion, ota.state);
  res.json(ota);
});

app.post('/ota/install', (req, res) => {
  if (!refreshOta().updateAvailable) {
    return res.status(409).json({ error: 'otaNoUpdate' });
  }
  if (ota.state === 'downloading') {
    return res.status(409).json({ error: 'otaBusy' });
  }

  ota.state = 'downloading';
  ota.progress = 0;
  ota.error = '';
  res.json(ota);

  // Walk the progress up the way a real download does, then reboot.
  const tick = setInterval(() => {
    ota.progress += 4;
    if (ota.progress >= 100) {
      clearInterval(tick);
      ota.progress = 100;
      ota.firmwareVersion = ota.availableVersion;
      ota.fsVersion = ota.availableVersion;
      ota.partition = ota.partition === 'app0' ? 'app1' : 'app0';
      ota.state = 'installed';
      refreshOta();
      rebootUntil = Date.now() + REBOOT_MS;
      console.log('/ota/install finished ->', ota.firmwareVersion, 'from', ota.partition);
    }
  }, 400);
});

app.post('/ota/config', (req, res) => {
  console.log('/ota/config', req.body);
  if (req.body.channel !== undefined && req.body.channel !== ota.channel) {
    ota.channel = req.body.channel;
    // A different channel says nothing about what the old one offered.
    ota.availableVersion = '';
    ota.availableNotes = '';
    ota.lastCheck = -1;
    ota.state = 'idle';
  }
  if (req.body.autoUpdate !== undefined) ota.autoUpdate = Boolean(req.body.autoUpdate);
  if (req.body.checkInterval !== undefined) ota.checkInterval = req.body.checkInterval;
  res.json(refreshOta());
});

/** Next patch level, so an update visibly changes something. */
function bumped(version) {
  const parts = version.split('.');
  parts[2] = String(Number(parts[2] || 0) + 1);
  return parts.join('.');
}

app.post('/ota/upload', (req, res) => {
  let bytes = 0;
  let kind = null;
  let filename = '';

  req.on('data', (chunk) => {
    if (kind === null) {
      // Shortcut a real parser: the multipart header always fits in the first
      // chunk, so the image starts right after the first blank line. The
      // firmware makes the same 0xE9 test, just on a clean stream.
      const head = chunk.indexOf('\r\n\r\n');
      kind = head >= 0 && chunk[head + 4] === 0xe9 ? 'firmware' : 'filesystem';
      filename = (chunk.toString('latin1', 0, Math.max(head, 0)).match(/filename="([^"]*)"/) || [])[1] || '';
    }
    bytes += chunk.length;

    // Throttle, otherwise a 3.5 MB image is through before it can be watched.
    req.pause();
    setTimeout(() => req.resume(), (chunk.length / UPLOAD_RATE) * 1000);
  });

  req.on('end', () => {
    console.log(`/ota/upload ${filename} (${kind}, ${bytes} bytes)`);

    if (/bad/i.test(filename)) {
      return res.status(500).json({ error: 'otaIncomplete', errorDetail: 'Bad Magic Byte' });
    }

    if (kind === 'firmware') {
      ota.firmwareVersion = bumped(ota.firmwareVersion);
      // A firmware upload boots from the other slot, same as on the device.
      ota.partition = ota.partition === 'app0' ? 'app1' : 'app0';
    } else {
      ota.fsVersion = bumped(ota.fsVersion);
    }

    rebootUntil = Date.now() + REBOOT_MS;
    res.json({ msg: '', reboot: true });
  });
});

/*
 * The log, as the debug tab sees it.
 *
 * A boot is written into the ring at startup so the tab has something with a
 * shape to it - the point of the whole feature is that opening it shows the
 * beginning, and a mock that starts empty would not exercise that at all.
 * A line is added every few seconds afterwards, and the ring is deliberately
 * short so the "lines were missed" path can be reached by leaving the tab
 * closed for a while.
 */
const LOG_LINES = 200;
const LOG_BATCH = 100;

const log = [];
let logSeq = 0;
const bootAt = Date.now();

/** RemoteDebug's levels: 2 debug, 3 info, 4 warning, 5 error. */
function logLine(level, text, ms = Date.now() - bootAt) {
  log.push({ s: logSeq++, t: ms, l: level, m: text });
  if (log.length > LOG_LINES) log.shift();
}

// A boot, roughly as the firmware writes one, timestamps included.
[
  [12, 3, '(setup)(C1) LittleFS Mount succesfull'],
  [31, 3, '(loadSettings)(C1) Settings read from NVS, schema 2'],
  [44, 2, '(1) wifi: wifi driver task: 3fcaf3d4, prio:23, stack:6656'],
  [212, 3, '(WiFiEvent)(C1) WiFi STA started'],
  [230, 3, '(applyTxPower)(C1) STA: Sendeleistung auf 13.0 dBm gesetzt'],
  [1893, 6, '(setup)(C1) Connected - Local IP: 172.22.102.219'],
  [1894, 6, '(setup)(C1) Compiled: Aug 20 2026 / 16:04:11'],
  [1895, 6, '(setup)(C1) Version: 2.0.2-9-gd1c3252'],
  [1896, 6, '(setup)(C1) Sendeleistung: 13.00 dBm, RSSI -56 dBm'],
  [1902, 6, '(begin)(C1) TSL2591 found, sampling every 2000 ms'],
  [2140, 4, '(startNtp)(C1) NTP has not answered yet, time is not set'],
  [3980, 3, '(onNtpSync)(C0) NTP sync, system clock set'],
  [4001, 3, '(loop)(C1) Display 16:05 CEST (UTC 14:05) [DE] | ES IST FUENF NACH VIER | corners +0']
].forEach(([ms, level, text]) => logLine(level, text, ms));

// Once a minute the firmware logs what is on the face; the mock is quicker so
// the window visibly moves while it is being looked at.
setInterval(() => {
  const now = new Date();
  const hh = String(now.getHours()).padStart(2, '0');
  const mm = String(Math.floor(now.getMinutes() / 5) * 5).padStart(2, '0');
  logLine(3, `(loop)(C1) Display ${hh}:${mm} CEST [DE] | ES IST … | corners +${now.getMinutes() % 5}`);
}, 5000);

// Something to colour: a warning and an error every so often.
setInterval(() => logLine(4, '(poll)(C1) NTP resync overdue, last sync 2 h ago'), 47000);
setInterval(() => logLine(5, '(readLux)(C0) TSL2591 read failed, keeping last value'), 71000);

app.get('/log', (req, res) => {
  const since = Number(req.query.since ?? 0);
  const oldest = log.length ? log[0].s : logSeq;
  const from = Math.max(since, oldest);

  const lines = log.filter((line) => line.s >= from).slice(0, LOG_BATCH);
  const seq = lines.length ? lines[lines.length - 1].s + 1 : from;

  res.json({
    oldest,
    seq,
    more: seq < logSeq,
    uptime: Date.now() - bootAt,
    // Drifts a little, so the numbers are not obviously frozen.
    heap: 208000 + Math.round(Math.sin(Date.now() / 30000) * 4000),
    heapMin: 181240,
    heapBlock: 106496,
    reset: 'software',
    lines
  });
});

/*
 * The clock's filesystem, as far as the mock needs it.
 *
 * A real directory rather than an object in memory, because the point of the
 * tab is uploading and downloading files and both want something that behaves
 * like flash: .mockfs/, seeded on first run with roughly what the image holds
 * and gitignored. It is deliberately not data/ - the file explorer can delete
 * things, and deleting the built web UI out from under the dev server would be
 * a puzzling way to find that out.
 *
 * See src/FileRoutes.cpp for the rules; this repeats the shape of them so the
 * debug tab can be worked on without a clock. Where the two disagree, the
 * clock is right.
 */
const FS_ROOT = path.resolve('.mockfs');
const FS_PATH_MAX = 63;
const FS_LIST_MAX = 96;
const FS_EDIT_MAX = 24576;

// A pretend 3.5 MB LittleFS partition, so the fullness bar means something.
const FS_TOTAL = 3538944;

/** Seeded once, so a fresh clone has a tree to look at. */
const FS_SEED = {
  'index.html': '<!doctype html>\n<html lang="de">\n  <head>\n    <meta charset="utf-8" />\n    <title>QlockThreeW32</title>\n  </head>\n  <body></body>\n</html>\n',
  'version.json': JSON.stringify({ version: '2.2.0', built: new Date().toISOString() }, null, 2),
  'zones.json': JSON.stringify({ tzdata: '2026a', zones: { 'Europe/Berlin': 'CET-1CEST,M3.5.0,M10.5.0/3' } }, null, 2),
  'assets/index-b7f21a.js': '// pretend bundle\nconsole.log("qlock");\n',
  'assets/index-b7f21a.css': ':root { --accent: #c05a1e; }\n'
};

function fsSeed() {
  if (fs.existsSync(FS_ROOT)) return;
  for (const [name, body] of Object.entries(FS_SEED)) {
    const target = path.join(FS_ROOT, name);
    fs.mkdirSync(path.dirname(target), { recursive: true });
    fs.writeFileSync(target, body);
  }
}
fsSeed();

/**
 * The same check the firmware makes, plus the one it gets for free: on the
 * clock every path is inside the volume by construction, here the resolved
 * path has to be proven to still sit under .mockfs.
 */
function fsResolve(raw) {
  if (typeof raw !== 'string' || raw.length === 0 || raw[0] !== '/') return null;
  if (raw.length > FS_PATH_MAX) return null;
  if (raw.includes('\\')) return null;

  const clean = raw.replace(/\/+/g, '/').replace(/(.)\/+$/, '$1');
  if (clean.split('/').some((part) => part === '.' || part === '..')) return null;

  const full = path.resolve(FS_ROOT, '.' + clean);
  if (full !== FS_ROOT && !full.startsWith(FS_ROOT + path.sep)) return null;
  return { clean, full };
}

const fsUsed = (dir = FS_ROOT) =>
  fs.readdirSync(dir, { withFileTypes: true }).reduce((sum, entry) => {
    const full = path.join(dir, entry.name);
    return sum + (entry.isDirectory() ? fsUsed(full) : fs.statSync(full).size);
  }, 0);

const fsVolume = () => ({ total: FS_TOTAL, used: fsUsed() });

const fsEditable = (name, size) =>
  size <= FS_EDIT_MAX &&
  (/\.(json|txt|css|html?|js|csv|scad|md)$/.test(name) || !name.includes('.'));

const fsMime = (name) =>
  ({
    '.html': 'text/html', '.htm': 'text/html', '.css': 'text/css',
    '.js': 'application/javascript', '.json': 'application/json',
    '.svg': 'image/svg+xml', '.png': 'image/png', '.ico': 'image/x-icon',
    '.txt': 'text/plain', '.scad': 'text/plain', '.gz': 'application/gzip'
  })[path.extname(name)] || 'application/octet-stream';

app.get('/fs/list', (req, res) => {
  const at = fsResolve(req.query.path ?? '/');
  if (!at) return res.status(400).json({ error: 'fsPath' });
  if (!fs.existsSync(at.full)) return res.status(404).json({ error: 'fsNotFound' });
  if (!fs.statSync(at.full).isDirectory()) return res.status(400).json({ error: 'fsNotDir' });

  const all = fs.readdirSync(at.full, { withFileTypes: true });
  const entries = all.slice(0, FS_LIST_MAX).map((entry) => {
    const size = entry.isDirectory() ? 0 : fs.statSync(path.join(at.full, entry.name)).size;
    return {
      name: entry.name,
      dir: entry.isDirectory(),
      size,
      ...(entry.isDirectory() ? {} : { edit: fsEditable(entry.name, size) })
    };
  });

  res.json({
    path: at.clean,
    ...fsVolume(),
    editMax: FS_EDIT_MAX,
    entries,
    ...(all.length > FS_LIST_MAX ? { truncated: true } : {})
  });
});

app.get('/fs/read', (req, res) => {
  const at = fsResolve(req.query.path ?? '');
  if (!at) return res.status(400).json({ error: 'fsPath' });
  if (!fs.existsSync(at.full)) return res.status(404).json({ error: 'fsNotFound' });
  if (fs.statSync(at.full).isDirectory()) return res.status(400).json({ error: 'fsIsDir' });

  const name = path.basename(at.clean);
  if (req.query.download !== undefined) {
    res.set('Content-Disposition', `attachment; filename="${name}"`);
    res.type('application/octet-stream');
  } else {
    res.type(fsMime(name));
  }
  res.send(fs.readFileSync(at.full));
});

/*
 * Multipart, split by hand.
 *
 * The firmware streams multipart because it must - the alternative is a bundle
 * sitting in the ESP32's heap - so the browser sends multipart, so the mock has
 * to read it. One part, one boundary, no encoding games: enough for the one
 * upload this sends, and cheaper than a dependency that would only ever be
 * used here.
 */
app.post('/fs/upload', express.raw({ type: 'multipart/form-data', limit: '8mb' }), (req, res) => {
  const at = fsResolve(req.query.path ?? '');
  if (!at || at.clean === '/') return res.status(400).json({ error: 'fsPath' });

  const boundary = /boundary=(?:"([^"]+)"|([^;]+))/.exec(req.headers['content-type'] ?? '');
  if (!boundary) return res.status(400).json({ error: 'fsBody' });

  const body = req.body;
  const mark = Buffer.from(`--${boundary[1] ?? boundary[2]}`);
  const head = body.indexOf('\r\n\r\n', body.indexOf(mark));
  const tail = body.indexOf(mark, head + 4);
  if (head < 0 || tail < 0) return res.status(400).json({ error: 'fsBody' });

  // The CRLF before the closing boundary belongs to the boundary, not the file.
  const content = body.subarray(head + 4, tail - 2);

  fs.mkdirSync(path.dirname(at.full), { recursive: true });
  fs.writeFileSync(at.full, content);
  res.json({ path: at.clean, size: content.length, ...fsVolume() });
});

app.post('/fs/save', (req, res) => {
  const at = fsResolve(req.body.path ?? '');
  if (!at || at.clean === '/') return res.status(400).json({ error: 'fsPath' });

  const content = String(req.body.content ?? '');
  if (content.length > FS_EDIT_MAX) return res.status(413).json({ error: 'fsTooBig' });

  fs.writeFileSync(at.full, content);
  res.json({ path: at.clean, size: Buffer.byteLength(content), ...fsVolume() });
});

app.post('/fs/delete', (req, res) => {
  const at = fsResolve(req.body.path ?? '');
  if (!at || at.clean === '/') return res.status(400).json({ error: 'fsPath' });
  if (!fs.existsSync(at.full)) return res.status(404).json({ error: 'fsNotFound' });

  if (fs.statSync(at.full).isDirectory()) {
    // Not recursive, the same as the clock: a file explorer that empties a
    // tree on one click is how the web UI gets deleted by accident.
    if (fs.readdirSync(at.full).length) return res.status(409).json({ error: 'fsNotEmpty' });
    fs.rmdirSync(at.full);
  } else {
    fs.unlinkSync(at.full);
  }
  res.json({ path: at.clean, ...fsVolume() });
});

app.post('/fs/mkdir', (req, res) => {
  const at = fsResolve(req.body.path ?? '');
  if (!at || at.clean === '/') return res.status(400).json({ error: 'fsPath' });
  if (fs.existsSync(at.full)) return res.status(409).json({ error: 'fsExists' });

  // LittleFS does not do -p, so neither does this.
  if (!fs.existsSync(path.dirname(at.full))) return res.status(500).json({ error: 'fsMkdir' });
  fs.mkdirSync(at.full);
  res.json({ path: at.clean });
});

/*
 * NVS, as far as the mock needs it.
 *
 * A flat list of {ns, key, type, value}, persisted to .mocknvs.json so an edit
 * made while developing survives a restart of this server - which is the whole
 * point of NVS on the clock and would be a strange thing for the stand-in to
 * get wrong.
 *
 * See src/NvsRoutes.h for the pretence this is built on and where it stops.
 * The seed is what a real clock holds: the three namespaces this firmware
 * writes, plus one of the WiFi driver's, because a tree that shows only the
 * tidy half would give a misleading idea of what is in there.
 */
const NVS_STORE = path.resolve('.mocknvs.json');
const NVS_EDIT_MAX = 4096;
const NVS_PEEK_MAX = 2048;

// NVS accounts for itself in 32-byte entries; a 20 kB partition is about this.
const NVS_TOTAL_ENTRIES = 630;

const NVS_SEED = [
  {
    ns: 'qlock', key: 'conf', type: 'str',
    value: JSON.stringify({
      Schema: 2, Language: 2, RenderCornerColor: 0, CornerDirection: 1,
      Brightness: 40, Hue: 134, Saturation: 50, UseLdr: 1, Mode: 1,
      Hostname: 'QlockThreeW32', NtpServer: 'pool.ntp.org', TzZone: 'Europe/Berlin',
      UseDs: 1, TzName: 'CET', TzWeek: 0, TzDoW: 1, TzMonth: 10, TzHour: 3, TzOffset: 60,
      TzDsName: 'CEST', TzDsWeek: 0, TzDsDoW: 1, TzDsMonth: 3, TzDsHour: 2, TzDsOffset: 120
    })
  },
  {
    ns: 'qlocklight', key: 'curve', type: 'str',
    value: JSON.stringify({
      slope: 54.2, offset: 48.3,
      points: [{ lux: 7.4, percent: 55, seconds: 1840 }]
    })
  },
  { ns: 'qlockexpert', key: 'hash', type: 'blob', bytes: 32 },
  { ns: 'qlockexpert', key: 'salt', type: 'blob', bytes: 16 },
  { ns: 'qlockexpert', key: 'on', type: 'u8', value: '1' },
  { ns: 'nvs.net80211', key: 'sta.ssid', type: 'blob', bytes: 36 },
  { ns: 'nvs.net80211', key: 'sta.pswd', type: 'blob', bytes: 64 },
  { ns: 'nvs.net80211', key: 'opmode', type: 'u8', value: '1' }
];

function nvsLoad() {
  if (fs.existsSync(NVS_STORE)) {
    try {
      return JSON.parse(fs.readFileSync(NVS_STORE, 'utf8'));
    } catch {
      /* rewritten from the seed below */
    }
  }
  fs.writeFileSync(NVS_STORE, JSON.stringify(NVS_SEED, null, 2));
  return structuredClone(NVS_SEED);
}

let nvsEntries = nvsLoad();
const nvsFlush = () => fs.writeFileSync(NVS_STORE, JSON.stringify(nvsEntries, null, 2));

// The one value that is not handed out. See the note in src/NvsRoutes.h: it is
// the only secret here whose leak outlives the unlock that leaked it.
const nvsProtected = (entry) =>
  entry.ns === 'qlockexpert' && (entry.key === 'hash' || entry.key === 'salt');

const nvsIsInteger = (type) => /^[ui](8|16|32|64)$/.test(type);

const nvsFind = (ns, key) => nvsEntries.find((e) => e.ns === ns && e.key === key);

/** The size the clock would report: bytes of text, or bytes of blob. */
const nvsSize = (entry) =>
  entry.type === 'blob' ? entry.bytes : Buffer.byteLength(entry.value ?? '');

/** The suffix, which is a reading of the value and not a stored name. */
function nvsSuffix(entry) {
  if (entry.type === 'blob') return 'bin';
  if (nvsIsInteger(entry.type)) return 'txt';
  if (nvsSize(entry) > NVS_PEEK_MAX) return 'bin';
  const first = (entry.value ?? '').trimStart()[0];
  return first === '{' || first === '[' ? 'json' : 'txt';
}

app.get('/nvs/list', (req, res) => {
  res.json({
    entries: nvsEntries.map((entry) => {
      const guarded = nvsProtected(entry);
      return {
        ns: entry.ns,
        key: entry.key,
        type: entry.type,
        size: guarded ? 0 : nvsSize(entry),
        suffix: guarded ? 'bin' : nvsSuffix(entry),
        ...(guarded
          ? { protected: true }
          : { edit: entry.type !== 'blob' && nvsSize(entry) <= NVS_EDIT_MAX })
      };
    }),
    used: nvsEntries.length + new Set(nvsEntries.map((e) => e.ns)).size,
    total: NVS_TOTAL_ENTRIES,
    namespaces: new Set(nvsEntries.map((e) => e.ns)).size,
    editMax: NVS_EDIT_MAX
  });
});

app.get('/nvs/read', (req, res) => {
  const entry = nvsFind(req.query.ns, req.query.key);
  if (!entry) return res.status(404).json({ error: 'nvsNotFound' });
  if (nvsProtected(entry)) return res.status(403).json({ error: 'nvsProtected' });

  if (entry.type === 'blob') {
    // No text form, so it is a download whether or not one was asked for -
    // which is exactly what `.bin` in the tree is telling the reader.
    res.set('Content-Disposition', `attachment; filename="${entry.key}.bin"`);
    return res.type('application/octet-stream').send(Buffer.alloc(entry.bytes, 0x5a));
  }

  if (req.query.download !== undefined) {
    res.set('Content-Disposition', `attachment; filename="${entry.key}.${nvsSuffix(entry)}"`);
    return res.type('application/octet-stream').send(entry.value);
  }
  res.type(nvsSuffix(entry) === 'json' ? 'application/json' : 'text/plain').send(entry.value);
});

app.post('/nvs/save', (req, res) => {
  const { ns, key, content } = req.body;
  if (!ns || !key) return res.status(400).json({ error: 'nvsPath' });

  const entry = nvsFind(ns, key);
  if (!entry) return res.status(404).json({ error: 'nvsNotFound' });
  if (nvsProtected(entry)) return res.status(403).json({ error: 'nvsProtected' });
  if (entry.type === 'blob') return res.status(400).json({ error: 'nvsBinary' });
  if (String(content).length > NVS_EDIT_MAX) return res.status(413).json({ error: 'nvsTooBig' });

  // The type the store already has decides what may go back. Writing a string
  // over an integer would leave the firmware reading it as missing, and a
  // setting that silently reverts to its default is the worst failure here.
  if (nvsIsInteger(entry.type) && !/^-?\d+\s*$/.test(String(content))) {
    return res.status(400).json({ error: 'nvsNotANumber' });
  }

  entry.value = String(content);
  nvsFlush();
  res.json({ ns, key, size: Buffer.byteLength(entry.value), cached: true });
});

app.post('/nvs/delete', (req, res) => {
  const { ns, key } = req.body;
  if (!ns || !key) return res.status(400).json({ error: 'nvsPath' });

  const before = nvsEntries.length;
  nvsEntries = nvsEntries.filter((e) => !(e.ns === ns && e.key === key));
  if (nvsEntries.length === before) return res.status(404).json({ error: 'nvsNotFound' });

  nvsFlush();
  res.json({ ns, key });
});


/*
 * Restart, which the NVS panel offers beside its warning about the firmware
 * keeping records in RAM. Nothing to restart here, so it answers the same
 * shape and says so in the console - enough for the button to be exercised.
 */
app.post('/restart', (req, res) => {
  if (!expert.on) return res.status(403).json({ error: 'expertLocked' });
  console.log('mock: restart asked for');
  res.json({ restarting: true });
});

// 8080 is what vite.config.js proxies to, so that is the default and nobody
// has to set anything. The override exists for the API-shape tests, which
// start a second copy and must not fight a mock somebody left running.
const PORT = Number(process.env.QLOCK_MOCK_PORT || 8080);
app.listen(PORT, () => {
  console.log(`QlockThreeW32 mock API on http://localhost:${PORT}`);
  console.log(PORTAL_MODE
    ? 'Setup portal mode - open /portal.html through the Vite dev server.'
    : 'Normal operation - open /index.html (or / ) through the Vite dev server.');
});
