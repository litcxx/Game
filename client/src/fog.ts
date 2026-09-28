// Fog of war on the client: what the local player knows about each cell.
//   unexplored — never seen: fully covered, its content unknown;
//   explored   — seen before but not now: the last known state, dimmed;
//   visible    — seen now: kept current by the server.
// The server decides what is visible (Snapshot.revealed / hidden); the client
// only remembers what it has ever seen — through deaths and respawns alike.
export type CellSight = "unexplored" | "explored" | "visible";

// A stretch of equal sight along one row: the renderer draws it as one rect.
export interface FogRun {
  col: number; // first column
  length: number; // in cells
  sight: CellSight;
}

const SIGHTS: readonly CellSight[] = ["unexplored", "explored", "visible"];
const EXPLORED = 1;
const VISIBLE = 2;

export class FogOfWar {
  private cols = 0;
  private sight = new Uint8Array(0); // index into SIGHTS per cell, row-major

  // A new map of cols x rows, all unexplored.
  reset(cols: number, rows: number): void {
    this.cols = Math.max(0, cols);
    this.sight = new Uint8Array(this.cols * Math.max(0, rows));
  }

  // One snapshot's deltas: revealed cells are visible, hidden ones explored.
  // Returns whether any cell changed (the map needs a redraw).
  apply(revealed: readonly number[], hidden: readonly number[]): boolean {
    let changed = false;
    const set = (index: number, value: number) => {
      if (index < 0 || index >= this.sight.length || this.sight[index] === value) return;
      this.sight[index] = value;
      changed = true;
    };
    for (const i of revealed) set(i, VISIBLE);
    for (const i of hidden) set(i, EXPLORED);
    return changed;
  }

  // Sight of a cell; off the map reads as unexplored.
  sightAt(index: number): CellSight {
    return SIGHTS[this.sight[index] ?? 0]!;
  }

  // Runs of equal sight across columns c0..c1 (inclusive) of one row.
  runs(row: number, c0: number, c1: number): FogRun[] {
    const out: FogRun[] = [];
    for (let col = c0; col <= c1; col++) {
      const sight = this.sightAt(row * this.cols + col);
      const last = out[out.length - 1];
      if (last !== undefined && last.sight === sight) last.length++;
      else out.push({ col, length: 1, sight });
    }
    return out;
  }
}
