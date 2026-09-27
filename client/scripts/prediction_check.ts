import { integrate, Predictor, type Bounds } from "../src/net/prediction.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};
const approx = (a: number, b: number, eps = 1e-6) => Math.abs(a - b) <= eps;

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

console.log("VERDICT:", failures === 0 ? "PASS" : "FAIL");
process.exit(failures === 0 ? 0 : 1);
