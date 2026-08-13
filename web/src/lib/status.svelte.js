/**
 * Shared status of the last request to the clock. `api.js` writes to it so any
 * failed write shows up in the UI instead of being silently swallowed (which is
 * what the old jQuery `.done()`-only handlers did).
 */
export const status = $state({
  /** Message of the last failed request, or null if the last request succeeded. */
  error: null
});
