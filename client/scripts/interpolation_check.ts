import { InterpolationBuffer, type RemoteState } from "../src/net/interpolation.js";
import { CREEP_MS_PER_S, MAX_LAG_MS, SnapshotClock } from "../src/net/snapshotClock.js";

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

// ids(): what is on screen at render time — both snapshots around it, so a
// projectile gone from the newest snapshot (it hit) is still drawn until the
// render time passes its last known position.
const ids = (b: InterpolationBuffer, now: number) => [...b.ids(now)].sort((a, c) => a - c).join(",");
const buf3 = new InterpolationBuffer(100);
buf3.push(1000, new Map([[1, { x: 0, y: 0 }], [2, { x: 0, y: 0 }]]));
buf3.push(1050, new Map([[2, { x: 0, y: 0 }], [3, { x: 0, y: 0 }]]));
check("between snapshots: ids of both", ids(buf3, 1125) === "1,2,3");
check("past the newest: its ids only", ids(buf3, 1200) === "2,3");
check("before the oldest: its ids only", ids(buf3, 1050) === "1,2");
check("empty buffer: no ids", ids(new InterpolationBuffer(100), 500) === "");

// SnapshotClock: a snapshot's place on the local clock is its server time plus the
// smallest arrival-minus-server offset seen (the fastest delivery), so network
// jitter doesn't reach the interpolation; the offset creeps up slowly to follow a
// latency that grows for good, and never lets a stamp lag its arrival by more than
// MAX_LAG_MS.
{
  const clock = new SnapshotClock();
  check("the first snapshot: at its arrival", approx(clock.stamp(1000, 1040), 1040));
  check("a faster one lowers the offset at once", approx(clock.stamp(1050, 1080), 1080));
  const slow = clock.stamp(1100, 1170); // 90 ms after the last: +0.09 ms creep, not its 40 ms
  check("a slower one moves it only by the creep", approx(slow, 1100 + 30 + 0.09 * CREEP_MS_PER_S));
  check("... so a late snapshot is placed before its arrival", slow < 1170);
  clock.reset();
  check("reset: the next snapshot sets the offset anew", approx(clock.stamp(5000, 5300), 5300));
}
{
  const clock = new SnapshotClock();
  const first = clock.stamp(1000, 1100); // offset 100
  const second = clock.stamp(1050, 1060); // the latency dropped by 90: offset 10
  check("stamps never go back, even when the latency drops", second > first);
}
{
  // The latency grew for good (a route changed) by more than the interpolation
  // delay covers: the creep would take ~100 s to follow, the render time running
  // past the newest snapshot — remote players freezing and jumping all along.
  const clock = new SnapshotClock();
  for (let i = 0; i < 20; i++) clock.stamp(i * 50, i * 50 + 20); // offset 20
  let worst = 0;
  for (let i = 20; i < 40; i++) worst = Math.max(worst, i * 50 + 120 - clock.stamp(i * 50, i * 50 + 120));
  check("a latency grown for good: no stamp lags its arrival by more than MAX_LAG_MS", worst <= MAX_LAG_MS + 1e-6);
  // At the new latency, jitter of ±15 ms is absorbed again (a bound that tight
  // would hand it on to the motion).
  const stamps: number[] = [];
  for (let i = 40; i < 80; i++) stamps.push(clock.stamp(i * 50, i * 50 + 120 + (i % 2 ? 15 : -15)));
  const steps = stamps.slice(2).map((t, k) => t - stamps[k + 1]!); // after the first late one settles it
  check("... and jitter at the new latency still moves remote players evenly", Math.max(...steps) - Math.min(...steps) < 0.1);
}

console.log("VERDICT:", failures === 0 ? "PASS" : "FAIL");
process.exit(failures === 0 ? 0 : 1);
