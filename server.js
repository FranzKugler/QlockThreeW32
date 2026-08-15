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
  error: ''
};

const FAKE_NETWORKS = [
  { ssid: 'Heimnetz', rssi: -54, secure: true },
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
  res.json({ msg: '' });

  setTimeout(() => {
    if (password === 'wrong') {
      wifi.error = `Verbindung zu '${ssid}' fehlgeschlagen`;
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
  firmwareVersion: '2.1.0',
  fsVersion: '2.1.0',
  sketchSize: 1348192,
  freeSpace: 6553600,
  error: ''
};

let rebootUntil = 0;

app.get('/ota/status', (req, res) => {
  if (Date.now() < rebootUntil) return res.status(503).json({ error: 'rebooting' });
  res.json(ota);
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
      return res.status(500).json({ error: 'Image unvollstaendig: Bad Magic Byte' });
    }

    if (kind === 'firmware') ota.firmwareVersion = bumped(ota.firmwareVersion);
    else ota.fsVersion = bumped(ota.fsVersion);

    rebootUntil = Date.now() + REBOOT_MS;
    res.json({ msg: '', reboot: true });
  });
});

app.listen(8080, () => console.log('QlockThreeW32 mock API on http://localhost:8080'));
