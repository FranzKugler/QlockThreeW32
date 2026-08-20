/**
 * panel
 * The clock's face as the colour tab draws it.
 *
 * A tiny module rather than lines inside Color.svelte, because the shape of
 * what /panel answers is a contract with the firmware and deserves to be
 * stated once: ten rows of eleven characters, a second grid of `#` and `.`
 * saying which of them are lit, and four corners in reading order.
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  20.8.2026
 * @updated  20.8.2026
 */

export const PANEL_ROWS = 10;
export const PANEL_COLS = 11;

/**
 * Turns the two grids into one flat list of cells, which is what a CSS grid
 * wants. Keyed by position rather than by letter: a panel has the same letter
 * many times over, and a keyed each would throw on the repeat.
 *
 * The letters are split with the spread operator, not with `charAt`: rows
 * carry Ü and Ö, and a UTF-16 index is not a character index for those.
 */
export function cells(panel) {
  if (!panel?.rows) return [];

  const out = [];
  for (let row = 0; row < PANEL_ROWS; row++) {
    const letters = [...(panel.rows[row] ?? '')];
    const lit = panel.on?.[row] ?? '';
    for (let col = 0; col < PANEL_COLS; col++) {
      out.push({
        key: row * PANEL_COLS + col,
        letter: letters[col] ?? '',
        on: lit[col] === '#'
      });
    }
  }
  return out;
}
