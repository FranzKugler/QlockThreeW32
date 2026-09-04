/**
 * api
 * REST client for the clock's endpoints. The firmware (src/main .cpp) and the
 * mock server (server.js) implement the same contract; keep all three in sync.
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 */
import { status } from './status.svelte.js';
import { dict } from './i18n.svelte.js';
import { errorText } from './errors.js';

async function post(path, body) {
  try {
    const res = await fetch(path, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    });
    if (!res.ok) {
      // The firmware answers a refusal with a code, and a banner reading
      // "HTTP 403" tells nobody what to do about it. A body that is not the
      // expected shape leaves the status, which is better than nothing.
      let said = null;
      try {
        const err = await res.json();
        said = errorText(dict(), err.error, err.errorDetail);
      } catch {
        /* not JSON, or no body at all */
      }
      throw new Error(said || `HTTP ${res.status}`);
    }
    status.error = null;
    return true;
  } catch (err) {
    // Writes are fire-and-forget: report, but never break the UI.
    status.error = `${path}: ${err.message}`;
    return false;
  }
}

/** The clock's refusal as a sentence, or the status when it did not say. */
async function failure(res, what) {
  try {
    const err = await res.json();
    const said = errorText(dict(), err.error, err.errorDetail);
    if (said) return said;
  } catch {
    /* not JSON, or no body */
  }
  return `${what}: HTTP ${res.status}`;
}

/** Loads the full settings object. Throws, so the caller can offer a retry. */
export async function fetchState() {
  const res = await fetch('/currentState');
  if (!res.ok) throw new Error(`/currentState: HTTP ${res.status}`);
  status.error = null;
  return res.json();
}

export const setDisplay = (display) => post('/display', { display });

/**
 * Colour and brightness, and each field is optional.
 *
 * A body carrying `lum` is a statement about the brightness, and with the
 * automatic on the clock reads it as a lesson - "at this light, I want this
 * much". So the colour wheel must not send one: it used to, unchanged, and
 * every drag of the wheel taught the automatic a calibration point.
 */
export const setColor = (fields) => post('/color', fields);

/**
 * What the light sensor sees, and whether there is one at all. Throws, so the
 * caller can tell "no sensor fitted" from "could not ask".
 */
export async function fetchLight() {
  const res = await fetch('/light');
  if (!res.ok) throw new Error(`/light: HTTP ${res.status}`);
  return res.json();
}

export const setAutoLuminance = (automaticLum) =>
  post('/autoluminance', { automaticLum: automaticLum ? 1 : 0 });

/**
 * Everything the automatic brightness is thinking: the line, the points it was
 * fitted through, and what it makes of the light right now.
 *
 * Open, while the writes below are not: looking at the curve is what somebody
 * does when the automatic feels wrong, and a password in front of a diagnosis
 * helps nobody. Throws, so the screen can say the clock did not answer rather
 * than draw an empty chart.
 */
export async function fetchLuminance() {
  const res = await fetch('/luminance');
  if (!res.ok) throw new Error(`/luminance: HTTP ${res.status}`);
  return res.json();
}

/**
 * Forgets one taught point, by its place in the list.
 *
 * A point can be wrong rather than merely old - a correction made just after
 * the light changed used to be stored at the light level the room had left -
 * and until this existed the only remedy was throwing the whole calibration
 * away. Behind expert mode, like every other write here.
 *
 * Answers with the curve as it now stands, so the screen redraws from what the
 * clock says rather than from what it assumes happened.
 */
export async function forgetLightPoint(index) {
  return writeLuminance({ forget: index });
}

/**
 * Moves the ends of the regulated range.
 *
 * Both are sent together even when only one moved, because the firmware checks
 * them against each other - a minimum posted alone would be validated against
 * whatever maximum the clock happens to hold, which is what the screen is
 * showing anyway but need not be.
 */
export async function setLightRange(minPercent, maxPercent) {
  return writeLuminance({ minPercent, maxPercent });
}

/** Throws every point away, from the brightness screen rather than the colour tab. */
export async function resetLightPoints() {
  return writeLuminance({ reset: true });
}

