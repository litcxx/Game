// Unit check for the pure camera math (no rendering, no server).
//   run: npx tsx scripts/camera_check.ts
import { Camera, UNITS_PER_CELL } from "../src/render/camera.js";

let failures = 0;
function check(name: string, cond: boolean): void {
  if (cond) {
    console.log(`ok   ${name}`);
  } else {
    failures++;
    console.log(`FAIL ${name}`);
  }
}
const approx = (a: number, b: number, eps = 1e-6) => Math.abs(a - b) <= eps;

const SW = 1500;
const SH = 900;
const cam = new Camera();
cam.setScreen(SW, SH);
cam.setWorld(100, 100); // 100x100 cells -> 10000x10000 world units
cam.setMode("follow");
cam.setTarget(5000, 5000);

// follow: exactly 15 cells across the width
check("follow scale = width/(15*cell)", approx(cam.scale, SW / (15 * UNITS_PER_CELL)));
check("follow shows exactly 15 cols wide", approx(SW / cam.scale / UNITS_PER_CELL, 15));

// centred on the target
{
  const [cx, cy] = cam.worldToScreen(5000, 5000);
  check("follow centres on target", approx(cx, SW / 2) && approx(cy, SH / 2));
}

// clamps to the top-left corner
cam.setTarget(0, 0);
{
  const [x, y] = cam.worldToScreen(0, 0);
  check("follow clamps at (0,0)", approx(x, 0) && approx(y, 0));
}

// clamps to the bottom-right corner
cam.setTarget(10000, 10000);
{
  const [x, y] = cam.worldToScreen(10000, 10000);
  check("follow clamps at (worldW,worldH)", approx(x, SW) && approx(y, SH));
}

// screenToWorld is the inverse of worldToScreen
cam.setTarget(5000, 5000);
{
  const [sx, sy] = cam.worldToScreen(4321, 6789);
  const [wx, wy] = cam.screenToWorld(sx, sy);
  check("round-trip world<->screen", approx(wx, 4321, 1e-6) && approx(wy, 6789, 1e-6));
}

// map mode fits the whole world inside the viewport
cam.setMode("map");
check("map scale = min(sw/ww, sh/wh)", approx(cam.scale, Math.min(SW / 10000, SH / 10000)));
{
  const [x0, y0] = cam.worldToScreen(0, 0);
  const [x1, y1] = cam.worldToScreen(10000, 10000);
  check(
    "map fits whole world on screen",
    x0 >= -1e-6 && y0 >= -1e-6 && x1 <= SW + 1e-6 && y1 <= SH + 1e-6,
  );
}

console.log("VERDICT:", failures === 0 ? "PASS" : "FAIL");
process.exit(failures === 0 ? 0 : 1);
