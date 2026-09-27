# Client Prediction + Fixed Timestep Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make movement feel smooth — the local player and camera move at render framerate with no input lag, remote players interpolate smoothly, and the server stays authoritative and deterministic.

**Architecture:** Server switches to a fixed timestep and consumes one input command per tick from a per-player queue (echoing `last_input_seq`). The client generates one input frame per fixed step, predicts its own movement locally, and reconciles against snapshots by dropping acked frames and replaying the rest. Remote players are interpolated ~100 ms in the past. The shared movement integration is byte-for-byte identical on both sides.

**Tech Stack:** C++23 / Boost.Asio (server), GoogleTest; TypeScript / PixiJS / protobuf-es (client), tsx for headless checks.

**Spec:** `docs/superpowers/specs/2026-09-27-client-prediction-design.md`

## Global Constraints

- Protocol is **unchanged** (`Input.frames` is already `repeated`, ≤ 8 per message; `InputFrame.seq`; `SelfState.last_input_seq`).
- Shared integration must match on both sides **exactly**: `if move==(0,0) keep pos; len=hypot(mx,my); pos += (m/len)*move_speed*dt; clamp each axis to [0, map*100 - 1]`.
- `fixedDt = 1 / tick_rate` (tick_rate = 60). Determinism depends on identical dt.
- Predict **movement only**; combat and capture stay server-authoritative (rendered from snapshots).
- `tick_rate=60`, `snapshot_rate=20` unchanged. `INTERP_DELAY ≈ 100 ms`. `MAX_ACCUM ≈ 0.25 s`.
- Follow existing patterns: server tests in `test/unit/game/world/world_tests.cpp`; client pure-logic checks are standalone `tsx` scripts under `client/scripts/` printing `VERDICT: PASS/FAIL` and `process.exit(0/1)`.
- The user commits; each task's final step proposes a commit but the human runs it.

## Review Focus

- **Input batch > 8 frames on a client hitch** (Task 6): the spec caps `Input.frames` at 8; a long stall can produce more pending frames than fit one message. Expected: split into ≤ 8-frame messages, never silently drop unsent inputs. (Build-only task; verify by code inspection.)
- **Correction pop on divergence** (Task 6): when replay diverges from the server (wall clamp, teleport), the local sprite must ease to the corrected position over ~100 ms, not snap — except on spawn/respawn/death, which snap. (Visual.)
- **Interpolation when a remote leaves mid-window** (Task 5): an id present in the older snapshot but absent from the newer must not throw or freeze a ghost; `sample` returns undefined and the sprite is removed. Add a test.
- **Respawn seq continuity** (Task 1): the client's `seq` keeps increasing across respawn; the server must keep enqueuing (it clears the queue on spawn but does not reset `last_enqueued_seq`), so post-respawn frames still enqueue. Add a test that enqueues after a spawn-clear.
- **Backgrounded tab / huge frame dt** (Task 6): the client fixed-step accumulator must clamp like the server (`MAX_ACCUM`) so returning to a tab doesn't dump hundreds of frames. (Code inspection; mirrors `fixed_steps`.)

---

### Task 1: Server — per-tick input queue + `consume_inputs`

**Files:**
- Modify: `server/src/game/world/world.hpp` (`Player`: add `InputCommand` queue + `last_enqueued_seq`; declare `consume_inputs`)
- Modify: `server/src/game/world/world.cpp` (`on_input` enqueues; add `consume_inputs`; `on_spawn` clears queue; `tick` order)
- Test: `server/test/unit/game/world/world_tests.cpp`

**Interfaces:**
- Produces: `Player` holds `std::deque<InputCommand> inputs;` and `std::uint32_t last_enqueued_seq{0};`. `consume_inputs()` pops one command per player per tick, sets intent + `last_input_seq`, repeats last when the queue is empty. `on_input` enqueues frames with `seq > last_enqueued_seq`.

- [ ] **Step 1: Write the failing tests**

Add to `world_tests.cpp` (after the combat tests):