/**
 * Forgets one colour correction, by its place in `user.residuals`.
 *
 * The same argument as forgetting a taught point, in the layer above it: a
 * correction can be wrong rather than merely old - made ten seconds after
 * somebody turned a lamp off - and the only remedy before this was throwing
 * all of them away with a factory restore.
 */
export async function forgetResidual(index) {
  return writeLuminance({ forgetResidual: index });
}

/**
 * Back to the factory baseline.
 *
 * What goes: the colour corrections learned from this clock's nudges, and the
 * white points that are the same preferences said in the old coordinates.
 * What stays: the **coupling measurement**, which is the clock having measured
 * where its own sensor sits behind its own letters and has nothing to do with
 * anybody's taste. Twenty minutes to redo, and no reason to.
 *
 * Behind expert mode, like every other write here, and refused by the clock
 * when there is no valid profile to restore *to* - which is the case worth
 * refusing, because otherwise "restore" would mean "delete".
 */
export async function restoreFactoryLuminance() {
  return writeLuminance({ factoryRestore: true });
}

/**
 * The colour-aware surface: what the factory profile asks for at every ambient
 * level and every hue, at full saturation.
 *
 * Fetched **once**, not polled. It is 3 KB of measurement that changes when
 * the filesystem image changes, which is a reboot away; /luminance carries the
 * one number that moves. Throws, so the diagram can say the clock did not
 * answer rather than draw an empty box that looks like a clock with no model.
 */
export async function fetchLuminanceSurface() {
  const res = await fetch('/luminance/surface');
  if (!res.ok) throw new Error(`/luminance/surface: HTTP ${res.status}`);
  return res.json();
}

/**
 * Starts the clock measuring its own coupling, and stops one.
 *
 * On /light rather than /luminance because what it produces is the same map
 * the script uploads. The progress arrives in /luminance, which the screen is
 * already polling once a second - a second poller would only be a second
 * answer to disagree with.
 */
export async function startCalibration() {
  return writeLight({ calibrate: true });
}

export async function abortCalibration() {
  return writeLight({ calibrateAbort: true });
}

async function writeLight(payload) {
  const res = await fetch('/light', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  });
  const body = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(errorText(dict(), body.error, body.errorDetail) || `HTTP ${res.status}`);
  return body;
}

/** Removes the coupling map, so the clock stops compensating its own face. */
export async function resetCoupling() {
  return writeLight({ couplingReset: true });
}

async function writeLuminance(payload) {
  const res = await fetch('/luminance', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  });
  const body = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(errorText(dict(), body.error, body.errorDetail) || `HTTP ${res.status}`);
  return body;
}

/**
 * Throws the calibration away and restores the default line.
 *
 * The only thing left to write about the curve. There is nothing to set any
 * more: it is taught by moving the brightness slider while the automatic is
 * on, which goes to /color like any other slider move.
 *
 * Not through post(), which reports and returns a bare boolean: the clock
 * answers with the curve it now has, and the tab shows that.
 */
export async function resetLightCurve() {
  try {
    const res = await fetch('/light', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ reset: true })
    });
    const body = await res.json();
    if (!res.ok) return { error: body.error || `HTTP ${res.status}` };
    status.error = null;
    return body;
  } catch (err) {
    status.error = `/light: ${err.message}`;
    return { error: 'connectionLost' };
  }
}

export const setConfiguration = ({ language, cornerColor, cornerDirection }) =>
  post('/configuration', { language, cornerColor, cornerDirection });

export const setTimezone = (tz) => post('/timezone', tz);

/**
 * Current WiFi connection. Throws, because the caller polls this while the
 * clock is switching networks and must tolerate it being briefly unreachable
 * without flagging that as an error to the user.
 */
export async function fetchWifi() {
  const res = await fetch('/wifi');
  if (!res.ok) throw new Error(`/wifi: HTTP ${res.status}`);
  return res.json();
}

/** One poll of the async scan: `{ scanning: true }` or `{ networks: [...] }`. */
export async function fetchWifiScan() {
  const res = await fetch('/wifi/scan');
  if (!res.ok) throw new Error(`/wifi/scan: HTTP ${res.status}`);
  return res.json();
}

