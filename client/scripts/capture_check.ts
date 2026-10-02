// Pure checks for which cells a faction may capture as far as the client knows
// (GDD 7.3, GAME-017, GAME-021): the cells the client marks. The server decides.
// Run: npx tsx scripts/capture_check.ts
import { capitalZone, capturable, protectedCells, type KnownMap } from "../src/capture.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};
const same = (a: unknown, b: unknown) => JSON.stringify(a) === JSON.stringify(b);

// A 5x4 map, cell index = row * 5 + col; `owned` lists [index, faction] pairs.
const map = (owned: [number, number][] = []): KnownMap => {
  const owners = new Uint8Array(20);
  for (const [i, f] of owned) owners[i] = f;
  return { cols: 5, rows: 4, owners };
};
const none = new Uint8Array(20);
const can = (m: KnownMap, i: number, f: number, guarded: ArrayLike<number> = none) => capturable(m, guarded, i, f);

// --- Next to your land by a side -------------------------------------------------
{
  const m = map([[7, 1]]); // Red holds (2,1)
  check("a neutral cell next to yours by a side", can(m, 2, 1) && can(m, 6, 1) && can(m, 8, 1) && can(m, 12, 1));
  check("not one by a corner", !can(m, 1, 1) && !can(m, 3, 1) && !can(m, 11, 1) && !can(m, 13, 1));
  check("not one further away", !can(m, 9, 1) && !can(m, 17, 1));
  check("not your own", !can(m, 7, 1));
  check("not for a faction without land", !can(m, 6, 2));
  check("no faction (0): nothing", !can(m, 6, 0));
}
{
  const m = map([[7, 1], [8, 2]]); // Red (2,1), Blue (3,1)
  check("an enemy cell next to yours", can(m, 8, 1) && can(m, 7, 2));
}
{
  // A row's last cell and the next row's first are neighbours by index, not on the map.
  const m = map([[4, 1], [15, 2]]); // Red on the right edge (4,0), Blue on the left (0,3)
  check("not across the map's edge", !can(m, 5, 1) && !can(m, 14, 2));
  check("off the map: nothing", !can(m, -1, 1) && !can(m, 20, 1));
}

// --- A capital's zone is protected -------------------------------------------------
{
  check("a zone: the cells whose centres are within the radius", same(capitalZone({ cell: 7, protectedRadius: 1 }, 5, 4).sort((a, b) => a - b), [2, 6, 7, 8, 12]));
  check("radius 0: the capital's cell", same(capitalZone({ cell: 7, protectedRadius: 0 }, 5, 4), [7]));
  check("clipped by the map", same(capitalZone({ cell: 0, protectedRadius: 1 }, 5, 4).sort((a, b) => a - b), [0, 1, 5]));
  const guarded = protectedCells([{ cell: 7, protectedRadius: 1 }, { cell: 19, protectedRadius: 0 }], 5, 4);
  check("protected cells of every capital", guarded[2] === 1 && guarded[12] === 1 && guarded[19] === 1 && guarded[0] === 0 && guarded.length === 20);
  const m = map([[2, 2], [6, 2], [7, 2], [8, 2], [12, 2], [13, 1]]); // Blue's zone, Red at (3,2)
  check("an enemy zone's cell is not to be taken", !can(m, 12, 1, guarded) && !can(m, 8, 1, guarded));
  check("... the cells around it still are", can(m, 14, 1, guarded) && can(m, 18, 1, guarded));
  check("the zone's faction grows out of it", can(m, 1, 2, guarded) && can(m, 11, 2, guarded));
}

console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
