/**
 * status
 * Shared status of the last request to the clock. `api.js` writes to it so any
 * failed write shows up in the UI instead of being silently swallowed (which is
 * what the old jQuery `.done()`-only handlers did).
 *
 * @autor    Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 */
export const status = $state({
  /** Message of the last failed request, or null if the last request succeeded. */
  error: null
});