export const connectWifi = ({ ssid, password }) => post('/wifi', { ssid, password });

/* ------ the setup portal ------
 *
 * Separate routes from the ones above, and deliberately so: the portal runs in
 * access-point mode with a different state machine behind it, and the two
 * would only be one set of endpoints if they meant the same thing. The scan
 * answers in the same *shape*, which is what lets NetworkList.svelte draw
 * both.
 */

export async function fetchPortalStatus() {
  const res = await fetch('/portal/status');
  if (!res.ok) throw new Error(`/portal/status: HTTP ${res.status}`);
  return res.json();
}

export async function fetchPortalScan() {
  const res = await fetch('/portal/scan');
  if (!res.ok) throw new Error(`/portal/scan: HTTP ${res.status}`);
  return res.json();
}

/** Throws: the portal is the one screen where a refusal has to be shown at once. */
export async function portalConnect({ ssid, password }) {
  const res = await fetch('/portal/connect', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ ssid, password })
  });
  if (!res.ok) throw new Error(await failure(res, '/portal/connect'));
  return res.json();
}

/**
 * Renames the clock. Answers with the name that was actually stored, which the
 * firmware may have reduced to a valid DNS label - so the caller should show
 * what comes back rather than what it sent.
 */
export async function setHostname(hostname) {
  const res = await fetch('/hostname', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ hostname })
  });
  if (!res.ok) {
    let code = `HTTP ${res.status}`;
    try {
      code = (await res.json()).error || code;
    } catch {
      /* not JSON */
    }
    throw new Error(code);
  }
  return res.json();
}

/**
 * The languages this clock can render, in the order of the numbers it stores -
 * so `value` is what POST /configuration wants. Static for a given firmware,
 * asked once.
 *
 * Throws, so the caller can fall back to the built-in list rather than end up
 * with an empty picker on a firmware that predates the endpoint.
 */
export async function fetchLanguages() {
  const res = await fetch('/languages');
  if (!res.ok) throw new Error(`/languages: HTTP ${res.status}`);
  return res.json();
}

/**
 * The face as it is at this moment: the panel of the language that is running,
 * and which of its letters are lit, read off the frame buffer itself.
 *
 * Throws, so the caller can tell "this firmware has no /panel" from "the clock
 * did not answer" and leave the preview empty rather than wrong.
 */
export async function fetchPanel() {
  const res = await fetch('/panel');
  if (!res.ok) throw new Error(`/panel: HTTP ${res.status}`);
  return res.json();
}

/**
 * Whether the clock is unlocked, whether a password has ever been set, and how
 * much of the reset window is left. Carries no secret.
 *
 * Throws: the shell asks for this before it can decide which tabs exist, and a
 * clock that cannot answer has to be treated as locked rather than as open.
 */
export async function fetchExpert() {
  const res = await fetch('/expert');
  if (!res.ok) throw new Error(`/expert: HTTP ${res.status}`);
  return res.json();
}

/**
 * Sets, checks or clears the password: `{password}`, `{off: true}` or
 * `{reset: true}`.
 *
 * Answers with the same shape `fetchExpert` does, so the caller has the new
 * state without asking again - or with `{error}` naming why not. Not through
 * post(), which only reports a boolean: which of the four refusals it was is
 * the whole content of the answer here.
 */
export async function setExpert(body) {
  try {
    const res = await fetch('/expert', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    });
    const answer = await res.json();
    if (!res.ok) return { error: answer.error || `HTTP ${res.status}` };
    status.error = null;
    return answer;
  } catch (err) {
    status.error = `/expert: ${err.message}`;
    return { error: 'connectionLost' };
  }
}

/**
 * The clock's log, everything after the sequence number last seen.
 *
 * `since` of 0 asks for the oldest the ring still holds, which is what fills a
 * freshly opened tab with the boot rather than with "from now on". The answer
 * carries `more` while further lines are waiting, so the caller keeps asking
 * instead of collecting one batch every polling interval.
 *
 * Throws, because the caller polls this and a clock that is briefly busy or
 * restarting must not turn into a permanent error banner.
 */
