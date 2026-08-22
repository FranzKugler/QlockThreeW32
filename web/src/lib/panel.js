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
 * U+2032 PRIME, the apostrophe of English O'CLOCK and Italian L'UNA. On the
 * panel the letter and its apostrophe share one milled opening and one LED, so
 * they are one cell here too. The firmware states the rule in Language.h and
 * sends the row as it is written; splitting it is this side's half.
 */
const PRIME = '′';

/**
 * The cells of one row: characters, except that a prime rides on the one
 * before it.
 *
 * Split with the spread operator rather than with `charAt`, because rows carry
 * Ü and Ö and a UTF-16 index is not a character index for those - and then
 * re-joined by the prime rule, because a character index is not a cell index
 * for O′ either.
 */
function rowCells(row) {
  const out = [];
  for (const character of row) {
    if (character === PRIME && out.length > 0) out[out.length - 1] += character;
    else out.push(character);
  }
  return out;
}

/**
 * Turns the two grids into one flat list of cells, which is what a CSS grid
 * wants. Keyed by position rather than by letter: a panel has the same letter
 * many times over, and a keyed each would throw on the repeat.
 */
export function cells(panel) {
  if (!panel?.rows) return [];

  const out = [];
  for (let row = 0; row < PANEL_ROWS; row++) {
    const letters = rowCells(panel.rows[row] ?? '');
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
