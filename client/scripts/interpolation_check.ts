import { InterpolationBuffer, type RemoteState } from "../src/net/interpolation.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};
const approx = (a: number, b: number, eps = 1e-6) => Math.abs(a - b) <= eps;
const m = (x: number, y: number): Map<number, RemoteState> => new Map([[1, { x, y }]]);

const buf = new InterpolationBuffer(100); // render 100ms behind
buf.push(0, m(0, 0));
buf.push(100, m(100, 0));

// nowMs=150 -> renderTime=50 -> halfway between the two snapshots
const mid = buf.sample(1, 150);
check("interpolates midpoint", mid !== undefined && approx(mid.x, 50));

// nowMs far ahead -> renderTime past newest -> clamp to newest
const newest = buf.sample(1, 100000);
check("clamps to newest when starved", newest !== undefined && approx(newest.x, 100));

// unknown id -> undefined
check("unknown id -> undefined", buf.sample(99, 150) === undefined);

// id present only in the newer snapshot (joined mid-window) -> uses newer
const buf2 = new InterpolationBuffer(100);
buf2.push(0, new Map());   // empty
buf2.push(100, m(100, 0)); // id 1 appears
const joined = buf2.sample(1, 150);
check("id appearing mid-window resolves", joined !== undefined && approx(joined.x, 100));

console.log("VERDICT:", failures === 0 ? "PASS" : "FAIL");
process.exit(failures === 0 ? 0 : 1);
