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

  ntpServer: 'pool.ntp.org',
  useDs: true,
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
  hostname: 'QlockThreeW32',
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

app.get('/wifi', (req, res) => res.json(wifi));

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

app.listen(8080, () => console.log('QlockThreeW32 mock API on http://localhost:8080'));