```cpp
// --- Fixed-step input model -------------------------------------------------
TEST(WorldInput, FixedStepMovementIsDeterministic) {
  lit::TSQueue<lit::ClientEvent> incoming;
  lit::test::MockClientGateway gw;
  auto config = test_config();
  lit::game::World world(incoming, gw, config);

  incoming.push(hello_event(7, "p"));
  incoming.push(spawn_event(7, /*cell=*/5, /*faction=*/1));  // center (150,150)
  incoming.push(input_event(7, /*move_x=*/1, /*move_y=*/0, /*seq=*/1));  // then repeat-last
  const double dt = 1.0 / config.tick_rate;
  run_ticks(world, 6, dt);  // tick1 consumes cmd, ticks 2..6 repeat it

  auto snap = last_snapshot_to(gw, 7);
  ASSERT_TRUE(snap.has_value());
  ASSERT_EQ(snap->players_size(), 1);
  const double expected = 150.0 + 6 * config.move_speed * dt;  // 6 ticks of +speed*dt
  EXPECT_NEAR(static_cast<double>(snap->players(0).x()), expected, 1.0);
  EXPECT_EQ(snap->players(0).y(), 150u);
}

TEST(WorldInput, ConsumesOneCommandPerTick) {
  lit::TSQueue<lit::ClientEvent> incoming;
  lit::test::MockClientGateway gw;
  auto config = test_config();
  config.snapshot_rate = config.tick_rate;  // one snapshot per tick, to read per-tick acks
  lit::game::World world(incoming, gw, config);

  incoming.push(hello_event(7, "p"));
  incoming.push(spawn_event(7, /*cell=*/5, /*faction=*/1));
  incoming.push(input_event(7, 0, 0, /*seq=*/1));
  incoming.push(input_event(7, 0, 0, /*seq=*/2));
  const double dt = 1.0 / config.tick_rate;

  run_ticks(world, 1, dt);
  EXPECT_EQ(last_snapshot_to(gw, 7)->you().last_input_seq(), 1u);  // consumed seq 1
  run_ticks(world, 1, dt);
  EXPECT_EQ(last_snapshot_to(gw, 7)->you().last_input_seq(), 2u);  // consumed seq 2
  run_ticks(world, 1, dt);
  EXPECT_EQ(last_snapshot_to(gw, 7)->you().last_input_seq(), 2u);  // empty queue -> repeat
}

TEST(WorldInput, EnqueuesAfterRespawnClear) {
  lit::TSQueue<lit::ClientEvent> incoming;
  lit::test::MockClientGateway gw;
  auto config = test_config();
  config.snapshot_rate = config.tick_rate;
  lit::game::World world(incoming, gw, config);

  incoming.push(hello_event(7, "p"));
  incoming.push(spawn_event(7, /*cell=*/5, /*faction=*/1));  // clears any queued input
  incoming.push(input_event(7, 1, 0, /*seq=*/10));           // seq keeps climbing
  const double dt = 1.0 / config.tick_rate;
  run_ticks(world, 1, dt);
  EXPECT_EQ(last_snapshot_to(gw, 7)->you().last_input_seq(), 10u);  // still enqueued & consumed
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd server && cmake --build build --target unit_tests -j4 && ./build/bin/unit_tests --gtest_filter='WorldInput.*'`
Expected: FAIL (e.g., `ConsumesOneCommandPerTick` sees `last_input_seq` jump to 2 immediately because the old `on_input` sets the latest frame directly).

- [ ] **Step 3: Add the input queue to `Player` and declare `consume_inputs`**

In `world.hpp`, add `#include <deque>` near the other includes, then above `struct Player` add:

```cpp
// One tick's worth of player intent, consumed one-per-tick for deterministic replay.
struct InputCommand {
    std::uint32_t seq{0};
    std::int32_t move_x{0};
    std::int32_t move_y{0};
    bool capturing{false};
    bool attack{false};
};
```

Inside `struct Player`, after `respawn_tick`, add:

```cpp
    std::deque<InputCommand> inputs;      // pending per-tick commands (FIFO by seq)
    std::uint32_t last_enqueued_seq{0};   // highest seq accepted into the queue
```

In the private methods of `World`, declare next to `update`:

```cpp
    // Pop one queued command per player (repeat last when empty); sets intent + last_input_seq.
    void consume_inputs();
```

- [ ] **Step 4: Rewrite `on_input` to enqueue, add `consume_inputs`, clear on spawn, reorder tick**

In `world.cpp`, replace the body of `on_input` with:

```cpp
void World::on_input(std::uint64_t session_id, const ::game::v1::Input& input) {
    auto it = players_.find(session_id);
    if (it == players_.end()) {
        return;
    }
    Player& player = it->second;
    for (const auto& frame : input.frames()) {
        if (frame.seq() <= player.last_enqueued_seq) continue;  // out-of-order / duplicate
        player.last_enqueued_seq = frame.seq();
        player.inputs.push_back(InputCommand{frame.seq(), frame.move_x(), frame.move_y(),
                                             frame.capturing(), frame.attack()});
    }
}
```

Add `consume_inputs` (next to `update`):

```cpp
void World::consume_inputs() {
    for (auto& [session_id, p] : players_) {
        if (p.inputs.empty()) {
            continue;  // no fresh command this tick -> repeat last intent (fields unchanged)
        }
        const InputCommand cmd = p.inputs.front();
        p.inputs.pop_front();
        p.move_x = cmd.move_x;
        p.move_y = cmd.move_y;
        p.capturing = cmd.capturing;
        p.attack = cmd.attack;
        p.last_input_seq = cmd.seq;
    }
}
```

In `on_spawn`, where intent is reset (the block setting `player.capturing = false; player.attack = false; ...`), also drop stale queued commands:

