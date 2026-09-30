import { CORRECTION_UNITS, integrate, Predictor, type Bounds } from "../src/net/prediction.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};
const approx = (a: number, b: number, eps = 1e-6) => Math.abs(a - b) <= eps;
const same = (a: unknown, b: unknown) => JSON.stringify(a) === JSON.stringify(b);

const bounds: Bounds = { maxX: 9999, maxY: 9999 }; // 100-cell map: 100*100 - 1
const speed = 300;
const dt = 1 / 60;

// integrate: move right 6 steps
let p = { x: 5050, y: 5050 };
for (let i = 0; i < 6; i++) p = integrate(p, 1, 0, dt, speed, bounds);
check("integrate advances 6 steps", approx(p.x, 5050 + 6 * speed * dt) && approx(p.y, 5050));

// integrate clamps at bounds
check("integrate clamps hi", integrate({ x: 9999, y: 0 }, 1, 0, dt, speed, bounds).x === 9999);
check("integrate clamps lo", integrate({ x: 0, y: 0 }, -1, 0, dt, speed, bounds).x === 0);

// predictor.step advances predicted and buffers with increasing seq
const pred = new Predictor(speed, dt, bounds);
pred.reset({ x: 100, y: 100 });
const f1 = pred.step({ moveX: 1, moveY: 0, capturing: false, attack: false });
const f2 = pred.step({ moveX: 1, moveY: 0, capturing: false, attack: false });
check("step seq increments", f1.seq === 1 && f2.seq === 2);
check("step advances predicted", approx(pred.position.x, 100 + 2 * speed * dt));

// reconcile: server acked seq 1, authoritative reflects 1 step -> replay f2 only
pred.reconcile({ x: 100 + speed * dt, y: 100 }, 1);
check("reconcile replays unacked", approx(pred.position.x, 100 + 2 * speed * dt));

// reconcile: all acked -> predicted == authoritative, no leftover replay
pred.reconcile({ x: 200, y: 100 }, 2);
check("reconcile full-ack snaps to authoritative", approx(pred.position.x, 200) && approx(pred.position.y, 100));

// divergence: renderPosition stays at the old displayed point, then eases to predicted
pred.reset({ x: 500, y: 500 });
const displayedBefore = pred.renderPosition.x; // 500
pred.reconcile({ x: 400, y: 500 }, 0);         // server says we are 100 to the left
check("renderPosition eases (no pop)", approx(pred.renderPosition.x, displayedBefore, 1e-6));
check("position is authoritative", approx(pred.position.x, 400));
for (let i = 0; i < 40; i++) pred.decayError(1 / 60);
check("error decays toward predicted", approx(pred.renderPosition.x, 400, 1.0));

// Corrections: how often, and how far, the server's word moved the prediction
// (reported to the server for the playtest metrics).
{
  const pr = new Predictor(speed, dt, bounds);
  pr.reset({ x: 1000, y: 1000 });
  check("no corrections yet", same(pr.takeCorrections(), { count: 0, max: 0 }));
  pr.reconcile({ x: 1000, y: 1000 }, 0); // the server agrees
  pr.reconcile({ x: 1000 + CORRECTION_UNITS, y: 1000 }, 0); // off by the threshold exactly
  check("agreement, or a drift up to the threshold, is no correction", same(pr.takeCorrections(), { count: 0, max: 0 }));
  pr.reconcile({ x: 1004 - 30, y: 1000 + 40 }, 0); // 50 units off
  pr.reconcile({ x: 974 + 10, y: 1040 }, 0); // 10 units off
  check("two corrections, the largest 50 units", same(pr.takeCorrections(), { count: 2, max: 50 }));
  check("taking them starts the count over", same(pr.takeCorrections(), { count: 0, max: 0 }));

  pr.reset({ x: 2000, y: 2000 });
  pr.step({ moveX: 1, moveY: 0, capturing: false, attack: false, ability: 0, aimX: 0, aimY: 0 });
  pr.reconcile({ x: 2000, y: 2000 }, 0); // the server has not seen the step yet: replayed
  check("an input the server has not applied yet is no correction", same(pr.takeCorrections(), { count: 0, max: 0 }));
  pr.reset({ x: 5000, y: 5000 });
  check("a reset (spawn, respawn) is no correction", same(pr.takeCorrections(), { count: 0, max: 0 }));
}

console.log("VERDICT:", failures === 0 ? "PASS" : "FAIL");
process.exit(failures === 0 ? 0 : 1);
