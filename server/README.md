# Game Server

Authoritative server for a minimalist multiplayer browser game: players move on a
shared 100×100 grid, fight, and paint territory in their faction's colour. State
lives on the server; the browser client is a thin renderer.

> **Status: MVP, work in progress.** The network layer (WebSocket + Protobuf +
> Asio coroutines) and the game-loop skeleton are in place. Game mechanics
> (movement, combat, respawn, territory capture) are still stubs. Authentication
> is intentionally parked, and the browser client is not started yet.

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
                   TSQueue<ClientEnvelope> ──swap──────▶┘            │
                        {session_id, ClientMessage}                 │ send()
                                                                    ▼
  client ◀─ws── Session.do_send ◀─ per-session channel ◀── IClientGateway.send_to
```

- **Transport / framing.** WebSocket binary frames; **one frame = one Protobuf
  message** — no hand-rolled length/opcode framing (the WebSocket layer already
  delimits messages).
- **Session** (`src/net/session`) is pure transport: read a whole message, parse
  a `ClientMessage`, and forward it tagged with its session id
  (`ClientEnvelope`). Outgoing frames go through a per-session concurrent channel
  drained by `do_send`.
- **Server** (`src/net/server`) owns the acceptor and the session registry, and
  implements `IClientGateway` (the game loop's way back to clients). Each session
  is supervised by one coroutine that runs `do_read` and `do_send` together
  (`co_await (a || b)`) and removes the session only after **both** finish — so
  teardown never frees a session out from under a live coroutine.
- **World** (`src/game/world`) is the authoritative loop: it drains the inbound
  queue each tick and dispatches by message type. Handlers are currently stubs.
- **Seam.** `World` depends only on the abstract `IClientGateway`
  (`src/shared/net`), not on the network layer — so the game code stays testable
  (mockable gateway) and free of transport details.

## Layout

```
../protocol/game/v1/protocol.proto   wire protocol (shared with the client)
src/net/       server, session — WebSocket transport + coroutines
src/game/      world — the game loop (+ player/tile entities)
src/shared/    config, net (IClientGateway, ClientEnvelope), utils (TSQueue)
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
game rules (matching `game.v1.GameConfig`) from the config file.

## Tests

```bash
ctest --preset debug-asan      # or run the binary directly:
./build/bin/unit_tests
```

Some suites tied to modules under active refactoring (the old binary protocol,
`World`, and the parked auth/DB integration tests) are temporarily disabled in the
CMake test lists and will be re-enabled as those modules land.

## Code style

```bash
cmake --build build --target clang-format-fix   # format in place
cmake --build build --target clang-tidy         # lint
```