```cpp
    player.inputs.clear();  // pre-spawn commands are stale; keep last_enqueued_seq monotonic
```

In `tick`, insert `consume_inputs()` between draining events and `update(dt)`:

```cpp
    consume_inputs();
    update(dt);
    update_combat();
    update_captures();
    send_snapshots();
```

- [ ] **Step 5: Run the whole suite to verify pass + no regressions**

Run: `cd server && cmake --build build --target unit_tests -j4 && ./build/bin/unit_tests`
Expected: PASS — the 3 new `WorldInput` tests plus all existing suites (movement/combat/capture rely on repeat-last, which preserves their behavior).

- [ ] **Step 6: Commit**

```bash
git add server/src/game/world/world.hpp server/src/game/world/world.cpp server/test/unit/game/world/world_tests.cpp
git commit -m "feat(server): per-tick input queue for deterministic replay"
```

---

### Task 2: Server — fixed-timestep run loop

**Files:**
- Modify: `server/src/game/world/world.hpp` (add `inline int fixed_steps(...)`)
- Modify: `server/src/game/world/world.cpp` (`run` uses the accumulator)
- Test: `server/test/unit/game/world/world_tests.cpp`

**Interfaces:**
- Produces: `inline int fixed_steps(double& accumulator, double frame, double fixed_dt, double max_accum)` — adds `frame`, clamps to `max_accum`, returns how many `fixed_dt` steps to run now, leaves the remainder in `accumulator`.

- [ ] **Step 1: Write the failing tests**

Add to `world_tests.cpp`:

```cpp
TEST(FixedStep, RunsOneStepPerInterval) {
  double acc = 0.0;
  EXPECT_EQ(lit::game::fixed_steps(acc, 1.0 / 60, 1.0 / 60, 0.25), 1);
  EXPECT_NEAR(acc, 0.0, 1e-9);
}

TEST(FixedStep, AccumulatesRemainder) {
  double acc = 0.0;
  const double f = 1.0 / 60;
  EXPECT_EQ(lit::game::fixed_steps(acc, 2.5 * f, f, 0.25), 2);
  EXPECT_NEAR(acc, 0.5 * f, 1e-9);
}

TEST(FixedStep, ClampsSpiralOfDeath) {
  double acc = 0.0;
  const double f = 1.0 / 60;
  EXPECT_EQ(lit::game::fixed_steps(acc, 10.0, f, 0.25), 15);  // 0.25s / (1/60) = 15
  EXPECT_LT(acc, f);
}
```

- [ ] **Step 2: Run to verify failure**

Run: `cd server && cmake --build build --target unit_tests -j4`
Expected: FAIL to compile — `fixed_steps` is not declared.

- [ ] **Step 3: Add `fixed_steps` to `world.hpp`**

In `world.hpp`, inside `namespace lit::game` (above `struct InputCommand` is fine):

```cpp
// Fixed-timestep accumulator: add `frame` seconds, clamp to `max_accum` (avoid a
// spiral of death after a stall), and return how many `fixed_dt` steps to run now.
inline int fixed_steps(double& accumulator, double frame, double fixed_dt, double max_accum) {
    accumulator += frame;
    if (accumulator > max_accum) accumulator = max_accum;
    int steps = 0;
    while (accumulator >= fixed_dt) {
        accumulator -= fixed_dt;
        ++steps;
    }
    return steps;
}
```

- [ ] **Step 4: Use it in `run`**

In `world.cpp`, replace the body of `World::run` with:

```cpp
void World::run(std::stop_token stop) {
    const double fixed_dt = 1.0 / config_.tick_rate;
    const double max_accum = 0.25;
    const auto step_duration = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(fixed_dt));

    double accumulator = 0.0;
    auto last = std::chrono::steady_clock::now();
    while (!stop.stop_requested()) {
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> frame = now - last;
        last = now;

        const int steps = fixed_steps(accumulator, frame.count(), fixed_dt, max_accum);
        for (int i = 0; i < steps && !stop.stop_requested(); ++i) {
            tick(fixed_dt);  // always a fixed step -> deterministic simulation
        }
        std::this_thread::sleep_until(now + step_duration);
    }
}
```

- [ ] **Step 5: Run tests + build the server binary**

Run: `cd server && cmake --build build -j4 && ./build/bin/unit_tests --gtest_filter='FixedStep.*'`
Expected: PASS (3 tests). Full `./build/bin/unit_tests` still green.

- [ ] **Step 6: Commit**

```bash
git add server/src/game/world/world.hpp server/src/game/world/world.cpp server/test/unit/game/world/world_tests.cpp
git commit -m "feat(server): fixed-timestep simulation loop"
```

---

### Task 3: e2e smoke — per-tick frames → authoritative movement + acks

**Files:**
- Create: `client/scripts/prediction_smoke.ts`