export async function fetchLog(since = 0) {
  const res = await fetch(`/log?since=${since}`);
  if (!res.ok) throw new Error(`/log: HTTP ${res.status}`);
  return res.json();
}

/**
 * Installed firmware and web UI versions. Throws, because the caller polls this
 * while the clock reboots after an update and must tolerate it being briefly
 * unreachable without flagging that as an error.
 */
export async function fetchOtaStatus() {
  const res = await fetch('/ota/status');
  if (!res.ok) throw new Error(`/ota/status: HTTP ${res.status}`);
  return res.json();
}

/**
 * Asks the clock to poll its release channel. Takes a second or two, since the
 * clock fetches the manifest while answering. Returns the same shape as
 * /ota/status.
 */
export async function checkForUpdate() {
  const res = await fetch('/ota/check');
  if (!res.ok) throw new Error(`/ota/check: HTTP ${res.status}`);
  return res.json();
}

/**
 * Starts the download. Answers immediately - the clock installs in a task of
 * its own, so the caller polls /ota/status for progress.
 */
export async function installUpdate() {
  const res = await fetch('/ota/install', { method: 'POST' });
  if (!res.ok) {
    let message = `HTTP ${res.status}`;
    try {
      message = (await res.json()).error || message;
    } catch {
      /* not JSON */
    }
    throw new Error(message);
  }
  return res.json();
}

/** Channel, automatic updates and check interval. */
export async function setOtaConfig(config) {
  const res = await fetch('/ota/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(config)
  });
  if (!res.ok) throw new Error(`/ota/config: HTTP ${res.status}`);
  return res.json();
}

/**
 * Uploads a firmware or filesystem image; the clock decides from the image
 * itself where it belongs and reboots into it.
 *
 * Uses XMLHttpRequest rather than fetch(), because only XHR reports upload
 * progress. That number is honest here: the clock writes every chunk to flash
 * as it arrives, so bytes sent really are bytes flashed.
 */
export function uploadImage(file, onProgress) {
  return new Promise((resolve, reject) => {
    const body = new FormData();
    body.append('image', file, file.name);

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/ota/upload');

    xhr.upload.onprogress = (event) => {
      if (event.lengthComputable) onProgress(event.loaded / event.total);
    };

    xhr.onload = () => {
      if (xhr.status >= 200 && xhr.status < 300) {
        resolve();
        return;
      }
      // The clock answers failures as {"error": "..."}; fall back to the
      // status code if the body is something else entirely.
      let message = `HTTP ${xhr.status}`;
      try {
        message = JSON.parse(xhr.responseText).error || message;
      } catch {
        /* not JSON */
      }
      reject(new Error(message));
    };

    // Fires when the connection dies mid-upload, which on a clock that is
    // being reflashed is not necessarily a failure - the caller checks back.
    xhr.onerror = () => reject(new Error(dict().connectionLost));
    xhr.onabort = () => reject(new Error(dict().uploadAborted));

    xhr.send(body);
  });
}

/* ------ the filesystem, for the file explorer in the debug tab ------
 *
 * These all throw rather than going through post(), which reports into the
 * banner and answers with a bare boolean. A file explorer needs to know
 * whether the delete happened - it has a tree to refresh and a message to put
 * next to the file - and "something went wrong somewhere on the page" is not
 * that. errorText() still turns the clock's code into a sentence.
 *
 * This is LittleFS, not NVS: the partition the web UI itself is served from.
 */

/** Everything the clock says about one directory, plus how full it is. */
export async function fetchDirectory(path = '/') {
  const res = await fetch(`/fs/list?path=${encodeURIComponent(path)}`);
  if (!res.ok) throw new Error(await fsFailure(res, '/fs/list'));
  return res.json();
}

/** One file as text, for the editor. Binary files are downloaded instead. */
export async function fetchFile(path) {
  const res = await fetch(`/fs/read?path=${encodeURIComponent(path)}`);
  if (!res.ok) throw new Error(await fsFailure(res, '/fs/read'));
  return res.text();
}

/**
 * Where to point a download link.
 *
 * A plain link rather than a fetch and a blob: expert mode is a flag in the
 * clock's NVS, not a header this page adds, so an ordinary GET is already
 * authorised and the browser saves the file itself with no copy in memory.
 */
