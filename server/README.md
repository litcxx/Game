# Game Server

Authoritative server for a minimalist multiplayer browser game: players move on a
shared 100×100 grid, fight, and paint territory in their faction's colour. State
lives on the server; the browser client predicts, interpolates, and renders.

> **Status: MVP, work in progress.** Transport (WebSocket + Protobuf + Asio
> coroutines) and the authoritative game loop are in place: a **fixed-timestep**
> simulation with movement, combat — data-driven abilities: a melee area attack
> and a dodgeable projectile on one shared cooldown, and a short block on its
> own cooldown — death/respawn, territory capture, and fog of war, all covered
> by unit tests. Input is consumed **one command per tick**
> (deterministic replay), which the client uses for prediction. A TypeScript +
> PixiJS client renders the map with client-side prediction/interpolation, a
> follow camera, HUD, and minimap. Authentication is intentionally parked.

## Tech stack

- **C++23** (coroutines, concepts)
- **Boost.Asio / Boost.Beast** — WebSocket transport, one coroutine per session
- **Protocol Buffers** — wire protocol (`../protocol/game/v1/protocol.proto`)
- **spdlog**, **nlohmann/json**, **GoogleTest**
- **CMake** with sanitizer presets, `clang-format` / `clang-tidy` targets

## Architecture

```
                io threads (asio)                         game thread
  client ──ws──▶ Session.do_read ─┐                    ┌─▶ World.tick
                                  │  push_packet        │     dispatch on payload type
                                  ▼                     │     (on_hello/spawn/input/ping)
                   TSQueue<ClientEvent> ──swap─────────▶┘            │
                     {session_id, kind, ClientMessage}              │ send()
                                                                    ▼
  client ◀─ws── Session.do_send ◀─ per-session channel ◀── IClientGateway.send_to
```

- **Transport / framing.** WebSocket binary frames; **one frame = one Protobuf
  message** — no hand-rolled length/opcode framing (the WebSocket layer already
  delimits messages).
- **Session** (`src/net/session`) is pure transport: read a whole message, parse
  a `ClientMessage`, and forward it tagged with its session id (`ClientEvent`).
  Outgoing frames go through a per-session concurrent channel (64 frames)
  drained by `do_send`; when it is full, a frame is refused, not queued.