**Interfaces:**
- Consumes: server tasks 1–2 running (`./server config/config.json`).

- [ ] **Step 1: Write the smoke**

Create `client/scripts/prediction_smoke.ts`:

```ts
// e2e: send per-tick input frames (6x move-right + 1x stop) and confirm the
// server applies exactly one per tick (position advances ~6 ticks) and acks
// via SelfState.last_input_seq.
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import {
  ClientMessageSchema,
  ServerMessageSchema,
  type ClientMessage,
} from "../src/gen/game/v1/protocol_pb.js";

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const ws = new WebSocket(URL);
ws.binaryType = "arraybuffer";

const spawnCell = 50 * 100 + 50; // center (5050, 5050)
let moveSpeed = 300;
let tickRate = 60;
let startX = 5050;
let lastX = -1;
let lastSeq = 0;

function send(msg: ClientMessage): void {
  ws.send(toBinary(ClientMessageSchema, msg));
}

ws.onopen = () =>
  send(create(ClientMessageSchema, { payload: { case: "hello", value: { protocolVersion: 1, name: "pred" } } }));

ws.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    moveSpeed = m.payload.value.config?.moveSpeed ?? 300;
    tickRate = m.payload.value.config?.tickRate ?? 60;
    const faction = m.payload.value.factions[0]?.id ?? 1;
    send(create(ClientMessageSchema, { payload: { case: "spawn", value: { cell: spawnCell, factionId: faction } } }));
    // one Input message, 7 frames (<=8): 6 move-right then stop
    const frames = [];
    for (let i = 1; i <= 6; i++) frames.push({ seq: i, moveX: 1, moveY: 0, capturing: false, attack: false });
    frames.push({ seq: 7, moveX: 0, moveY: 0, capturing: false, attack: false });
    send(create(ClientMessageSchema, { payload: { case: "input", value: { frames } } }));
  } else if (m.payload.case === "snapshot") {
    lastSeq = m.payload.value.you?.lastInputSeq ?? lastSeq;
    const me = m.payload.value.players.find((p) => p.x !== undefined);
    if (me) lastX = me.x;
  }
};
ws.onerror = () => console.error("[pred] socket error");

setTimeout(() => {
  const expectedDx = 6 * moveSpeed * (1 / tickRate); // 6 ticks moved before the stop
  const dx = lastX - startX;
  console.log(`[pred] lastSeq=${lastSeq} startX=${startX} lastX=${lastX} dx=${dx} expected≈${expectedDx}`);
  const pass = lastSeq === 7 && Math.abs(dx - expectedDx) <= moveSpeed * (1 / tickRate) + 1;
  console.log("VERDICT:", pass ? "PASS" : "FAIL");
  ws.close();
  process.exit(pass ? 0 : 1);
}, 1500);
```

- [ ] **Step 2: Run against a fresh server, verify it fails on the OLD server if run before Tasks 1–2**

(If Tasks 1–2 are already merged, this passes; the point of the step is to confirm the contract.) Start the server, then:

Run: `cd server && ASAN_OPTIONS=detect_container_overflow=0 ./build/bin/server config/config.json & sleep 1; cd ../client && npx tsx scripts/prediction_smoke.ts`
Expected (with Tasks 1–2): `VERDICT: PASS` (`lastSeq=7`, `dx≈30`).

- [ ] **Step 3: Stop the server**

Run: `kill %1` (or the server PID).

- [ ] **Step 4: Commit**

```bash
git add client/scripts/prediction_smoke.ts
git commit -m "test(client): e2e smoke for per-tick input + last_input_seq acks"
```

---

### Task 4: Client — predictor module (pure)

**Files:**
- Create: `client/src/net/prediction.ts`
- Create: `client/scripts/prediction_check.ts`

**Interfaces:**
- Produces: `integrate(pos, moveX, moveY, dt, speed, bounds): Vec2`; `class Predictor` with `reset(pos)`, `get position`, `step(sample): PendingInput`, `reconcile(authoritative, lastInputSeq)`. Types `Vec2 {x,y}`, `Bounds {maxX,maxY}`, `InputSample {moveX,moveY,capturing,attack}`, `PendingInput extends InputSample {seq}`.

- [ ] **Step 1: Write the module**

Create `client/src/net/prediction.ts`:

