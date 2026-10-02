import { Graphics } from "pixi.js";

import { UNITS_PER_CELL } from "../state/territory.js";

// The cells you may capture (GAME-021, GameState.mayCapture): a thin line of your
// faction's colour just inside each one's edges — a hint, not a highlight.
const LINE = { width: 1, alpha: 0.5, inset: 1.5 }; // px; the inset keeps it off the gridline
const MIN_CELL_PX = 12; // smaller (the full-map view): nothing to mark

export class CaptureHintView {
  readonly gfx = new Graphics();

  // `cols`: the map width in cells; `cellPx`: a cell's size on screen.
  draw(
    cells: readonly number[],
    cols: number,
    toScreen: (x: number, y: number) => [number, number],
    cellPx: number,
    color: number,
  ): void {
    const g = this.gfx;
    g.clear();
    if (cells.length === 0 || cellPx < MIN_CELL_PX) return;
    const side = cellPx - 2 * LINE.inset;
    for (const i of cells) {
      const [sx, sy] = toScreen((i % cols) * UNITS_PER_CELL, Math.floor(i / cols) * UNITS_PER_CELL);
      g.rect(sx + LINE.inset, sy + LINE.inset, side, side);
    }
    g.stroke({ width: LINE.width, color, alpha: LINE.alpha });
  }
}
