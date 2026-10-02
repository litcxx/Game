// Pure checks for the borders of a set of cells (GAME-021): the lines the client
// draws around the cells you may capture, each border once.
// Run: npx tsx scripts/borders_check.ts
import { cellBorders } from "../src/render/borders.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};
// Order does not matter: compare as sorted lists.
const same = (a: number[][], b: number[][]) => JSON.stringify(a.map(String).sort()) === JSON.stringify(b.map(String).sort());
const count = (a: number[][], border: number[]) => a.filter((b) => String(b) === String(border)).length;

// A map 5 cells wide, cell index = row * 5 + col; a border is [x0, y0, x1, y1] in cells.
const COLS = 5;

check("none: no borders", cellBorders([], COLS).length === 0);

check(
  "one cell: its four sides",
  same(cellBorders([7], COLS), [
    [2, 1, 3, 1], // top
    [2, 2, 3, 2], // bottom
    [2, 1, 2, 2], // left
    [3, 1, 3, 2], // right
  ]),
);

{
  const side = cellBorders([7, 8], COLS); // (2,1) and (3,1)
  check("two side by side: seven borders", side.length === 7);
  check("... the one between them once", count(side, [3, 1, 3, 2]) === 1);
}
{
  const stacked = cellBorders([7, 12], COLS); // (2,1) and (2,2)
  check("one above the other: seven borders", stacked.length === 7);
  check("... the one between them once", count(stacked, [2, 2, 3, 2]) === 1);
}
check("a 2x2 block: twelve borders (eight outside, four inside)", cellBorders([7, 8, 12, 13], COLS).length === 12);
check("corner to corner: nothing shared", cellBorders([7, 13], COLS).length === 8);

{
  const ends = cellBorders([9, 10], COLS); // (4,1), the row's last cell, and (0,2), the next row's first
  check("no wrap between rows: nothing shared", ends.length === 8);
  check("... the map's right edge", count(ends, [5, 1, 5, 2]) === 1);
  check("... and its left edge", count(ends, [0, 2, 0, 3]) === 1);
}

console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