```ts
export interface Vec2 {
  x: number;
  y: number;
}
export interface Bounds {
  maxX: number;
  maxY: number;
}
export interface InputSample {
  moveX: number;
  moveY: number;
  capturing: boolean;
  attack: boolean;
}
export interface PendingInput extends InputSample {
  seq: number;
}

// MUST match the server's World::update integration exactly.
export function integrate(
  pos: Vec2,
  moveX: number,
  moveY: number,
  dt: number,
  speed: number,
  bounds: Bounds,
): Vec2 {
  if (moveX === 0 && moveY === 0) return pos;
  const len = Math.hypot(moveX, moveY);
  return {
    x: Math.max(0, Math.min(pos.x + (moveX / len) * speed * dt, bounds.maxX)),
    y: Math.max(0, Math.min(pos.y + (moveY / len) * speed * dt, bounds.maxY)),
  };
}

// Predicts the local player and reconciles against authoritative snapshots.
// `renderPosition` = predicted + a correction error that decays to zero, so a
// divergence eases in over ~100 ms instead of popping.
export class Predictor {
  private predicted: Vec2 = { x: 0, y: 0 };
  private error: Vec2 = { x: 0, y: 0 };
  private pending: PendingInput[] = [];
  private nextSeq = 1;

  constructor(
    private readonly speed: number,
    private readonly fixedDt: number,
    private readonly bounds: Bounds,
  ) {}

  // Hard snap (spawn / respawn / death / teleport): no easing.
  reset(pos: Vec2): void {
    this.predicted = { x: pos.x, y: pos.y };
    this.error = { x: 0, y: 0 };
    this.pending = [];
  }

  get position(): Vec2 {
    return this.predicted;
  }

  // What to draw: predicted plus the (decaying) correction error.
  get renderPosition(): Vec2 {
    return { x: this.predicted.x + this.error.x, y: this.predicted.y + this.error.y };
  }

  // Apply one fixed-step input locally; returns the frame to send.
  step(sample: InputSample): PendingInput {
    const frame: PendingInput = { seq: this.nextSeq++, ...sample };
    this.predicted = integrate(this.predicted, sample.moveX, sample.moveY, this.fixedDt, this.speed, this.bounds);
    this.pending.push(frame);
    return frame;
  }

  // Drop acked inputs, replay the remainder from the authoritative pos, and keep
  // the currently-displayed point continuous by folding the delta into `error`.
  reconcile(authoritative: Vec2, lastInputSeq: number): void {
    const displayed = this.renderPosition;
    this.pending = this.pending.filter((f) => f.seq > lastInputSeq);
    let pos: Vec2 = { x: authoritative.x, y: authoritative.y };
    for (const f of this.pending) {
      pos = integrate(pos, f.moveX, f.moveY, this.fixedDt, this.speed, this.bounds);
    }
    this.predicted = pos;
    this.error = { x: displayed.x - pos.x, y: displayed.y - pos.y };
  }

  // Ease the correction error toward zero (tau ≈ 80 ms). Call once per frame.
  decayError(dtSeconds: number): void {
    const a = Math.exp(-dtSeconds / 0.08);
    this.error = { x: this.error.x * a, y: this.error.y * a };
  }
}
```

- [ ] **Step 2: Write the failing check**

Create `client/scripts/prediction_check.ts`:

```ts
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
```

- [ ] **Step 3: Run to verify it passes**

Run: `cd client && npx tsc --noEmit && npx tsx scripts/prediction_check.ts`
Expected: `VERDICT: PASS` (5 checks) and clean typecheck.

- [ ] **Step 4: Commit**

```bash
git add client/src/net/prediction.ts client/scripts/prediction_check.ts
git commit -m "feat(client): movement predictor + reconciliation (pure)"
```

---

### Task 5: Client — interpolation buffer (pure)

**Files:**
- Create: `client/src/net/interpolation.ts`
- Create: `client/scripts/interpolation_check.ts`

**Interfaces:**
- Produces: `class InterpolationBuffer` with `push(timeMs, states: Map<number, RemoteState>)` and `sample(id, nowMs): RemoteState | undefined`. `RemoteState {x,y}`.

- [ ] **Step 1: Write the module**

Create `client/src/net/interpolation.ts`:

```ts
export interface RemoteState {
  x: number;
  y: number;
}

interface Snap {
  time: number;
  states: Map<number, RemoteState>;
}

// Buffers recent snapshots and returns positions interpolated `delayMs` in the past.
export class InterpolationBuffer {
  private snaps: Snap[] = [];

  constructor(
    private readonly delayMs: number,
    private readonly keepMs = 1000,
  ) {}

  push(timeMs: number, states: Map<number, RemoteState>): void {
    this.snaps.push({ time: timeMs, states });
    const cutoff = timeMs - this.keepMs;
    while (this.snaps.length > 2 && this.snaps[0]!.time < cutoff) this.snaps.shift();
  }

  // Position of `id` at renderTime = nowMs - delayMs; undefined if unknown.
  sample(id: number, nowMs: number): RemoteState | undefined {
    const t = nowMs - this.delayMs;
    if (this.snaps.length === 0) return undefined;

    let s0: Snap | undefined;
    let s1: Snap | undefined;
    for (const s of this.snaps) {
      if (s.time <= t) s0 = s;
      if (s.time >= t && s1 === undefined) s1 = s;
    }
    if (s0 && s1 && s0 !== s1) {
      const a = s0.states.get(id);
      const b = s1.states.get(id);
      if (a && b) {
        const f = (t - s0.time) / (s1.time - s0.time);
        return { x: a.x + (b.x - a.x) * f, y: a.y + (b.y - a.y) * f };
      }
      return b ?? a; // one side missing (joined/left) -> use whichever exists
    }
    return (s1 ?? s0)!.states.get(id); // clamp to the nearest end
  }
}
```

