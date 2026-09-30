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
// cell (row-major) and how many cells each faction owns. MapState sets it whole,
// Snapshot.cells change it; a cell in the fog keeps what was last seen of it.
export class Territory {
  cols = 1;
  rows = 1;
  owners: Uint8Array = new Uint8Array(0); // owner faction id (0 = neutral)
  captureFaction: Uint8Array = new Uint8Array(0); // who is capturing (0 = none)
  captureProgress: Uint8Array = new Uint8Array(0); // 0..100
  private readonly ownedCount = new Map<number, number>(); // faction id -> owned cells

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
    this.ownedCount.clear();
    for (const o of owners) {
      if (o !== 0) this.ownedCount.set(o, (this.ownedCount.get(o) ?? 0) + 1);
    }
    for (const c of captures) this.setCapture(c);
  }

  // Incremental changes; returns whether there were any.
  applyCellUpdates(cells: readonly CellUpdate[]): boolean {
    for (const c of cells) {
      if (c.index < this.owners.length) {
        const old = this.owners[c.index]!;
        if (old !== c.owner) {
          if (old !== 0) this.ownedCount.set(old, (this.ownedCount.get(old) ?? 1) - 1);
          if (c.owner !== 0) this.ownedCount.set(c.owner, (this.ownedCount.get(c.owner) ?? 0) + 1);
          this.owners[c.index] = c.owner;
        }
      }
      this.setCapture(c);
    }
    return cells.length > 0;
  }

  owned(factionId: number): number {
    return this.ownedCount.get(factionId) ?? 0;
  }

  // A faction's cells and its share of the map (rounded percent).
  stats(factionId: number): { cells: number; percent: number } {
    const cells = this.owned(factionId);
    const total = this.cols * this.rows;
    return { cells, percent: total > 0 ? Math.round((cells / total) * 100) : 0 };
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
    this.captureFaction[c.index] = c.captureFaction;
    this.captureProgress[c.index] = Math.min(100, c.captureProgress);
  }
}
