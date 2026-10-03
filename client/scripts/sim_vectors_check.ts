// The client's copies of the server's formulas, checked to the bit against the
// cases the server writes (protocol/sim/*.json, the server's SimVectors tests):
// a step of movement (Predictor's integrate). A shot's velocity and flight are
// in projectile.json for the client's own (LTC-88).
// Run: npx tsx scripts/sim_vectors_check.ts
import { readFileSync } from "node:fs";

import { integrate } from "../src/net/prediction.js";

let failures = 0;
const check = (name: string, cond: boolean, detail = "") => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}${detail ? `  [${detail}]` : ""}`);
  if (!cond) failures++;
};

interface MovementCase {
  name: string;
  x: number;
  y: number;
  move_x: number;
  move_y: number;
  speed: number;
  dt: number;
  max_x: number;
  max_y: number;
  steps: number;
  after: { x: number; y: number };
}
const movement = JSON.parse(readFileSync(new URL("../../protocol/sim/movement.json", import.meta.url), "utf8")) as {
  cases: MovementCase[];
};

check("movement: there are cases", movement.cases.length > 0);
for (const c of movement.cases) {
  let pos = { x: c.x, y: c.y };
  for (let i = 0; i < c.steps; i++) pos = integrate(pos, c.move_x, c.move_y, c.dt, c.speed, { maxX: c.max_x, maxY: c.max_y });
  check(`movement: ${c.name}`, pos.x === c.after.x && pos.y === c.after.y, `got ${pos.x}, ${pos.y}; server ${c.after.x}, ${c.after.y}`);
}

console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