- [ ] **Step 2: Write the failing check**

Create `client/scripts/interpolation_check.ts`:

```ts
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
buf2.push(0, new Map());        // empty
buf2.push(100, m(100, 0));      // id 1 appears
const joined = buf2.sample(1, 150);
check("id appearing mid-window resolves", joined !== undefined && approx(joined.x, 100));

console.log("VERDICT:", failures === 0 ? "PASS" : "FAIL");
process.exit(failures === 0 ? 0 : 1);
```

- [ ] **Step 3: Run to verify it passes**

Run: `cd client && npx tsc --noEmit && npx tsx scripts/interpolation_check.ts`
Expected: `VERDICT: PASS` (4 checks).

- [ ] **Step 4: Commit**

```bash
git add client/src/net/interpolation.ts client/scripts/interpolation_check.ts
git commit -m "feat(client): remote interpolation buffer (pure)"
```

---

### Task 6: Client — fixed-step input + local self prediction (per-frame render)

**Files:**
- Modify: `client/src/input/keyboard.ts` (expose live input state instead of fire-on-change)
- Modify: `client/src/render/scene.ts` (per-frame render; self from predicted pos; positional vs snapshot-meta split)
- Modify: `client/src/main.ts` (fixed-step accumulator on the ticker: sample → predict → send batched; reconcile on snapshot; camera follows predicted)

**Interfaces:**
- Consumes: `Predictor` (Task 4); `client.sendInput` already accepts a frame batch via `Input.frames`.
- Produces: `installInput(): () => { moveX; moveY; capturing }` (a live reader); `Scene.setSelfPredicted(x,y)`, `Scene.updateMeta(players: PlayerState[])`, `Scene.frame()` (per-frame draw). Remotes still placed from their latest snapshot position in this task (Task 7 interpolates them).

- [ ] **Step 1: Make keyboard expose a live reader**

Replace `client/src/input/keyboard.ts` with:

```ts
// WASD / arrows -> movement direction; 'e' -> capture. Returns a reader for the
// current intent, sampled once per fixed step by the game loop.
export interface KeyboardState {
  moveX: number;
  moveY: number;
  capturing: boolean;
}

export function installInput(): () => KeyboardState {
  const pressed = new Set<string>();
  const axis = (positive: boolean, negative: boolean) => (positive ? 1 : 0) - (negative ? 1 : 0);

  window.addEventListener("keydown", (e) => pressed.add(e.key.toLowerCase()));
  window.addEventListener("keyup", (e) => pressed.delete(e.key.toLowerCase()));

  return () => ({
    moveX: axis(pressed.has("d") || pressed.has("arrowright"), pressed.has("a") || pressed.has("arrowleft")),
    moveY: axis(pressed.has("s") || pressed.has("arrowdown"), pressed.has("w") || pressed.has("arrowup")),
    capturing: pressed.has("e"),
  });
}
```

- [ ] **Step 2: Split Scene into per-frame positions vs snapshot meta**

In `scene.ts`, replace the snapshot-driven `applySnapshot` with a meta store + a self-predicted position + a per-frame `frame()`:

```ts
// Fields (add near the other private members):
private selfPredicted = { x: 0, y: 0 };
private readonly meta = new Map<number, { x: number; y: number; hp: number }>(); // per-id latest snapshot
private selfReadyTick = 0;
private serverTick = 0;

// Called every animation frame by main with the predicted self position.
setSelfPredicted(x: number, y: number): void {
  this.selfPredicted = { x, y };
}

// Called on each snapshot: store positions (remotes) + hp for every player, and self cooldown.
updateMeta(players: readonly PlayerState[], selfReadyTick: number, serverTick: number): void {
  this.selfReadyTick = selfReadyTick;
  this.serverTick = serverTick;
  const seen = new Set<number>();
  for (const p of players) {
    seen.add(p.id);
    this.meta.set(p.id, { x: p.x, y: p.y, hp: p.hp });
  }
  for (const id of [...this.meta.keys()]) if (!seen.has(id)) this.meta.delete(id);
}

// Per-frame render: follow camera on predicted self, draw world + all players.
frame(): void {
  this.camera.setTarget(this.selfPredicted.x, this.selfPredicted.y);
  this.drawWorld();
  this.drawPlayers();
}
```

Rewrite `drawPlayers` to iterate `this.meta`, using the predicted position for self and the snapshot position for remotes (Task 7 swaps remotes to interpolation):

