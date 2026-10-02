import type { Capital } from "./state/gameState.js";

// Which cells a faction may capture, as far as the client knows (GDD 7.3,
// GAME-017): the rules the server checks, run on the client's map, so that the
// cells worth trying are marked (GAME-021). The server decides.

// The territory as far as the client knows it (state/territory.ts).
export interface KnownMap {
  cols: number;
  rows: number;
  owners: ArrayLike<number>; // faction id per cell, row-major (0 = neutral or unknown)
}

// A capital's zone: the cells whose centres are within protectedRadius cells of
// the capital's centre — the server's capital_zone().
export function capitalZone(capital: Pick<Capital, "cell" | "protectedRadius">, cols: number, rows: number): number[] {
  const col = capital.cell % cols;
  const row = Math.floor(capital.cell / cols);
  const r = capital.protectedRadius;
  const zone: number[] = [];
  for (let y = Math.max(0, row - r); y <= Math.min(rows - 1, row + r); y++) {
    for (let x = Math.max(0, col - r); x <= Math.min(cols - 1, col + r); x++) {
      if ((x - col) ** 2 + (y - row) ** 2 <= r * r) zone.push(y * cols + x);
    }
  }
  return zone;
}

// A flag per cell: 1 = in a capital's zone, which no other faction can take.
export function protectedCells(capitals: readonly Pick<Capital, "cell" | "protectedRadius">[], cols: number, rows: number): Uint8Array {
  const guarded = new Uint8Array(cols * rows);
  for (const capital of capitals) for (const i of capitalZone(capital, cols, rows)) guarded[i] = 1;
  return guarded;
}

// Whether `faction` may capture cell `index`: not its own, not in a capital's
// zone, next to a cell of its own by a side (not by a corner, nor across a row's
// end). Faction 0 (none chosen) captures nothing.
export function capturable(map: KnownMap, protectedMask: ArrayLike<number>, index: number, faction: number): boolean {
  const { cols, rows, owners } = map;
  if (faction === 0 || index < 0 || index >= cols * rows) return false;
  if (owners[index] === faction || protectedMask[index] === 1) return false;
  const col = index % cols;
  const row = Math.floor(index / cols);
  const ours = (c: number, r: number) => c >= 0 && r >= 0 && c < cols && r < rows && owners[r * cols + c] === faction;
  return ours(col - 1, row) || ours(col + 1, row) || ours(col, row - 1) || ours(col, row + 1);
}
