import { status } from './status.svelte.js';

/**
 * REST client for the clock's endpoints. The firmware (src/main .cpp) and the
 * mock server (server.js) implement the same contract; keep all three in sync.
 */

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
