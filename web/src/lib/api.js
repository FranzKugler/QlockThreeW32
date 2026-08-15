/**
 * api
 * REST client for the clock's endpoints. The firmware (src/main .cpp) and the
 * mock server (server.js) implement the same contract; keep all three in sync.
 *
 * @autor    Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 */
import { status } from './status.svelte.js';

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
