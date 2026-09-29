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
> follow camera, HUD, and minimap. There are no accounts yet: a player joins
> with a name and gets a session token that brings the same character back after
> a disconnect.

## Tech stack

- **C++23** (coroutines, concepts)
- **Boost.Asio / Boost.Beast** — WebSocket transport, one coroutine per session
- **Protocol Buffers** — wire protocol (`../protocol/game/v1/protocol.proto`)
- **OpenSSL** (libcrypto) — the session token: `RAND_bytes` and SHA-256
- **spdlog**, **nlohmann/json**, **GoogleTest**
- **CMake** with sanitizer presets, `clang-format` / `clang-tidy` targets

## Architecture

```
                io threads (asio)                         game thread
  client ──ws──▶ Session.do_read ─┐                    ┌─▶ World.tick
                                  │  push_packet        │     dispatch on payload type
                                  ▼                     │     (on_hello/spawn/input/ping,
                                                        │      send_error)
                   TSQueue<ClientEvent> ──swap─────────▶┘            │
                     {session_id, kind, ClientMessage}              │ send()
                                                                    ▼
  client ◀─ws── Session.do_send ◀─ per-session channel ◀── IClientGateway.send_to
```

- **Transport / framing.** WebSocket binary frames; **one frame = one Protobuf
  message** — no hand-rolled length/opcode framing (the WebSocket layer already
  delimits messages). The upgrade is taken on any path, so a reverse proxy can
  forward e.g. `/ws` (where a production client connects) as it is.
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
  queue (dispatch → handlers, input frames → the session's queue), then runs
  the systems in order — consume one input command per session into its
  character's body, integrate movement,
  rebuild the spatial index, start blocks, resolve attacks (melee hits,
  projectile launches), fly projectiles (swept hits), advance territory
  capture — and sends a per-recipient snapshot, filtered by fog of war.
  `SelfState.last_input_seq` acks the consumed command so the client can
  reconcile its prediction.
- **Systems over plain data.** All simulation state is one plain struct,
  `WorldState` (`src/game/state`: characters, units, sessions, territory grid,
  projectiles, pending events). The
  per-tick logic lives in free functions over it (`src/game/systems`), so each
  rule is small and testable on its own. Every hp loss goes through a single
  choke point, `apply_damage()` (hit/death events, respawn timer). A uniform-grid
  `SpatialIndex` (`src/game/spatial`) answers "who is within r of here" without
  scanning every pair. Outbound messages are pure builders in `src/game/sync`;
  the snapshot is built per recipient.
- **Characters, sessions and units.** A *character* is a player's identity: its
  `id` is the public `player_id` (never reused) and it has a name. A *session*
  (`ClientSession`, one per connection) drives one character; the input it sends
  (queue, seqs, ack) and what it has been told (`ClientSync`) belong to the
  connection, so another session taking the character over starts both afresh.
  The character's *body* is a `Unit` with the same id — position, hp, life,
  faction, cooldowns and the current `Intent` — from its spawn on (a dead body
  stays until it respawns); with no unit the player is `NOT_SPAWNED`. Movement,
  blocks, attacks, projectiles, `apply_damage()`, capture, vision and the
  `SpatialIndex` work over units only, so a unit without a character — a
  monster later, its intent written by an AI — fights through the same code.
  `World` joins and leaves through `src/game/systems/presence_system`
  (`create_character`, `attach_session`, `detach_session`, `leave_world`,
  `leave_after_grace`, `find_by_token`).
- **Reconnect.** Welcome hands a new player a session token: 128 bits from
  OpenSSL's `RAND_bytes` as 32 hex digits (`src/shared/utils/session_token`).
  The server keeps only its SHA-256 on the character and never logs it. A
  dropped session leaves its character in the world, **away** — visible and
  vulnerable, its body standing still — for `reconnect_grace_ms` (30 s): closing
  the tab does not take it out of a fight. Then the character leaves the world
  with its body (`Roster.removed`); its record stays. A `Hello` with the token
  brings it back whatever name it carries: still in the world, it is the same
  body in the same state (`Welcome.resumed`), which no one else notices; after
  the grace it comes back `NOT_SPAWNED`, with the same id (and joins the
  others' rosters again). The session is new either way, so it gets the full
  greeting — `MapState`, the full roster — and its first snapshot reveals all
  it sees. A second connection with the token takes the character over: the
  one driving it gets `SESSION_REPLACED` (fatal). An unknown token (e.g. from
  before a restart) is no token: a new character. The faction still comes with
  the body and is picked again on spawn after the grace (pinning it to the
  character is GAME-014).
- **Fog of war is a delivery filter.** The simulation ignores sight — attacks
  from the fog land as usual. On each snapshot `compute_vision()` marks, once
  per faction, the cells whose centre is within `vision_radius` (config; 300 = 3 cells)
  of a source: an alive unit of the faction or a cell it owns (a body still
  sees in the snapshot reporting its death, not after). `build_snapshot()` then
  sends a recipient only units and projectiles on visible cells (its own body
  always), events whose named units are all visible (a hit from the fog tells
  the victim nothing), and the visible cells that changed plus every newly
  revealed cell with its current state. The difference from what the client was
  told last time (`ClientSession::sync.vision`) goes out as `revealed` / `hidden`; the
  client remembers explored cells. A joiner's `MapState` reveals nothing.
- **Delivery is checked.** The protocol is delta-based (cells, sight, roster),
  so a lost frame would leave the client out of step for good.
  `IClientGateway::send_to` reports whether the frame was queued; on a refusal
  `note_drop()` marks the client for a **resync**: its next snapshot goes out
  after a `Roster{full}` and starts its deltas from nothing (every visible cell
  revealed with its state, `Snapshot.resync` set — the client first turns what
  it held as visible to explored). Another refusal on a later tick within
  `limits.resync_window_ms` (5 s) means it can't keep up: `World` closes the
  session at the end of the tick, and its character is away until the client
  reconnects or the grace ends. What was told to a
  client and how delivery goes is kept per connection in `ClientSession::sync`
  (`ClientSync`).
- **Abilities are data.** The config lists them (`melee` / `projectile` with
  cooldown, damage, range, projectile speed and radius); Welcome sends them to
  clients, and each input frame picks one by id plus an aim vector. Using any
  ability starts one shared cooldown with that ability's length. A projectile
  flies from the shooter's centre along the aim; each tick its step is swept
  against alive enemies (segment vs body + projectile radius, first contact
  wins), so fast shots can't tunnel and a target that moves out of the line
  dodges it. A `block` ability runs on its own timer, independent of the attack
  cooldown: for `duration_ticks` the unit takes no damage — `apply_damage()`
  records a blocked hit instead (a projectile is spent on it). Blocks start
  before attacks in the tick, so one pressed together with a swing already
  stops it. Every ability use is announced (`AbilityEvent`) so clients can show
  what others pressed.
- **Connection limits.** `Server` announces each session with a `Connected`
  event, ahead of its messages; `World` keeps a `Connection` for it (until
  `Disconnected`). Every message spends a token-bucket budget
  (`messages_per_second`, a burst of as many is fine) — over it is
  `RATE_LIMITED`; each tick `time_out_connections()` sends `HANDSHAKE_TIMEOUT`
  (no `Hello` within `handshake_timeout_ms`) or `IDLE_TIMEOUT` (nothing heard for
  `idle_timeout_ms`; the client pings every 2 s). All three are fatal, through
  `send_error()` below. Input is capped without an error: at most
  `max_input_frames` taken from one `Input`, at most `input_queue` kept — a burst
  after a stall only costs a prediction correction. The numbers are
  `game.limits` in the config.
- **Joining and errors.** A connection's first message is `Hello`:
  `check_hello()` accepts `PROTOCOL_VERSION_CURRENT` (defined once, in the proto)
  and a name trimmed of spaces, 1–16 characters (not bytes) with no control or
  invisible ones. A new player's name must not be any character's already,
  ignoring the case of Latin and Cyrillic letters (`is_name_taken()`, a
  stopgap until accounts) — else `INVALID_NAME`. Anything before `Hello`, or a
  second `Hello`, is `UNEXPECTED_MESSAGE`. `send_error()` sends a `ServerError`
  echoing the request's `request_id`; codes 1–19 are fatal — the session is
  released: its character is away (see Reconnect), its send queue writes the
  error and then closes the WebSocket with 4000 + code, and whatever it still
  sends is ignored until its `Disconnected`. Codes 20+ refuse one request (a spawn: `SPAWN_INVALID_CELL`,
  `SPAWN_TOO_EARLY`, `ALREADY_SPAWNED`, `INVALID_FACTION`; an empty payload:
  `UNSUPPORTED_MESSAGE`) and the connection lives on.
- **Metrics.** `World` times every tick and counts in `Metrics` what a playtest
  needs to see (CCU, tick time, snapshot size, delivery trouble, joins, leaves and
  resumes, errors); every `log.metrics_interval_s` it logs them as one `metrics {json}`
  line and starts over — see [Logs & metrics](#logs--metrics).
- **Seam.** `World` depends only on the abstract `IClientGateway`
  (`src/shared/net`: `send_to`, `broadcast`, `disconnect` with a reason), not on
  the network layer — so the game code stays testable (a mock gateway can refuse
  frames and records closes) and free of transport details.

## Layout

```
../protocol/game/v1/protocol.proto   wire protocol (shared with the client)
proto/         builds that schema into the `proto` library (protoc -> C++)
third_party/   FetchContent: spdlog, nlohmann/json
scripts/       install-protobuf.sh — the pinned protobuf, built from source
src/main.cpp   entry point: config, io threads, game thread, shutdown on signal
src/asan_default_options.cpp   ASan defaults for the server and the unit tests
src/net/       server, session — WebSocket transport + coroutines
src/game/      the simulation:
  world/         World — tick orchestration, event dispatch, delivery; fixed step;
                 Metrics — the periodic `metrics` log line
  state/         WorldState, Character, Unit (Intent), ClientSession (InputQueue,
                 ClientSync), Projectile, Territory, Vision — plain data
  systems/       input, movement, combat (abilities: blocks, melee, projectile
                 launch), projectiles (flight, swept hits), damage, capture, spawn
                 (refusal reasons), join (Hello rules: version, name), presence
                 (characters and the sessions driving them), vision (fog of war:
                 what a faction sees)
  spatial/       SpatialIndex — uniform-grid broad phase over unit positions;
                 geometry — swept segment-vs-circle hit test
  sync/          ServerMessage builders + per-recipient snapshot (fog-filtered,
                 resync), delivery policy for dropped frames (note_drop)
src/shared/    config, net (IClientGateway, ClientEvent), utils (TSQueue, session
               token: generation and hash)
config/        config.json (runtime settings + game rules)
test/unit/     unit tests (GoogleTest), mirroring src/
docs/          sequence + ownership diagrams
```

Generated Protobuf headers land in the build tree (`build/proto/game/v1/`).

## Prerequisites

The versions CI builds and tests with (Ubuntu 24.04):

| Dependency | Version | Where from |
|---|---|---|
| GCC (C++23) | 13.3 | `g++` |
| CMake | 3.28 (≥ 3.21 for the presets) | `cmake` |
| Ninja | 1.11 | `ninja-build` (optional; any generator works) |
| Protobuf (`protoc` + `libprotobuf`, with abseil) | 36.2 | `scripts/install-protobuf.sh` (from source) |
| Boost (Asio + Beast, header-only) | 1.83 | `libboost-dev` |
| OpenSSL (libcrypto) | 3.0.13 | `libssl-dev` |

```bash
sudo apt-get install g++ cmake ninja-build libboost-dev libssl-dev
scripts/install-protobuf.sh            # protobuf 36.2 into /usr/local (about 5 minutes)
scripts/install-protobuf.sh ~/protobuf # ... or anywhere, then cmake -DCMAKE_PREFIX_PATH=~/protobuf
```

**Protobuf 36 only.** The build takes protobuf from its own CMake package, at
version 36 or newer, and stops with a hint otherwise: one version is tested, and
distro packages such as Ubuntu 24.04's 3.21 are too old (and ship no CMake
package). The script builds the pinned release, checked by SHA-256, the same way
CI does. `spdlog` 1.12.0, `nlohmann/json` 3.11.0 and `GoogleTest` 1.15.0 are
fetched (pinned by hash) via `FetchContent`.

**ASan and prebuilt libraries.** libprotobuf is not built with ASan, while its
headers mark container memory for ASan in our instrumented code only; the two
disagree, and ASan would report container overflows that are not there. So the
server and the unit tests turn that one check off by default
(`src/asan_default_options.cpp`: `detect_container_overflow=0`); `ASAN_OPTIONS`
still overrides it.

## Build & run

```bash
cmake --preset debug-asan      # or: debug-tsan | debug | release
cmake --build build -j
./build/bin/server config/config.json
```

Presets differ only in build type / sanitizer (`debug-asan` enables
Address+UB sanitizers). The server reads its address, port, thread count, and the
game rules (matching `game.v1.GameConfig`, plus the factions and the abilities,
and the server-only `capture_ticks`, `vision_radius` and the connection
`limits`) from the config file, and the `log` section below; invalid abilities, a
zero vision radius, a zero or missing limit, an unknown log level or a zero
metrics interval stop the server at startup.

## Logs & metrics

The server logs to stdout (spdlog); keep it for a playtest, e.g.
`./build/bin/server config/config.json 2>&1 | tee server.log`. The config's `log`
section sets the level — `trace` | `debug` | `info` | `warn` | `error` |
`critical` | `off` (`info`; the startup lines about the config come before it
applies) — and `metrics_interval_s` (60).

Every `metrics_interval_s` the game thread logs one line at `info` (so none at
`warn` and above): `metrics ` and a JSON object.

```
[2026-09-29 15:30:14.974] [info] metrics {"period_s":60,"ccu":2,"ticks":3600,"tick_us":{"avg":113,"p99":1269,"max":2070},"snapshot_ticks":1200,"snapshot_tick_us":{"avg":183,"p99":2024,"max":2070},"snapshots":2400,"snapshot_bytes_avg":35,"drops":0,"resyncs":0,"closed_behind":0,"joins":3,"leaves":1,"resumes":2,"errors":{"RATE_LIMITED":1}}
```

| Field | Over the period |
|---|---|
| `period_s` | its length, seconds |
| `ccu` | players in the world at its end |
| `ticks`, `tick_us` | ticks run and how long they took, µs: `avg`, `p99` (nearest rank), `max` — the whole tick, from draining events to closing sessions; the budget at 60 Hz is 16 667 |
| `snapshot_ticks`, `snapshot_tick_us` | the same for the ticks that send snapshots (every `tick_rate / snapshot_rate`-th) |
| `snapshots`, `snapshot_bytes_avg` | snapshots built (one per player per snapshot tick) and their average size, bytes |
| `drops` | frames the send queue refused: a client that can't keep up, or one whose connection is already closing |
| `resyncs` | resync snapshots sent after a drop |
| `closed_behind` | sessions closed for another drop within `limits.resync_window_ms` |
| `joins` | characters entering the world: new ones, and those back after their reconnect grace |
| `leaves` | characters leaving it: their reconnect grace is over |
| `resumes` | reconnects within the grace, and takeovers from another tab (the character never left) |
| `errors` | `ServerError`s sent, by code: `RATE_LIMITED`, `IDLE_TIMEOUT`, … |

**Export.** The lines turn into JSON Lines with the log's timestamp as `time`:

```bash
sed -nE 's/^\[([^]]+)\] \[info\] metrics \{/{"time":"\1",/p' server.log > metrics.jsonl
# under systemd: journalctl -u <unit> -o cat | sed -nE '…the same…' > metrics.jsonl
```

and from there, with `jq`, into a CSV for a spreadsheet, or answers directly:

```bash
{ echo time,ccu,tick_avg_us,tick_p99_us,tick_max_us,snapshot_tick_p99_us,snapshot_bytes_avg,drops,resyncs,closed_behind,joins,leaves,resumes
  jq -r '[.time, .ccu, .tick_us.avg, .tick_us.p99, .tick_us.max, .snapshot_tick_us.p99,
          .snapshot_bytes_avg, .drops, .resyncs, .closed_behind, .joins, .leaves, .resumes] | @csv' metrics.jsonl
} > metrics.csv

jq -s 'map(.tick_us.p99) | max' metrics.jsonl    # the worst p99 of the playtest
jq -s 'map(.errors | to_entries[]) | group_by(.key)
       | map({(.[0].key): (map(.value) | add)}) | add' metrics.jsonl    # errors, summed
```

## Tests

```bash
ctest --preset debug-asan      # or run the binary directly:
./build/bin/unit_tests
```

CI ([`../.github/workflows/ci.yml`](../.github/workflows/ci.yml)) builds
`debug-asan` on every PR and push to `main`, runs the unit tests, then starts the
server and runs the client's end-to-end smoke suite against it
(`npm run smoke`, see [`../client`](../client)); the server must then shut down
cleanly (exit 0 — no sanitizer report, no leak).

The `World*` suites (`Hello`, `Roster`, `Spawn`, `Movement`, `Input`, `Snapshot`,
`Combat`, `Ability`, `Ranged`, `Block`, `Capture`, `Fog`, `Resync`, `Errors`,
`Limits`, `Metrics`, `Reconnect`) test the game end to end through a mock
gateway; `Damage`, `SpatialIndex`, `SegmentCircle`, `Projectiles`, `Vision`,
`Delivery`, `HelloRules`, `PlayerName`, `ConnectionLimits`, `InputLimits`,
`FixedStep`, `Metrics`, `GameConfigParse`, `LogConfigParse`, `SessionToken` and
`TSQueueTest` test those units directly; `Presence` covers characters and
sessions (away, the grace, takeover, two sessions in a row driving one
character) and `UnitWithoutCharacter`
a body no player drives fighting through the same systems. All of them run by
default.

## Code style

```bash
cmake --build build --target clang-format-fix   # format in place
cmake --build build --target clang-tidy         # lint
```
