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

async function post(path, body) {
  try {
    const res = await fetch(path, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
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

export const setAutoLuminance = (automaticLum) =>
  post('/autoluminance', { automaticLum: automaticLum ? 1 : 0 });

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
