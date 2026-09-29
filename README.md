# Game

[![CI](https://github.com/litcxx/Game/actions/workflows/ci.yml/badge.svg)](https://github.com/litcxx/Game/actions/workflows/ci.yml)

A minimalist top-down multiplayer browser game. Players join a shared **100×100
grid**, pick a faction, move around, fight, and capture territory painted in
their faction's colour. The **server is authoritative** — it owns all state and
runs the simulation; the browser client sends intent, predicts its own player,
interpolates the others, and renders.

> **Status: MVP, work in progress.** Working today: connection/presence,
> faction choice, spawn, movement, combat (a melee area attack and a dodgeable
> ranged projectile on a shared cooldown, and a block on its own, on a 1–5
> ability bar), death/respawn, territory capture, and fog of war (you see only
> what your faction's players and cells see, 3 cells around — `vision_radius`
> in the server config; explored ground stays dimmed, unexplored is covered) —
> end to end from the C++ server to the browser client.
> The server runs a fixed-timestep authoritative simulation; the client adds
> client-side prediction and interpolation for smooth play, plus a follow
> camera, HUD, and minimap.
> There are no accounts yet: a player joins with a name, and a session token
> brings the same character back after a disconnect — it stays in the world for
> a 30 s grace (the client keeps the token from GAME-009 on). The fuller
> faction/economy systems come later.

## Repository layout

```
protocol/   Protocol Buffers schema — the single source of truth for the wire
            format, shared by both sides (game/v1/protocol.proto)
server/     Authoritative game server — C++23, Boost.Asio/Beast WebSocket,
            coroutines, protobuf; runs the tick loop and the simulation
client/     Browser client — TypeScript + PixiJS + protobuf-es; renders the map
            and players, sends input
```

Each side has its own README with details:
[**server/README.md**](server/README.md) · [**client/README.md**](client/README.md).

## The shared protocol

`protocol/game/v1/protocol.proto` defines every `ClientMessage` / `ServerMessage`
and the game config. Both sides generate their bindings from it — the server via
CMake/`protoc`, the client via `buf` (`npm run generate`). On the wire, **one
WebSocket binary frame = one Protobuf message** (no custom framing). Change the
protocol in one place and regenerate on both sides.

Fog of war is enforced by the server: each snapshot carries only what the
recipient's faction sees, so hidden players and territory changes never reach
the client at all.

## Quick start

Build and run the server, then start the client and open it in a browser.
Needs GCC 13+ / CMake 3.21+ with Boost, OpenSSL and protobuf 36 (exact versions in
[server/README.md](server/README.md#prerequisites)) and Node 22+.

```bash
# 1) server (default ws://localhost:27998/)
cd server
scripts/install-protobuf.sh    # once: protobuf 36.2 into /usr/local, from source
cmake --preset debug-asan && cmake --build build -j
./build/bin/server config/config.json

# 2) client (in another terminal)
cd client
npm install
npm run dev          # regenerates protobuf-es -> src/gen, then serves; open the printed URL
```

For a public server, `npm run build` gives a `client/dist/` that connects to its
own origin at `/ws` (`wss` over https): serve it and the game server behind one
reverse proxy that forwards `/ws` to `127.0.0.1:27998` — or build with
`VITE_SERVER_URL=<address>` (see [client/README.md](client/README.md#server-address)).

**Controls:** WASD move · hold **E** capture the cell under you · hold **left
mouse** to use the active ability (**1** melee area attack, **2** ranged shot
toward the cursor, **3** block) · keys **1–5** pick the ability · click a faction
card, then a cell to spawn/respawn · **M** toggle the full-map view.

## Checks & CI

```bash
(cd server && ctest --preset debug-asan)   # server unit tests
(cd client && npm run check)               # client pure-logic checks
(cd client && npm run smoke)               # end-to-end, against a freshly started server
```

[GitHub Actions](.github/workflows/ci.yml) runs all three on every pull request
and every push to `main` — the smoke suite against the `debug-asan` server it has
just built. A red run blocks the merge once its `client` and `server` checks are
required for `main` (Settings → Branches → branch protection).

## Documentation

- Architecture & flow: [`server/docs`](server/docs) (sequence + ownership
  diagrams), [`client/docs`](client/docs) (client sequence diagram, design
  concept).

## License

[MIT](LICENSE).