```ts
private drawPlayers(): void {
  const cam = this.camera;
  const seen = new Set<number>();
  for (const [id, mp] of this.meta) {
    seen.add(id);
    let g = this.sprites.get(id);
    if (!g) { g = new Graphics(); this.playersLayer.addChild(g); this.sprites.set(id, g); }
    const isSelf = id === this.selfId;
    const pos = isSelf ? this.selfPredicted : { x: mp.x, y: mp.y };
    g.clear();
    if (mp.hp <= 0) {
      g.circle(0, 0, 5).fill({ color: 0x555555, alpha: 0.6 });
      g.moveTo(-4, -4).lineTo(4, 4).moveTo(-4, 4).lineTo(4, -4).stroke({ width: 1.5, color: 0x1a1a1a });
    } else {
      const color = this.factionColors.get(this.playerFaction.get(id) ?? 0) ?? 0xaaaaaa;
      if (isSelf && this.attackRange > 0) g.circle(0, 0, this.attackRange * cam.scale).stroke({ width: 1, color: 0xffffff, alpha: 0.22 });
      g.circle(0, 0, 5).fill(color);
      if (isSelf) g.circle(0, 0, 8).stroke({ width: 2, color: 0xffffff });
      this.drawHpBar(g, mp.hp);
      if (isSelf) this.drawCooldownBar(g, this.selfReadyTick, this.serverTick);
    }
    const [sx, sy] = cam.worldToScreen(pos.x, pos.y);
    g.position.set(sx, sy);
  }
  for (const [id, g] of this.sprites) if (!seen.has(id)) { g.destroy(); this.sprites.delete(id); }
}
```

Keep `applyCellUpdates`, `setMapState`, `drawWorld`, `screenToCell`, `toggleMap` as-is. Delete the old `applySnapshot`. (These edits also remove the previous per-snapshot camera/draw calls; the ticker now drives rendering.)

- [ ] **Step 3: Wire the fixed-step loop + reconciliation in main.ts**

In `main.ts`: after `welcome`, build the predictor and input reader; run a fixed-step accumulator on the Pixi ticker that samples input, predicts, and sends batched frames; reconcile on snapshot; call `scene.frame()` every tick.

```ts
import { Predictor } from "./net/prediction.js";
import { installInput, type KeyboardState } from "./input/keyboard.js";

// after app/scene/status setup:
const FIXED_DT = 1 / 60;
const MAX_ACCUM = 0.25;
let predictor: Predictor | undefined;
const readKeys: () => KeyboardState = installInput();
let attacking = false; // set by the existing mouse handlers (unchanged from current min-hold logic)
let alive = false;
let acc = 0;
const outbox: { seq: number; moveX: number; moveY: number; capturing: boolean; attack: boolean }[] = [];

// welcome handler: create predictor once config is known
//   const speed = w.config.moveSpeed; const bounds = { maxX: w.config.mapWidth*100 - 1, maxY: w.config.mapHeight*100 - 1 };
//   predictor = new Predictor(speed, FIXED_DT, bounds);

// snapshot handler: replace scene.applySnapshot(...) with:
//   const you = s.you; alive = you?.life === LifeState.ALIVE;
//   if (predictor && you && alive) {
//     const self = s.players.find((p) => p.id === myId);
//     if (self) predictor.reconcile({ x: self.x, y: self.y }, you.lastInputSeq);
//   }
//   scene.updateMeta(s.players, you?.attackReadyTick ?? 0, s.tick);
//   scene.applyCellUpdates(s.cells);
//   (status text as before, using predictor?.position for hp/coords is optional)

// fixed-step game loop on the ticker:
app.ticker.add((ticker) => {
  if (!predictor) { scene.frame(); return; }
  acc = Math.min(acc + ticker.deltaMS / 1000, MAX_ACCUM);
  while (acc >= FIXED_DT) {
    acc -= FIXED_DT;
    const k = readKeys();
    const sample = { moveX: alive ? k.moveX : 0, moveY: alive ? k.moveY : 0, capturing: k.capturing, attack: alive && attacking };
    const f = predictor.step(sample);
    outbox.push(f);
  }
  // flush pending frames in chunks of <=8 (protocol limit); never drop
  while (outbox.length > 0) {
    const batch = outbox.splice(0, 8);
    client.sendInputFrames(batch); // see Step 4
  }
  predictor.decayError(ticker.deltaMS / 1000);
  scene.setSelfPredicted(predictor.renderPosition.x, predictor.renderPosition.y);
  scene.frame();
});
```

**Remove the old send wiring** (critical — otherwise two `seq` sources collide): delete the `pushInput` helper and the old `installInput((moveX, moveY, capturing) => { ... })` callback block. The fixed-step loop is now the **only** sender. The mouse handlers keep their min-hold logic but now **only set the `attacking` flag** — no `pushInput`/`sendInput` call. Per-tick sampling reads `attacking`; the min-hold keeps a quick click's `attacking=true` alive long enough to be sampled by at least one fixed step.

