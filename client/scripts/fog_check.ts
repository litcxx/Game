// Pure checks for the fog of war model: cell sight from the snapshot's
// revealed / hidden deltas, the memory of explored cells, and the per-row runs
// the renderer draws. Run: npx tsx scripts/fog_check.ts
import { FogOfWar } from "../src/fog.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};
const same = (a: unknown, b: unknown) => JSON.stringify(a) === JSON.stringify(b);

// A 4x3 map: cell index = row * 4 + col.
const fresh = () => {
  const fog = new FogOfWar();
  fog.reset(4, 3);
  return fog;
};

{
  const fog = fresh();
  check("a fresh map is all unexplored", [...Array(12).keys()].every((i) => fog.sightAt(i) === "unexplored"));
}
{
  const fog = fresh();
  fog.apply([1, 5], []);
  check("revealed cells are visible", fog.sightAt(1) === "visible" && fog.sightAt(5) === "visible");
  check("the others stay unexplored", fog.sightAt(0) === "unexplored");
}
{
  const fog = fresh();
  fog.apply([1, 5], []);
  fog.apply([], [5]);
  check("a hidden cell is remembered as explored", fog.sightAt(5) === "explored");
  check("a cell still in sight stays visible", fog.sightAt(1) === "visible");
  fog.apply([2], [1]);
  check("explored cells are never forgotten", fog.sightAt(5) === "explored" && fog.sightAt(1) === "explored");
  fog.apply([5], []);
  check("an explored cell can be seen again", fog.sightAt(5) === "visible");
}
{
  const fog = fresh();
  check("a change is reported", fog.apply([3], []));
  check("nothing new: no change", !fog.apply([3], []) && !fog.apply([], []));
  check("hiding a visible cell is a change", fog.apply([], [3]));
}
{
  const fog = fresh();
  fog.apply([99], [-1]);
  check("indices off the map are ignored", fog.sightAt(99) === "unexplored");
  check("off the map reads as unexplored", fog.sightAt(-1) === "unexplored");
}
{
  const fog = fresh();
  fog.apply([1, 2, 5], []);
  fog.apply([], [2]);
  check(
    "a row splits into runs of equal sight",
    same(fog.runs(0, 0, 3), [
      { col: 0, length: 1, sight: "unexplored" },
      { col: 1, length: 1, sight: "visible" },
      { col: 2, length: 1, sight: "explored" },
      { col: 3, length: 1, sight: "unexplored" },
    ]),
  );
  check("equal neighbours merge into one run", same(fog.runs(2, 0, 3), [{ col: 0, length: 4, sight: "unexplored" }]));
  check("runs cover only the asked columns", same(fog.runs(1, 1, 2), [
    { col: 1, length: 1, sight: "visible" },
    { col: 2, length: 1, sight: "unexplored" },
  ]));
}
{
  // Snapshot.resync: a frame was lost, so what we hold as visible may be stale.
  const fog = fresh();
  fog.apply([1, 2], []);
  fog.apply([], [2]);
  check("a resync turns every visible cell to explored", fog.forgetSight() && fog.sightAt(1) === "explored");
  check("a resync keeps explored cells explored", fog.sightAt(2) === "explored");
  check("a resync leaves unexplored cells unexplored", fog.sightAt(0) === "unexplored");
  check("nothing visible: a resync changes nothing", !fog.forgetSight());
  fog.apply([1], []);
  check("the resync's revealed cells are visible again", fog.sightAt(1) === "visible");
}
{
  const fog = fresh();
  fog.apply([0], []);
  fog.reset(2, 2);
  check("reset starts a new map, all unexplored", fog.sightAt(0) === "unexplored");
}
{
  // MapState.explored: the faction's explored cells, a bit per cell, lowest bit first.
  const fog = fresh();
  fog.apply([2], []);
  const changed = fog.explore(new Uint8Array([0b0000_0101, 0b0000_0010])); // cells 0, 2, 9
  check("explored bits mark their cells explored", changed && fog.sightAt(0) === "explored" && fog.sightAt(9) === "explored");
  check("... the rest stays unexplored", fog.sightAt(1) === "unexplored" && fog.sightAt(8) === "unexplored");
  check("... a visible cell stays visible", fog.sightAt(2) === "visible");
  check("nothing new: no change", !fog.explore(new Uint8Array([0b0000_0101, 0b0000_0010])));
  check("no bits: nothing explored, no change", !fresh().explore(new Uint8Array(0)));
  const wide = fresh();
  wide.explore(new Uint8Array([0, 0xff])); // bits 8..15: cells 12..15 are off the 4x3 map
  check("bits past the map are ignored", wide.sightAt(11) === "explored" && wide.sightAt(12) === "unexplored");
}

console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
