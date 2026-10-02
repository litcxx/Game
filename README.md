# Game

[![CI](https://github.com/litcxx/Game/actions/workflows/ci.yml/badge.svg)](https://github.com/litcxx/Game/actions/workflows/ci.yml)

A minimalist top-down multiplayer browser game. Players join a shared **100×100
grid**, pick a faction, move around, fight, and capture territory painted in
their faction's colour. The **server is authoritative** — it owns all state and
runs the simulation; the browser client sends intent, predicts its own player,
interpolates the others, and renders.

> **Status: MVP, work in progress.** Working today: connection/presence,
> faction choice (once a season), a capital per faction in the map's corners
> (its zone of 49 cells is the faction's from the start, its marker seen through
> the fog), spawn, movement, combat (a melee area attack and a dodgeable
> ranged projectile on a shared cooldown, and a block on its own, on a 1–5
> ability bar), death/respawn, territory capture, and fog of war (you see only
> what your faction's players and cells see, 3 cells around — `vision_radius`
> in the server config; explored ground stays dimmed, unexplored is covered) —
> end to end from the C++ server to the browser client.
> The server runs a fixed-timestep authoritative simulation; the client adds
> client-side prediction and interpolation for smooth play, plus a follow
> camera, HUD, and minimap.
> There are no accounts yet: a player picks a nickname, and a session token
> (kept in the browser with it) brings the same character back after a
> disconnect — the client reconnects by itself, and the character stays in the
> world for a 30 s grace. The fuller faction/economy systems come later.

## Repository layout

```
protocol/   Protocol Buffers schema — the single source of truth for the wire
            format, shared by both sides (game/v1/protocol.proto)
server/     Authoritative game server — C++23, Boost.Asio/Beast WebSocket,
            coroutines, protobuf; runs the tick loop and the simulation
client/     Browser client — TypeScript + PixiJS + protobuf-es; renders the map
            and players, sends input
deploy/     Running it on a VPS: setup and deploy scripts, the systemd service,
            the Caddy site (HTTPS, the client, /ws to the server)
docs/       The game design document (GDD.md), operations (ops.md), playtests
```

Each side has its own README with details:
[**server/README.md**](server/README.md) · [**client/README.md**](client/README.md);
running it on a VPS: [**docs/ops.md**](docs/ops.md).

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
Needs GCC 16.2 / CMake 3.21+ with Boost, OpenSSL and protobuf 36 (exact versions in
[server/README.md](server/README.md#prerequisites)) and Node 22+.

```bash
# 1) server (default ws://localhost:27998/)
cd server
scripts/install-gcc.sh         # once: GCC 16.2 into /opt/gcc-16.2.0, as g++-16 (Ubuntu 24.04)
scripts/install-protobuf.sh    # once: protobuf 36.2 into /usr/local, from source
cmake --preset debug-asan && cmake --build build -j
./build/bin/server config/config.json   # saves the world to server/saves; --fresh: a new one

# 2) client (in another terminal)
cd client
npm install
npm run dev          # regenerates protobuf-es -> src/gen, then serves; open the printed URL
```

For a public server, see [docs/ops.md](docs/ops.md): on a fresh Ubuntu 24.04
VPS, `sudo deploy/setup.sh <domain>` once, then `deploy/deploy.sh` for every
release — the game at `https://<domain>/`, Caddy in front (HTTPS, the client,
`/ws` to the server), the server as a systemd service that restarts by itself.

**Controls:** WASD move · hold **E** capture the cell under you (one next to your
faction's land; an enemy's takes twice as long) · hold **left
mouse** to use the active ability (**1** melee area attack, **2** ranged shot
toward the cursor, **3** block) · keys **1–5** pick the ability · click a faction
card (once: it is yours until the season changes), then **«В бой»** — you come
into the world at your faction's capital; after a death **«Возродиться»** brings
you back there · **M** toggle the full-map view. Keys work
by their place on the keyboard, in any layout.

## Checks & CI

```bash
(cd server && ctest --preset debug-asan)   # server unit tests
(cd client && npm run check)               # client pure-logic checks
(cd client && npm run smoke)               # end-to-end, against a fresh server on the smoke map
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
