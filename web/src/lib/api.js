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
 * Writes the two points of the automatic brightness curve.
 *
 * Not through post(), which reports and returns a bare boolean: the clock
 * validates the pair and answers either with the curve it stored or with a
 * code saying why it did not, and the calibration button has to show which.
 */
/**
 * "At the light there is right now, I want this much display."
 *
 * Shifts the curve rather than setting a brightness: with the automatic on,
 * the slider is not a level any more but a preference, and the clock keeps it
 * at every other light level too. The current reading is left to the firmware,
 * which has it first-hand - this UI polls, and would be a step behind.
 */
export const nudgeBrightness = (want) => setLightCurve({ want });

export async function setLightCurve(curve) {
  try {
    const res = await fetch('/light', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(curve)
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
