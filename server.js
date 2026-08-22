/**
 * server
 * Mock of the clock's REST API, so the web UI can be developed without
 * hardware. Mirrors the endpoints in `src/main .cpp`; the initial values are
 * the defaults from the Settings constructor in `src/Settings.cpp`.
 *
 *   npm run mock   # this server on :8080
 *   npm run dev    # Vite dev server, proxies the API routes here
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

const app = express();

app.use(express.json());
app.use(express.urlencoded({ extended: true }));

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

// Everything behind the lock, in one list.
for (const prefix of ['/log', '/ota', '/fs']) {
  app.use(prefix, (req, res, next) => {
    if (expert.on) return next();
    res.status(403).json({ error: 'expertLocked' });
  });
}

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

app.use(express.static('data'));

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
      nudge = {
        percent: Math.min(LUM_MAX_PERCENT, Math.max(LUM_MIN_PERCENT, req.body.lum)),
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

const curve = { slope: 0, offset: 0, fitted: false, points: [] };

function defaultLine() {
  const lo = logLux(DEFAULT_LINE.lowLux);
  const hi = logLux(DEFAULT_LINE.highLux);
  curve.slope = (DEFAULT_LINE.highPercent - DEFAULT_LINE.lowPercent) / (hi - lo);
  curve.offset = DEFAULT_LINE.lowPercent - curve.slope * lo;
  curve.fitted = false;
}

function fit() {
  if (curve.points.length === 0) return defaultLine();

  const xs = curve.points.map((p) => logLux(p.lux));
  const ys = curve.points.map((p) => p.percent);
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
  return Math.min(LUM_MAX_PERCENT, Math.max(LUM_MIN_PERCENT, Math.round(raw)));
}

/*
 * A new point replaces a near neighbour rather than joining the queue, so ten
 * corrections made in one evening cannot push the daylight point out and
 * collapse the line onto a single lighting condition.
 */
function remember(lux, percent) {
  const near = curve.points.find(
    (p) => Math.max(lux / p.lux, p.lux / lux) <= LUM_SAME_LIGHT_RATIO
  );
  const point = { lux, percent, seconds: Math.round(process.uptime()) };
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

function settle() {
  if (nudge && Date.now() >= nudge.at) {
    remember(currentLux(), nudge.percent);
    nudge = null;
  }
}

/** The body of GET /light, shared with the POST which answers the same shape. */
function lightState() {
  settle();
  const lux = currentLux();
  return {
    sensor: 'VEML7700',
    present: true,
    available: true,
    lux,
    raw: 7.1 + Math.sin(Date.now() / 3000) * 3,
    slope: curve.slope,
    offset: curve.offset,
    fitted: curve.fitted,
    brightness: brightnessForLux(lux),
    minPercent: LUM_MIN_PERCENT,
    maxPercent: LUM_MAX_PERCENT,
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
  if (req.body.reset) {
    curve.points = [];
    nudge = null;
    defaultLine();
    return res.json(lightState());
  }
  res.status(400).json({ error: 'calibrationRange' });
});

/** Everything the automatic is thinking, for the #luminance screen. */
app.get('/luminance', (req, res) => {
  settle();
  const lux = currentLux();
  res.json({
    lux,
    raw: 7.1 + Math.sin(Date.now() / 3000) * 3,
    available: true,
    slope: curve.slope,
    offset: curve.offset,
    fitted: curve.fitted,
    brightness: brightnessForLux(lux),
    minPercent: LUM_MIN_PERCENT,
    maxPercent: LUM_MAX_PERCENT,
    capacity: LUM_POINTS,
    settleMs: LUM_SETTLE_MS,
    uptime: Math.round(process.uptime()),
    adjusting: nudge !== null,
    ...(nudge ? { wanted: nudge.percent } : {}),
    points: curve.points.map((p) => ({ ...p, curve: brightnessForLux(p.lux) }))
  });
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

app.listen(8080, () => console.log('QlockThreeW32 mock API on http://localhost:8080'));
