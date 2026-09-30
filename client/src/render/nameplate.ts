// Where a player's name pill and hp bar go, relative to the token, px — pure, so
// the layout is checkable without a renderer. The pill grows with the name; the
// hp bar has one width for everyone: tied to the pill, a long name read as more
// hp (playtest #0).
export const HP_BAR_W = 32;

export interface Rect {
  x: number;
  y: number;
  w: number;
  h: number;
}

export interface Nameplate {
  pill: Rect | undefined; // none before the roster names the player
  bar: Rect;
  fillW: number; // the hp share of the bar
}

const PILL_Y = -24;
const PILL_H = 15;
const BAR_H = 2;

// `nameWidth`: the rendered name, px (undefined: no name yet); `hpFrac`: hp / max.
export function nameplate(nameWidth: number | undefined, hpFrac: number): Nameplate {
  const pill =
    nameWidth === undefined
      ? undefined
      : { x: -Math.max(nameWidth + 10, 20) / 2, y: PILL_Y, w: Math.max(nameWidth + 10, 20), h: PILL_H };
  const bar = { x: -HP_BAR_W / 2, y: pill ? pill.y + pill.h : -13, w: HP_BAR_W, h: BAR_H };
  return { pill, bar, fillW: HP_BAR_W * Math.max(0, Math.min(1, hpFrac)) };
}