- **Server** (`src/net/server`) owns the acceptor and the session registry, and
  implements `IClientGateway` (the game loop's way back to clients). Each session
  is supervised by one coroutine that runs `do_read` and `do_send` together
  (`co_await (a || b)`) and removes the session only after **both** finish — so
  teardown never frees a session out from under a live coroutine.
- **World** (`src/game/world`) is the authoritative loop, run at a **fixed
  timestep** (accumulator). It only orchestrates: each tick it drains the inbound
  queue (dispatch → handlers, input frames → per-player queue), then runs the
  systems in order — consume one input command per player, integrate movement,
  rebuild the spatial index, start blocks, resolve attacks (melee hits,
  projectile launches), fly projectiles (swept hits), advance territory
  capture — and sends a per-recipient snapshot, filtered by fog of war.
  `SelfState.last_input_seq` acks the consumed command so the client can
  reconcile its prediction.
- **Systems over plain data.** All simulation state is one plain struct,
  `WorldState` (`src/game/state`: players, territory grid, pending events). The
  per-tick logic lives in free functions over it (`src/game/systems`), so each
  rule is small and testable on its own. Every hp loss goes through a single
  choke point, `apply_damage()` (hit/death events, respawn timer). A uniform-grid
  `SpatialIndex` (`src/game/spatial`) answers "who is within r of here" without
  scanning every pair. Outbound messages are pure builders in `src/game/sync`;
  the snapshot is built per recipient.
- **Fog of war is a delivery filter.** The simulation ignores sight — attacks
  from the fog land as usual. On each snapshot `compute_vision()` marks, once
  per faction, the cells whose centre is within `vision_radius` (config; 300 = 3 cells)
  of a source: an alive player of the faction or a cell it owns (a body still
  sees in the snapshot reporting its death, not after). `build_snapshot()` then
  sends a recipient only players and projectiles on visible cells (itself
  always), events whose named players are all visible (a hit from the fog tells
  the victim nothing), and the visible cells that changed plus every newly
  revealed cell with its current state. The difference from what the client was
  told last time (`Player::sync.vision`) goes out as `revealed` / `hidden`; the
  client remembers explored cells. A joiner's `MapState` reveals nothing.
- **Delivery is checked.** The protocol is delta-based (cells, sight, roster),
  so a lost frame would leave the client out of step for good.
  `IClientGateway::send_to` reports whether the frame was queued; on a refusal
  `note_drop()` marks the client for a **resync**: its next snapshot goes out
  after a `Roster{full}` and starts its deltas from nothing (every visible cell
  revealed with its state, `Snapshot.resync` set — the client first turns what
  it held as visible to explored). Another refusal on a later tick within
  `resync_window_ms` (5 s) means it can't keep up: `World` closes the session
  at the end of the tick and it leaves the world. What was told to a client and
  how delivery goes is kept per connection in `Player::sync` (`ClientSync`).
- **Abilities are data.** The config lists them (`melee` / `projectile` with
  cooldown, damage, range, projectile speed and radius); Welcome sends them to
  clients, and each input frame picks one by id plus an aim vector. Using any
  ability starts one shared cooldown with that ability's length. A projectile
  flies from the shooter's centre along the aim; each tick its step is swept
  against alive enemies (segment vs body + projectile radius, first contact
  wins), so fast shots can't tunnel and a target that moves out of the line
  dodges it. A `block` ability runs on its own timer, independent of the attack
  cooldown: for `duration_ticks` the player takes no damage — `apply_damage()`
  records a blocked hit instead (a projectile is spent on it). Blocks start
  before attacks in the tick, so one pressed together with a swing already
  stops it. Every ability use is announced (`AbilityEvent`) so clients can show
  what others pressed.
- **Seam.** `World` depends only on the abstract `IClientGateway`
  (`src/shared/net`: `send_to`, `broadcast`, `disconnect`), not on the network
  layer — so the game code stays testable (a mock gateway can refuse frames)
  and free of transport details.

## Layout

```
../protocol/game/v1/protocol.proto   wire protocol (shared with the client)
src/net/       server, session — WebSocket transport + coroutines
src/game/      the simulation:
  world/         World — tick orchestration, event dispatch, delivery; fixed step
  state/         WorldState, Player, Territory, Vision, ClientSync — plain data
  systems/       input, movement, combat (abilities: blocks, melee, projectile
                 launch), projectiles (flight, swept hits), damage, capture, spawn,
                 vision (fog of war: what a faction sees)
  spatial/       SpatialIndex — uniform-grid broad phase over player positions;
                 geometry — swept segment-vs-circle hit test
  sync/          ServerMessage builders + per-recipient snapshot (fog-filtered,
                 resync), delivery policy for dropped frames (note_drop)
src/shared/    config, net (IClientGateway, ClientEvent), utils (TSQueue)
config/        config.json (runtime settings + game rules)
test/          unit + integration (GoogleTest)
docs/          sequence + ownership diagrams (being updated for the refactor)
```

Generated Protobuf headers land in the build tree (`build/proto/game/v1/`).
Some directories still hold pre-refactor code (the old binary protocol, the
platformer physics, the auth/DB module) that is disabled in the build and being
phased out.

## Prerequisites

- A C++23 compiler and **CMake ≥ 3.21**
- **Boost** (Asio + Beast), **Protobuf** (`protoc` + `libprotobuf`, CMake config
  mode), **OpenSSL**

`spdlog`, `nlohmann/json`, and `GoogleTest` are fetched automatically via
`FetchContent`.

## Build & run

```bash
cmake --preset debug-asan      # or: debug-tsan | debug | release
cmake --build build -j
./build/bin/server config/config.json
```

Presets differ only in build type / sanitizer (`debug-asan` enables
Address+UB sanitizers). The server reads its address, port, thread count, and the
game rules (matching `game.v1.GameConfig`, plus the factions and the abilities,
and the server-only `capture_ticks`, `vision_radius` and `resync_window_ms`)
from the config file; invalid abilities or a zero vision radius or resync window
stop the server at startup.

## Tests

```bash
ctest --preset debug-asan      # or run the binary directly:
./build/bin/unit_tests
```

The `World` suites (presence, spawn, movement, combat, abilities, ranged, block,
capture, input, fog, resync) test the game end to end through a mock gateway;
`Damage`, `SpatialIndex`, `SegmentCircle`, `Projectiles`, `Vision`, `Delivery`
and `GameConfigParse` test those units directly. They all run by default.
Some legacy suites (the old binary protocol and the parked auth/DB integration
tests) remain disabled in the CMake test lists.

## Code style

```bash
cmake --build build --target clang-format-fix   # format in place
cmake --build build --target clang-tidy         # lint
```
