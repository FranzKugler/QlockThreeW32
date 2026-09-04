/**
 * signal
 * Signal strength as 1..4 bars, shared between the WLAN tab's own status card
 * and NetworkList.svelte - both draw the same bars for the same numbers.
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  1.0
 * @created  4.9.2026
 * @updated  4.9.2026
 */

/** Signal strength as 1..4 bars; t.quality holds the matching labels. */
export function bars(rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -67) return 3;
  if (rssi >= -75) return 2;
  return 1;
}
