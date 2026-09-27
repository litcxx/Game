// Layout of a centred row of equal cards near the bottom of the screen (the
// faction picker) — pure, so click hit-testing is checkable without a renderer.
export interface CardRow {
  x0: number; // left edge of the first card, canvas px
  y: number; // top edge of the row
  w: number;
  h: number;
  gap: number;
  count: number;
}

export const CARD_W = 150;
export const CARD_H = 44;
export const CARD_GAP = 12;

export function cardRow(screenW: number, screenH: number, count: number, bottomMargin: number): CardRow {
  const width = count * CARD_W + Math.max(0, count - 1) * CARD_GAP;
  return {
    x0: Math.round((screenW - width) / 2),
    y: Math.round(screenH - bottomMargin - CARD_H),
    w: CARD_W,
    h: CARD_H,
    gap: CARD_GAP,
    count,
  };
}

// Index of the card under a canvas point, or -1 (outside the row or in a gap).
export function cardAt(row: CardRow, sx: number, sy: number): number {
  if (sy < row.y || sy > row.y + row.h) return -1;
  const dx = sx - row.x0;
  if (dx < 0) return -1;
  const i = Math.floor(dx / (row.w + row.gap));
  if (i >= row.count) return -1;
  return dx - i * (row.w + row.gap) <= row.w ? i : -1; // not in the gap after it
}
