// Fog of war on the client: what the local player knows about each cell.
//   unexplored — never seen: fully covered, its content unknown;
//   explored   — seen before but not now: the last known state, dimmed;
//   visible    — seen now: kept current by the server.
// The server decides what is visible (Snapshot.revealed / hidden); the client
// remembers what it has seen since — through deaths and respawns alike. What the
// faction explored before (another tab, before a reload or a restart, a newcomer's
// faction) comes with MapState.explored (GAME-020).
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

  // MapState.explored: the cells the faction has explored, a bit per cell (bit
  // i % 8 of byte i / 8). An unexplored one turns explored; a visible one stays
  // as it is. Returns whether any cell changed.
  explore(bits: Uint8Array): boolean {
    let changed = false;
    const cells = Math.min(this.sight.length, bits.length * 8);
    for (let i = 0; i < cells; i++) {
      if (this.sight[i] !== 0 || ((bits[i >> 3]! >> (i & 7)) & 1) === 0) continue;
      this.sight[i] = EXPLORED;
      changed = true;
    }
    return changed;
  }

  // Snapshot.resync: a frame was lost, so the cells held as visible may be stale —
  // they become explored until the resync reveals them again. Returns whether any
  // cell changed.
  forgetSight(): boolean {
    let changed = false;
    for (let i = 0; i < this.sight.length; i++) {
      if (this.sight[i] === VISIBLE) {
        this.sight[i] = EXPLORED;
        changed = true;
      }
    }
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