export const fileUrl = (path) =>
  `/fs/read?path=${encodeURIComponent(path)}&download=1`;

export const saveFile = (path, content) => fsWrite('/fs/save', { path, content });
export const deleteEntry = (path) => fsWrite('/fs/delete', { path });
export const makeDirectory = (path) => fsWrite('/fs/mkdir', { path });

/**
 * Sends one file to the clock, as multipart so the firmware can stream it
 * into flash instead of holding it in the heap.
 *
 * XMLHttpRequest for the same reason uploadImage() uses it: only XHR reports
 * upload progress, and here too the number is honest - the clock writes each
 * chunk as it arrives.
 */
export function uploadFile(path, file, onProgress) {
  return new Promise((resolve, reject) => {
    const body = new FormData();
    body.append('file', file, file.name);

    const xhr = new XMLHttpRequest();
    xhr.open('POST', `/fs/upload?path=${encodeURIComponent(path)}`);

    xhr.upload.onprogress = (event) => {
      if (event.lengthComputable) onProgress?.(event.loaded / event.total);
    };

    xhr.onload = () => {
      if (xhr.status >= 200 && xhr.status < 300) {
        resolve(JSON.parse(xhr.responseText));
        return;
      }
      let message = `HTTP ${xhr.status}`;
      try {
        const err = JSON.parse(xhr.responseText);
        message = errorText(dict(), err.error, err.errorDetail) || message;
      } catch {
        /* not JSON */
      }
      reject(new Error(message));
    };

    xhr.onerror = () => reject(new Error(dict().connectionLost));
    xhr.onabort = () => reject(new Error(dict().uploadAborted));

    xhr.send(body);
  });
}

/** The clock's refusal as a sentence, or the status when it did not say. */
async function fsFailure(res, what) {
  try {
    const err = await res.json();
    const said = errorText(dict(), err.error, err.errorDetail);
    if (said) return said;
  } catch {
    /* not JSON, or no body */
  }
  return `${what}: HTTP ${res.status}`;
}

async function fsWrite(path, body) {
  const res = await fetch(path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  });
  if (!res.ok) throw new Error(await fsFailure(res, path));
  return res.json();
}

/* ------ NVS, shown as a two-level tree beside the filesystem ------
 *
 * A namespace is not a folder and a key is not a file; see src/NvsRoutes.h for
 * how far that pretence is carried and exactly where it stops. These are the
 * four calls it rests on, and they take the namespace and key apart rather
 * than a path, because that is what the store actually has.
 */

/** Every entry in the partition, in one answer. */
export async function fetchNvs() {
  const res = await fetch('/nvs/list');
  if (!res.ok) throw new Error(await fsFailure(res, '/nvs/list'));
  return res.json();
}

/** One value as text. Blobs have none and are downloaded instead. */
export async function readNvs(ns, key) {
  const res = await fetch(nvsQuery(ns, key));
  if (!res.ok) throw new Error(await fsFailure(res, '/nvs/read'));
  return res.text();
}

export const nvsUrl = (ns, key) => `${nvsQuery(ns, key)}&download=1`;

export const saveNvs = (ns, key, content) => fsWrite('/nvs/save', { ns, key, content });
export const deleteNvs = (ns, key) => fsWrite('/nvs/delete', { ns, key });

const nvsQuery = (ns, key) =>
  `/nvs/read?ns=${encodeURIComponent(ns)}&key=${encodeURIComponent(key)}`;

/**
 * Restarts the clock.
 *
 * Its one caller is the NVS panel, where it is the answer to that panel's own
 * warning: the firmware holds its records in RAM and writes them back on the
 * next settings change, so an edit made there is only durable if the clock is
 * restarted before anything else touches it.
 *
 * Throws, so the button can say what went wrong. The connection dying right
 * afterwards is not a failure - the answer goes out before the restart.
 */
export async function restartClock() {
  const res = await fetch('/restart', { method: 'POST' });
  if (!res.ok) throw new Error(await fsFailure(res, '/restart'));
  return res.json();
}
