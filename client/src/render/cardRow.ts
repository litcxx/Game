// Layout of the spawn controls in the middle of the screen — the faction choice
// (a note, a centred row of equal cards, «В бой» under them) and the lone respawn
// button — pure, so click hit-testing is checkable without a renderer.
export interface CardRow {
  x0: number; // left edge of the first card, canvas px
  y: number; // top edge of the row
  w: number;
  h: number;
  gap: number;
  count: number;
}

export const CARD_W = 170; // room for «в сети 25 · 33,3%» under the name
export const CARD_H = 44;
export const CARD_GAP = 12;
export const NOTE_GAP = 28; // between the note and the cards; the row's title sits in it
export const BUTTON_W = 180;
export const BUTTON_H = 44;
export const BUTTON_GAP = 16; // between the cards and «В бой»

export interface Rect {
  x: number;
  y: number;
  w: number;
  h: number;
}

// Whether a canvas point is on the rect (its edges included).
export function inRect(r: Rect, sx: number, sy: number): boolean {
  return sx >= r.x && sx <= r.x + r.w && sy >= r.y && sy <= r.y + r.h;
}

// A button in the middle of the screen (respawning).
export function centredButton(screenW: number, screenH: number): Rect {
  return { x: Math.round((screenW - BUTTON_W) / 2), y: Math.round((screenH - BUTTON_H) / 2), w: BUTTON_W, h: BUTTON_H };
}

// A row of `count` cards centred across the screen, its top edge at `top`.
export function cardRow(screenW: number, count: number, top: number): CardRow {
  const width = count * CARD_W + Math.max(0, count - 1) * CARD_GAP;
  return {
    x0: Math.round((screenW - width) / 2),
    y: Math.round(top),
    w: CARD_W,
    h: CARD_H,
    gap: CARD_GAP,
    count,
  };
}

// The choice as one block in the middle of the screen: the note (`noteH` px
// tall, centred at noteY), the gap, the row of cards, then «В бой».
export function choiceLayout(
  screenW: number,
  screenH: number,
  count: number,
  noteH: number,
): { noteY: number; row: CardRow; button: Rect } {
  const top = Math.round((screenH - (noteH + NOTE_GAP + CARD_H + BUTTON_GAP + BUTTON_H)) / 2);
  const row = cardRow(screenW, count, top + noteH + NOTE_GAP);
  const button = { x: Math.round((screenW - BUTTON_W) / 2), y: row.y + CARD_H + BUTTON_GAP, w: BUTTON_W, h: BUTTON_H };
  return { noteY: top + noteH / 2, row, button };
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