On spawn/respawn and death, snap the predictor: in the click-to-spawn handler after `client.sendSpawn(...)` call `predictor?.reset({ x: col * 100 + 50, y: row * 100 + 50 })`; in the snapshot handler when `you.life !== ALIVE` and a body exists, call `predictor?.reset({ x: self.x, y: self.y })`.

- [ ] **Step 4: Add a batched sender to the client**

In `client.ts`, add alongside `sendInput`:

```ts
sendInputFrames(
  frames: readonly { seq: number; moveX: number; moveY: number; capturing: boolean; attack: boolean }[],
): void {
  this.dispatch(create(ClientMessageSchema, { payload: { case: "input", value: { frames } } }));
}
```

(Keep `sendInput` for the smokes; the predictor generates seqs, so `inputSeq` is no longer used for the live client — that's fine.)

- [ ] **Step 5: Typecheck + build + visual check**

Run: `cd client && npx tsc --noEmit && npx vite build`
Expected: clean typecheck, successful build.

Then visual (server + `npm run dev`): the local player and follow camera move smoothly at 60 FPS with no input lag; standing still, the player holds position (no drift); walking into a wall stops cleanly (no jitter).

- [ ] **Step 6: Commit**

```bash
git add client/src/input/keyboard.ts client/src/render/scene.ts client/src/main.ts client/src/net/client.ts
git commit -m "feat(client): fixed-step input + local prediction of self"
```

---

### Task 7: Client — remote interpolation in the render loop

**Files:**
- Modify: `client/src/render/scene.ts` (remotes drawn from interpolated positions)
- Modify: `client/src/main.ts` (feed snapshots into an InterpolationBuffer; pass interpolated remote positions per frame)

**Interfaces:**
- Consumes: `InterpolationBuffer` (Task 5); `Scene.setSelfPredicted`, `Scene.frame` (Task 6).
- Produces: `Scene.setRemotePositions(positions: Map<number, {x,y}>)` — per-frame remote positions; `drawPlayers` uses them for non-self ids.

- [ ] **Step 1: Scene consumes per-frame remote positions**

In `scene.ts`, add a field and setter, and use it in `drawPlayers`:

```ts
private remotePositions = new Map<number, { x: number; y: number }>();

setRemotePositions(positions: Map<number, { x: number; y: number }>): void {
  this.remotePositions = positions;
}
```

In `drawPlayers`, change the remote branch position source:

```ts
const pos = isSelf
  ? this.selfPredicted
  : (this.remotePositions.get(id) ?? { x: mp.x, y: mp.y }); // interpolated, fallback to snapshot
```

- [ ] **Step 2: main feeds snapshots into the buffer and samples per frame**

In `main.ts`:

```ts
import { InterpolationBuffer, type RemoteState } from "./net/interpolation.js";
const INTERP_DELAY = 100;
const interp = new InterpolationBuffer(INTERP_DELAY);

// snapshot handler, after updateMeta:
//   const states = new Map<number, RemoteState>();
//   for (const p of s.players) if (p.id !== myId) states.set(p.id, { x: p.x, y: p.y });
//   interp.push(performance.now(), states);

// in the ticker, before scene.frame():
//   const now = performance.now();
//   const remotes = new Map<number, { x: number; y: number }>();
//   for (const id of scene.remoteIds()) { const r = interp.sample(id, now); if (r) remotes.set(id, r); }
//   scene.setRemotePositions(remotes);
```

Add a small helper on Scene to enumerate known non-self ids:

```ts
remoteIds(): number[] {
  return [...this.meta.keys()].filter((id) => id !== this.selfId);
}
```

- [ ] **Step 3: Typecheck + build + visual check**

Run: `cd client && npx tsc --noEmit && npx vite build`
Expected: clean.

Visual (two browser tabs, different factions): remote players glide smoothly (no 20 Hz stepping); combat/capture still resolve from snapshots.

- [ ] **Step 4: Commit**

```bash
git add client/src/render/scene.ts client/src/main.ts
git commit -m "feat(client): interpolate remote players in the render loop"
```

---

## Wrap-up

- [ ] **Regression sweep:** `cd server && ./build/bin/unit_tests` (all green); `cd client && npx tsc --noEmit`; run `prediction_check.ts`, `interpolation_check.ts`, and — against a fresh server — `prediction_smoke.ts`, `combat_smoke.ts`, `capture_smoke.ts`, `takeover_smoke.ts`.
- [ ] **Docs:** update `server/docs/sequence-diagram.md` (tick order now includes `consume_inputs`; note fixed timestep) and `client/docs/sequence-diagram.md` (prediction + interpolation loop). Optional, can be a follow-up.
