# Client-side prediction + fixed timestep — design

**Status:** proposed · **Date:** 2026-09-27

## Intent

Make the game feel smooth. Today the local player and the follow camera move only
when a snapshot arrives (20 Hz), so movement steps in ~50 ms jumps and the local
player also lags behind input by a round-trip. Goal: the player you control moves
at render framerate with no input lag, other players move smoothly, and the server
stays authoritative and deterministic.

Chosen scope (agreed): **full** — deterministic client prediction of the local
player with server reconciliation, plus a **fixed timestep** on the server, plus
interpolation of remote players.

## Background: current state

- **Server** (`world.cpp`): `run()` calls `tick(dt)` with a *variable* wall-clock
  `dt`. `on_input` takes the latest `InputFrame` and stores it as a persistent
  intent (`move_x/move_y/capturing/attack`); `update(dt)` integrates that intent
  each tick: `pos += normalize(dir) * move_speed * dt`, clamped to
  `[0, map*100 - 1]`. `SelfState.last_input_seq` echoes the last processed frame.
- **Client**: sends one `InputFrame` only when intent *changes*; renders each
  player sprite directly at the snapshot position (no interpolation, no
  prediction). Rendering only happens on snapshot receipt.
- **Protocol** already fits the target model: `Input.frames` is `repeated`
  ("several accumulated, ≤ 8"), each `InputFrame` has `seq`, and
  `SelfState.last_input_seq` acks the last applied frame. No protocol change.

## Goals / non-goals

**Goals:** zero-input-lag, framerate-smooth local player; smooth remote players;
deterministic server sim; correct authority (server wins). Client-side prediction
covers **movement only**.

**Non-goals:** predicting combat/capture outcomes (stay server-authoritative,
shown from snapshots); full packet-loss rollback / duplicate-frame accounting
(LAN-first MVP); changing tick/snapshot rates; touching the protocol.

## Design

### Shared integration (must match byte-for-byte in intent)

Both server and client advance a position with the *same* function and the same
`fixedDt = 1 / tick_rate`:

```
integrate(pos, moveX, moveY, dt):
  if moveX == 0 and moveY == 0: return pos
  len = sqrt(moveX^2 + moveY^2)
  pos.x = clamp(pos.x + moveX/len * move_speed * dt, 0, mapW*100 - 1)
  pos.y = clamp(pos.y + moveY/len * move_speed * dt, 0, mapH*100 - 1)
  return pos
```

Determinism depends on identical `dt` on both sides — hence the fixed timestep.

### 1. Server — fixed timestep

`World::run` switches to an accumulator loop:

```
fixedDt = 1 / tick_rate
accumulator += frameElapsed          // real time since last loop
accumulator = min(accumulator, MAX)  // clamp; avoid spiral of death
while accumulator >= fixedDt: tick(fixedDt); accumulator -= fixedDt
sleep briefly
```

`tick()` always uses `fixedDt`. Snapshot cadence stays tick-count based
(`tick_ % snapshot_interval_`), so 20 Hz snapshots are unchanged.

### 2. Server — per-tick input queue

Each `Player` gets an input FIFO of commands `{seq, move_x, move_y, capturing,
attack}`.

- `on_input` **enqueues** each frame whose `seq` is greater than the last enqueued
  seq (ordered, dedup). It no longer sets intent directly.
