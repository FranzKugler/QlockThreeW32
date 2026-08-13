/**
 * Mock of the clock's REST API, so the web UI can be developed without
 * hardware. Mirrors the endpoints in `src/main .cpp`; the initial values are
 * the defaults from the Settings constructor in `src/Settings.cpp`.
 *
 *   npm run mock   # this server on :8080
 *   npm run dev    # Vite dev server, proxies the API routes here
 *
 * It also serves data/ statically, so a production build can be checked
 * against the mock by opening http://localhost:8080 directly.
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
app.post('/autoluminance', accept(['automaticLum']));
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

app.listen(8080, () => console.log('QlockThreeW32 mock API on http://localhost:8080'));
