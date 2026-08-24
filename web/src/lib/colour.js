/**
 * colour
 * The clock's hue and saturation, as a colour a browser can paint.
 *
 * Extracted from Color.svelte when a second screen needed it: the brightness
 * screen draws the colour each calibration point was taught in, and two copies
 * of a colour conversion is exactly the kind of thing that drifts apart and
 * then makes two screens disagree about the same clock.
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.2
 * @created  24.8.2026
 * @updated  24.8.2026
 */

/**
 * The same model the firmware uses: CHSV(hue, sat, 255) scaled by brightness,
 * which is a plain per-channel multiply - so brightness maps onto HSV value.
 *
 * Hue in degrees, saturation and value in per cent. Returns three floats in
 * 0..255, unrounded, because callers blend with them before painting.
 */
export function hsvRgb(h, s, v) {
  const S = s / 100;
  const V = v / 100;
  const c = V * S;
  const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
  const m = V - c;
  let rgb;
  if (h < 60) rgb = [c, x, 0];
  else if (h < 120) rgb = [x, c, 0];
  else if (h < 180) rgb = [0, c, x];
  else if (h < 240) rgb = [0, x, c];
  else if (h < 300) rgb = [x, 0, c];
  else rgb = [c, 0, x];
  return rgb.map((part) => (part + m) * 255);
}

export const css = (rgb) => `rgb(${rgb.map(Math.round).join(' ')})`;
