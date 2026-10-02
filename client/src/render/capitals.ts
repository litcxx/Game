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
      const [sx, sy] = toScreen(...capitalCentre(c.cell, cols));
      drawCapitalMarker(g, sx, sy, s, colorOf(c.factionId), 2);
    }
  }
}

// A capital cell's centre, in world units.
export function capitalCentre(cell: number, cols: number): [number, number] {
  return [((cell % cols) + 0.5) * UNITS_PER_CELL, (Math.floor(cell / cols) + 0.5) * UNITS_PER_CELL];
}

// A capital's marker at (x, y): a diamond of half-diagonal `s` in the faction's
// colour, white-rimmed, on a dark halo — on the map and, smaller, on the minimap.
export function drawCapitalMarker(g: Graphics, x: number, y: number, s: number, color: number, rim: number): void {
  g.circle(x, y, s * 1.5).fill({ color: 0x0a0a0f, alpha: 0.6 });
  g.moveTo(x, y - s)
    .lineTo(x + s, y)
    .lineTo(x, y + s)
    .lineTo(x - s, y)
    .closePath()
    .fill(color)
    .stroke({ width: rim, color: 0xffffff, alpha: 0.9 });
}
