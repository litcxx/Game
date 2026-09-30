import { Graphics } from "pixi.js";

import type { Capital } from "../state/gameState.js";
import { UNITS_PER_CELL } from "../state/territory.js";

// The capitals' markers: a diamond in the faction's colour, white-rimmed, on a dark
// halo, at the centre of the capital's cell. Drawn over the fog — where the capitals
// are is known from the start (Welcome.map); who owns the land around them is not.
export class CapitalsView {
  readonly gfx = new Graphics();

  // `cols`: the map width in cells; `cellPx`: a cell's size on screen.
  draw(
    capitals: readonly Capital[],
    cols: number,
    toScreen: (x: number, y: number) => [number, number],
    cellPx: number,
    colorOf: (factionId: number) => number,
  ): void {
    const g = this.gfx;
    g.clear();
    const s = Math.max(7, cellPx * 0.3); // the diamond's half-diagonal, px
    for (const c of capitals) {
      const col = c.cell % cols;
      const row = Math.floor(c.cell / cols);
      const [sx, sy] = toScreen((col + 0.5) * UNITS_PER_CELL, (row + 0.5) * UNITS_PER_CELL);
      g.circle(sx, sy, s * 1.5).fill({ color: 0x0a0a0f, alpha: 0.6 });
      g.moveTo(sx, sy - s)
        .lineTo(sx + s, sy)
        .lineTo(sx, sy + s)
        .lineTo(sx - s, sy)
        .closePath()
        .fill(colorOf(c.factionId))
        .stroke({ width: 2, color: 0xffffff, alpha: 0.9 });
    }
  }
}
