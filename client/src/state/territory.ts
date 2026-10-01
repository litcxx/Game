import type { CellUpdate } from "../gen/game/v1/protocol_pb.js";

// A cell's side in world units.
export const UNITS_PER_CELL = 100;

// One cell as the client knows it: its owner and the capture in progress
// (faction ids, 0 = none; progress 0..100).
export interface CellState {
  index: number;
  owner: number;
  captureFaction: number;
  captureProgress: number;
}

// The territory grid as far as the client knows it: owner and capture state per
// cell (row-major). MapState sets it whole, Snapshot.cells change it; a cell in
// the fog keeps what was last seen of it. How much each faction owns is the
// server's count (FactionScores), not this.
export class Territory {
  cols = 1;
  rows = 1;
  owners: Uint8Array = new Uint8Array(0); // owner faction id (0 = neutral)
  captureFaction: Uint8Array = new Uint8Array(0); // who is capturing (0 = none)
  captureProgress: Uint8Array = new Uint8Array(0); // 0..100

  // The map's size (Welcome); its cells come with MapState.
  resize(cols: number, rows: number): void {
    this.cols = Math.max(1, cols);
    this.rows = Math.max(1, rows);
  }

  // Full territory state: owners plus any in-progress captures.
  setMapState(owners: Uint8Array, captures: readonly CellUpdate[]): void {
    this.owners = owners;
    this.captureFaction = new Uint8Array(owners.length);
    this.captureProgress = new Uint8Array(owners.length);
    for (const c of captures) this.setCapture(c);
  }

  // Incremental changes; returns whether there were any.
  applyCellUpdates(cells: readonly CellUpdate[]): boolean {
    for (const c of cells) {
      if (c.index < this.owners.length) this.owners[c.index] = c.ownerFactionId;
      this.setCapture(c);
    }
    return cells.length > 0;
  }

  // The cell at (col, row); off the map it reads as nothing.
  cell(col: number, row: number): CellState {
    const index = row * this.cols + col;
    if (col < 0 || row < 0 || col >= this.cols || row >= this.rows) {
      return { index, owner: 0, captureFaction: 0, captureProgress: 0 };
    }
    return {
      index,
      owner: this.owners[index] ?? 0,
      captureFaction: this.captureFaction[index] ?? 0,
      captureProgress: this.captureProgress[index] ?? 0,
    };
  }

  private setCapture(c: CellUpdate): void {
    if (c.index >= this.captureFaction.length) return;
    this.captureFaction[c.index] = c.captureFactionId;
    this.captureProgress[c.index] = Math.min(100, c.captureProgress);
  }
}
