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
for (const prefix of ['/log', '/ota']) {
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
app.post('/color', accept(['hue', 'sat', 'lum']));
app.post('/configuration', accept(['language', 'cornerColor', 'cornerDirection']));
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
// Mirrors the defaults in src/Settings.cpp, and is changed by POST /light the
// way the clock's own curve is.
const DEFAULT_CURVE = { luxLow: 1.0, brightLow: 20, luxHigh: 200.0, brightHigh: 100 };
const curve = { ...DEFAULT_CURVE };

// Same constant as CALIBRATION_MIN_RATIO in src/LightSensor.h.
const MIN_RATIO = 4.0;

const clamp = (value) => Math.min(100, Math.max(1, Math.round(value)));

/** Where a reading sits between the two points, 0..1, on the log scale. */
function luxPosition(lux) {
  const floor = 0.01;
  const value = Math.max(lux, floor);
  const low = Math.max(curve.luxLow, floor);
  const high = Math.max(curve.luxHigh, low * MIN_RATIO);
  return Math.min(
    1,
    Math.max(0, (Math.log10(value) - Math.log10(low)) / (Math.log10(high) - Math.log10(low)))
  );
}

/** The same log interpolation brightnessForLux() does in the firmware. */
function brightnessForLux(lux) {
  return clamp(curve.brightLow + luxPosition(lux) * (curve.brightHigh - curve.brightLow));
}

// Roughly a lit living room seen through a dark front panel, drifting slowly
// so the tab has something to react to.
const currentLux = () => 7.4 + Math.sin(Date.now() / 20000) * 2;

/** The body of GET /light, shared with the POST which answers the same shape. */
function lightState() {
  const lux = currentLux();
  return {
    sensor: 'VEML7700',
    present: true,
    available: true,
    lux,
    raw: 7.1 + Math.sin(Date.now() / 3000) * 3,
    ...curve,
    brightness: brightnessForLux(lux),
    minRatio: MIN_RATIO
  };
}

app.get('/light', (req, res) => res.json(lightState()));

/**
 * Writes the brightness curve, and refuses the same pairs the firmware refuses
 * - a UI that only ever sees the happy path here would not show its error
 * state until it met a real clock.
 */
app.post('/light', (req, res) => {
  if (req.body.reset) {
    Object.assign(curve, DEFAULT_CURVE);
    return res.json(lightState());
  }

  // "At this light, I want this much" - translates the curve, keeping the
  // slope, and where that will not fit pins the overflowing end and solves the
  // other so the request is still met exactly. Same arithmetic as
  // updateLight() in WebRoutes.cpp; keep the two in step.
  if (typeof req.body.want === 'number') {
    const want = req.body.want;
    if (want < 1 || want > 100) return res.status(400).json({ error: 'calibrationRange' });

    const lux = currentLux();
    const delta = want - brightnessForLux(lux);
    let low = curve.brightLow + delta;
    let high = curve.brightHigh + delta;

    if (low < 1 || low > 100 || high < 1 || high > 100) {
      const position = luxPosition(lux);
      low = clamp(low);
      high = clamp(high);
      if (position <= 0.5) low = Math.round((want - position * high) / (1 - position));
      else high = Math.round((want - (1 - position) * low) / position);
      low = clamp(low);
      high = clamp(high);
    }

    curve.brightLow = low;
    curve.brightHigh = high;
    return res.json(lightState());
  }

  const { luxLow, brightLow, luxHigh, brightHigh } = req.body;

  if (!(luxHigh > luxLow * MIN_RATIO)) {
    return res.status(400).json({ error: 'calibrationTooClose' });
  }
  if (brightLow < 1 || brightLow > 100 || brightHigh < 1 || brightHigh > 100) {
    return res.status(400).json({ error: 'calibrationRange' });
  }

  Object.assign(curve, { luxLow, brightLow, luxHigh, brightHigh });
  res.json(lightState());
});

// The name lives in the settings, like it does on the clock, and is only
// reported here as well - so there is one copy of it, not two that drift.
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

app.listen(8080, () => console.log('QlockThreeW32 mock API on http://localhost:8080'));
