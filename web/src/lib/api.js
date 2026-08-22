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

/** Loads the full settings object. Throws, so the caller can offer a retry. */
export async function fetchState() {
  const res = await fetch('/currentState');
  if (!res.ok) throw new Error(`/currentState: HTTP ${res.status}`);
  status.error = null;
  return res.json();
}

export const setDisplay = (display) => post('/display', { display });

export const setColor = ({ hue, sat, lum }) => post('/color', { hue, sat, lum });

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
 * Reached at #luminance, and read-only - the only write is resetLightCurve().
 * Throws, so the screen can say the clock did not answer rather than draw an
 * empty chart.
 */
export async function fetchLuminance() {
  const res = await fetch('/luminance');
  if (!res.ok) throw new Error(`/luminance: HTTP ${res.status}`);
  return res.json();
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