- A new `consume_inputs()` step runs at the start of each tick (after draining
  events, before `update`): for every player, pop **one** command from its queue
  and set the current intent + `last_input_seq = seq`. If the queue is empty
  (frame hasn't arrived), **repeat** the last command. Movement is applied only to
  alive players (existing guard in `update`); `last_input_seq` advances regardless
  so acks keep flowing while dead/not-spawned.

Tick order becomes: drain events → `consume_inputs()` → `update(fixedDt)` →
`update_combat()` → `update_captures()` → `send_snapshots()`.

### 3. Client — fixed-step prediction

A predictor runs its own fixed-`dt` accumulator, driven by the Pixi ticker. Each
fixed step:

1. sample held input → `InputFrame{seq++, move, capturing, attack}`;
2. `predicted = integrate(predicted, move, fixedDt)`;
3. push the frame to an **unacked buffer**;
4. mark it for sending.

Frames produced since the last render frame are flushed as one `Input` message
(≤ 8 frames per message; on a large hitch, send multiple messages). The local
player sprite and the follow camera read `predicted` **every render frame** →
smooth and lag-free. Input sampling captures short clicks (button-down-since-last-
step counts), which also removes the earlier click "min-hold" workaround.

### 4. Client — reconciliation

On each snapshot, using `you` (authoritative self pos) and `you.last_input_seq`:

```
buffer = buffer.filter(f => f.seq > last_input_seq)   // drop acked
corrected = authoritativePos
for f in buffer (ascending seq): corrected = integrate(corrected, f.move, fixedDt)
error = predicted - corrected      // ~0 when replay matches (normal case)
predicted = corrected
```

Same dt + same integration ⇒ the replay reproduces the server exactly, so `error`
is ~0 in the normal case (no visible change). On genuine divergence (clamp at a
wall, teleport, death) `error` is nonzero; we render at `predicted + error` and
**decay `error` toward 0 over ~100 ms** so corrections ease instead of popping.
Spawn / respawn / death set `error = 0` (hard snap).

### 5. Client — remote interpolation

Keep a short buffer of received snapshots, each stamped with local arrival time.
Render remote players at `renderTime = now − INTERP_DELAY` (≈ 100 ms ≈ 2
snapshots), lerping each remote's position between the two snapshots bracketing
`renderTime`. Starved (renderTime newer than newest) → clamp to newest; older than
oldest → clamp to oldest. `hp` / faction / life come from the newest snapshot.
The **local** player is excluded from interpolation (it is predicted).

### Render loop

Rendering moves onto the Pixi ticker (per frame), not snapshot receipt:
- self → predicted position (+ decaying error);
- remotes → interpolated position;
- follow camera → tracks predicted self each frame;
- world grid → redraws on camera move (follow ⇒ each frame; culled, ~120 cells).

Snapshots feed the predictor (reconcile) and the interpolation buffer; they no
longer directly place sprites.

## Data flow

```
Client fixed step ─► InputFrame{seq} ─► predict(local) ─► unacked buffer ─► WS ─► server queue
                                                                                     │ consume 1/tick
Server tick (fixedDt): consume_inputs ─► update ─► combat ─► captures ─► Snapshot{you.last_input_seq, players, tick}
                                                                                     │
Client ◄── Snapshot ──┬─ reconcile self (drop acked, replay unacked, ease error)
                      └─ push to interpolation buffer (remotes rendered ~100 ms behind)
```

## Edge cases & decisions

- **Underrun** (server queue empty for a tick): repeat last command; `last_input_seq`
  stays. Rare on LAN; any resulting drift is absorbed by error decay. No rollback.
- **Burst delivery**: server catches up one command per tick; buffer holds ~RTT of
  frames in steady state.
- **Clamp / walls**: server clamps; client integrate clamps identically ⇒ matches.
- **Spawn / respawn / death / teleport**: snap predicted to authoritative, error=0.
- **Not-alive**: client stops predicting movement (dead body static); server still
  advances `last_input_seq` so acks flow.
- **Rates**: tick 60, snapshot 20 unchanged; `INTERP_DELAY ≈ 100 ms`;
  `MAX accumulator ≈ 0.25 s`.

## Testing

- **Server (unit, GoogleTest):** fixed-step determinism (N ticks of a held command
  ⇒ exactly N·speed·fixedDt, and clamp at bounds); per-tick queue (one frame/tick,
  `last_input_seq` advances, empty queue repeats last).
- **Client (tsx pure-logic checks):** predictor apply/replay; reconciliation drops
  acked frames and replays unacked to the corrected present; interpolation lerps
  between two snapshots and clamps at ends.
- **e2e smoke:** send per-tick frames; assert authoritative movement and
  `last_input_seq` acks advance.
- **Visual:** browser — local player and camera smooth at 60 FPS, remotes smooth,
  corrections not jarring.

## Scope / files

- **Server:** `world.hpp` (`Player` input queue; `consume_inputs` decl),
  `world.cpp` (fixed-step `run`, `on_input` enqueues, `consume_inputs`, tick
  order). Config unchanged.
- **Client:** new `src/net/prediction.ts` (predictor + reconciliation) and
  interpolation buffer; `src/render/scene.ts` (per-frame render: predicted self +
  interpolated remotes); `src/input` + `src/main.ts` (fixed-step input generation,
  ticker-driven render). Generated protobuf unchanged.
- **Protocol:** unchanged.
