import { Graphics } from "pixi.js";

import type { CellSight, FogOfWar } from "../fog.js";
import { UNITS_PER_CELL } from "../state/territory.js";

const BORDER = 0x33333f; // the map's bounds are always known
// Brightness falls visible > explored > unexplored. Unexplored: dense fog —
// nothing is known there; only the map's geometry (gridlines, coordinates)
// faintly shows through. Explored: the last known state, dimmed. Visible: as is.
const COVER: Record<CellSight, { color: number; alpha: number } | undefined> = {
  unexplored: { color: 0x07070a, alpha: 0.9 },
  explored: { color: 0x000000, alpha: 0.35 },
  visible: undefined,
};

// Draws fog of war over the on-screen territory, one rect per run of equal
// sight along a row (a mostly-fogged full-map view stays a few hundred rects).
// Sits above the grid and coordinates, below the players — the server sends
// none in the fog.
export class FogView {
  readonly gfx = new Graphics();

  draw(
    fog: FogOfWar,
    cols: readonly [number, number],
    rows: readonly [number, number],
    toScreen: (x: number, y: number) => [number, number],
    cellPx: number,
    mapSize: { cols: number; rows: number },
  ): void {
    const g = this.gfx;
    g.clear();
    for (let row = rows[0]; row <= rows[1]; row++) {
      for (const run of fog.runs(row, cols[0], cols[1])) {
        const cover = COVER[run.sight];
        if (cover === undefined) continue;
        const [sx, sy] = toScreen(run.col * UNITS_PER_CELL, row * UNITS_PER_CELL);
        g.rect(sx, sy, run.length * cellPx, cellPx).fill(cover);
      }
    }
    const [x0, y0] = toScreen(0, 0);
    const [x1, y1] = toScreen(mapSize.cols * UNITS_PER_CELL, mapSize.rows * UNITS_PER_CELL);
    g.rect(x0, y0, x1 - x0, y1 - y0).stroke({ width: 1, color: BORDER });
  }
}
