# Ranged Attack (Projectile) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> Executed inline by the author right after writing (the user asked to proceed
> straight to implementation; the design below was agreed in conversation). The
> exact code lives in the commits; this plan fixes the design, interfaces and
> the tests each task must pass.

**Goal:** Add the last MVP mechanic — a ranged attack: a dodgeable projectile
fired toward the mouse cursor — with a 1–5 ability bar, a shared cooldown and a
separate faction-picker UI.

**Architecture:** Abilities become data: the server config lists them
(`melee`, `projectile`), Welcome sends them, and `InputFrame` carries the
selected ability id plus an aim vector. A combat system applies the selected
ability when the attack key is held and the shared cooldown is ready (melee:
area hit via `SpatialIndex`; projectile: spawn). A projectile system moves
projectiles each tick and resolves hits with a swept segment-vs-circle test
against the spatial index; every hit goes through `apply_damage()`. The client
shows the ability bar, aims at the cursor, draws projectiles interpolated like
remote players, and picks the faction in its own UI.

**Tech Stack:** C++23 server (GoogleTest, protobuf), TypeScript + PixiJS 8 +
protobuf-es client, `tsx` check/smoke scripts.

**Spec:** the "Design" section below (agreed in conversation on 2026-09-27).

## Design

- **Abilities (server-defined).** `config.json` → `game.abilities[]`: `id`
  (≥ 1), `kind` (`"melee"` | `"projectile"`), `name`, `cooldown_ticks`,
  `damage`, `range`; projectiles also `projectile_speed` (units/s) and
  `projectile_radius` (units). Order = bar order (`abilities[0]` → key 1).
  Defaults: `1 melee "Удар" cd 45 dmg 20 range 120`;
  `2 projectile "Выстрел" cd 90 dmg 30 range 500 speed 800 radius 8`.
  `game.player_radius = 16` — body hit radius for projectiles.
  `attack_range` / `attack_cooldown_ticks` / `attack_damage` are removed from the
  config and from `GameConfig` in the protocol (field numbers reserved).
- **Input.** `InputFrame.ability` = `Ability.id` (0 = first ability, i.e.
  melee — keeps old clients/tests working); `aim_x`/`aim_y` = aim direction
  from the player's centre, any length (client sends a unit vector × 1000).
  Unknown ability id → no attack.
- **Shared cooldown.** One `attack_ready_tick` for all abilities; using an
  ability sets it to `tick + ability.cooldown_ticks` and records that length.
  `SelfState.attack_cooldown_ticks` reports the length so the client can draw
  the arc for variable cooldowns.
- **Projectile.** Spawns at the shooter's centre with velocity
  `normalize(aim) × speed`, flies `range` units, then despawns; also despawns on
  hit or when leaving the map. Owner id and faction are captured at fire time.
  Zero aim → no shot, cooldown not consumed.
- **Hits.** Each tick the projectile's step `p0 → p1` is tested against alive
  enemies (other faction, not the owner, alive right now) with a swept
  segment-vs-circle test, combined radius `player_radius + projectile_radius`;
  the first contact along the path wins, one target per projectile. Candidates
  come from the `SpatialIndex` (circle around the segment). No lag
  compensation — dodging is intended.
- **Tick order.** inputs → movement → index → attacks (melee + launches) →
  projectiles → capture → snapshot.
- **Snapshot.** `repeated ProjectileState projectiles` (`id, x, y, faction_id,
  vx, vy`) — the client interpolates them like remote players (same 100 ms
  delay, so a shot leaves the shooter's drawn position).
- **Client.** Keys 1–5 select the active slot (empty slots ignored; instant);
  the bar (bottom centre, visible while alive) shows 5 slots, active one glows.
  LMB held = use the active ability (min-hold 60 ms kept). The ring around the
  player shows the active ability's range with the cooldown arc; ranged also
  shows an aim line toward the cursor. Faction choice moves from digit keys to
  a picker panel (bottom centre while not alive).

## Global Constraints

- Server builds warning-free under `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wold-style-cast -Werror`, ASan/UBSan preset.
- No `auto [..] = co_await ...` when the result owns resources (see memory: GCC 16.1 leak).
- Proto: enum 0 = UNSPECIFIED; id 0 = "none/default"; hot fields keep numbers 1–15.
- Client hardcodes no rule numbers: everything comes from Welcome.
- Behaviour of existing features (melee, capture, prediction) must not change; all existing tests and smokes stay green.

## Review Focus

- Projectile fired point-blank into an overlapping enemy → hits on the launch tick (t = 0).
- Very fast projectile vs a target it would jump over in one step → still hits (swept test, not end-point test).
- Target killed by melee earlier in the same tick is on the projectile's path → projectile flies on (not consumed by a body).
- Projectile reaching its range exactly at a target's edge → the contact within the final partial step still counts.
- Cursor exactly on the player → zero aim → nothing fires, cooldown untouched.

---

## File Structure

Server (`server/`):
- Modify `../protocol/game/v1/protocol.proto` — `Ability`, `AbilityKind`, `ProjectileState`; `InputFrame.ability/aim_x/aim_y`; `Welcome.abilities`; `GameConfig.player_radius` (+ reserved 7, 8); `SelfState.attack_cooldown_ticks`; `Snapshot.projectiles`.
- Modify `src/shared/config/config.{hpp,cpp}`, `config/config.json` — `AbilityKind`, `AbilityConfig`, `GameConfig::abilities`, `player_radius`; drop `attack_*`.
- Modify `src/game/state/player.hpp` — `InputCommand`/`Player`: `ability`, `aim_x`, `aim_y`; `Player::cooldown_ticks`.
- Create `src/game/state/projectile.hpp` — `Projectile`.
- Modify `src/game/state/world_state.hpp` — `projectiles`, `next_projectile_id`.
- Create `src/game/spatial/geometry.hpp` — `segment_circle_hit()`.
- Modify `src/game/systems/input_system.cpp` — carry ability + aim.
- Modify `src/game/systems/combat_system.{hpp,cpp}` — `resolve_attacks()` (ability lookup, shared cooldown, melee, projectile launch).
- Create `src/game/systems/projectile_system.{hpp,cpp}` — `update_projectiles()`.
- Modify `src/game/sync/messages.cpp`, `src/game/sync/snapshot_builder.cpp`, `src/game/world/world.cpp`.
- Tests: create `test/unit/game/spatial/geometry_tests.cpp`, `test/unit/game/systems/projectile_system_tests.cpp`; modify `test/unit/game/world/world_tests.cpp`.

Client (`client/`):
- Create `src/abilities.ts` — ability model from Welcome, slot selection, aim vector (pure).
- Create `src/input/mouse.ts` — LMB hold (min-hold) + cursor tracking (moved out of `main.ts`).
- Create `src/render/abilityBar.ts`, `src/render/factionPicker.ts`, `src/render/projectiles.ts`.
- Modify `src/net/prediction.ts`, `src/net/client.ts` (frames carry ability + aim), `src/render/scene.ts` (active-ability ring, aim line, variable cooldown, projectiles), `src/main.ts` (wiring).
- Create `scripts/abilities_check.ts` (pure logic), `scripts/ranged_smoke.ts` (e2e).

## Tasks

### Task 1: Data-driven abilities in protocol and config (melee unchanged)

**Interfaces — Produces:** `lit::AbilityKind { Melee, Projectile }`;
`lit::AbilityConfig { id, kind, name, cooldown_ticks, damage, range, projectile_speed, projectile_radius }`;
`GameConfig::abilities`, `GameConfig::player_radius`; `Welcome.abilities`,
`GameConfig.player_radius` on the wire; melee reads its numbers from the first ability.

- [ ] Write failing test `WorldHello.WelcomeCarriesAbilitiesAndPlayerRadius` (both abilities with every field; `config().player_radius()`); update `test_config()` to define abilities instead of `attack_*`.
- [ ] Run — fails to compile/assert.
- [ ] Proto changes; C++ config structs + JSON parsing (`kind` string → enum, unknown → throw); `config.json`; `make_welcome` fills abilities; melee temporarily uses `abilities.front()`.
- [ ] Run full suite — all pass (melee tests unchanged in intent).
- [ ] Regenerate client bindings (`npm run generate`); client keeps compiling (ring range from the first ability); `npm run typecheck`.
- [ ] Commit.

### Task 2: Swept segment-vs-circle geometry

**Interfaces — Produces:** `std::optional<double> segment_circle_hit(double x0, double y0, double x1, double y1, double cx, double cy, double r)` — smallest `t ∈ [0, 1]` where the moving point is within `r` of the centre (inclusive); `0` if it starts inside; `nullopt` on a miss.

Tests (literal expectations): path through centre `(0,0)→(100,0)`, circle `(50,0) r10` → `0.4`; passes beside `(50,20) r10` → miss; tangent `(50,10) r10` → `0.5`; starts inside `(0,0) r10` → `0`; circle beyond end `(150,0) r10` → miss; zero-length segment inside → `0`, outside → miss.

- [ ] Write failing tests; run (fail); implement; run (pass); commit.

### Task 3: Ability selection and shared cooldown

**Interfaces — Consumes:** Task 1 config. **Produces:** `InputCommand`/`Player` fields `ability`, `aim_x`, `aim_y`; `Player::cooldown_ticks`; `resolve_attacks(WorldState&, const GameConfig&, const SpatialIndex&)` replaces `resolve_melee`; `SelfState.attack_cooldown_ticks`.

Tests (World, end to end): `WorldAbility.DefaultAbilityIsMelee` (ability 0 swings melee); `WorldAbility.SelfStateReportsCooldownLength` (after a swing = melee cooldown); `WorldAbility.UnknownAbilityDoesNothing` (no hit, cooldown untouched).

- [ ] Write failing tests; run; implement; run full suite; commit.

### Task 4: Projectiles — launch, flight, swept hits, snapshot

**Interfaces — Produces:** `lit::game::Projectile { id, owner_id, faction_id, damage, radius, x, y, vx, vy, remaining }`; `WorldState::projectiles`, `next_projectile_id`; `update_projectiles(WorldState&, const GameConfig&, const SpatialIndex&, double dt)`; `Snapshot.projectiles`.

Unit tests (`Projectiles.*`, driving `update_projectiles` over a hand-built `WorldState` + index): flies straight at its speed; despawns after its range; despawns when leaving the map; hits the first enemy on its path (damage via `apply_damage`, attacker = owner, removed); passes allies and its owner; fast projectile does not tunnel; ignores bodies (dead this tick); one target per projectile; point-blank overlap hits at once; contact within the last partial step counts.

World tests (`WorldRanged.*`): fires toward the aim (snapshot projectile `vx > 0, vy == 0`, shooter's faction); hits an enemy for the ranged damage; no aim → no projectile and cooldown untouched; a target moving out of the line is missed (dodge); shared cooldown: after a shot, melee can't swing until `ready_tick`, and `attack_cooldown_ticks` = ranged cooldown.

- [ ] Write failing tests; run; implement state + launch + system + snapshot; run full suite; commit.

### Task 5: Client — abilities, aim and input plumbing

**Interfaces — Produces (`src/abilities.ts`):** `interface AbilityInfo { id; kind: "melee" | "projectile"; name; range; cooldownTicks }`; `abilitiesFromWelcome(list)`; `selectSlot(abilities, current, key) → number` (empty slot → unchanged); `aimVector(from, to) → { x, y }` (unit × 1000, rounded; `(0,0)` when closer than 1 unit). `InputSample` gains `ability`, `aimX`, `aimY`; `sendInputFrames` sends them. `src/input/mouse.ts`: `installMouse(canvas, canAttack) → { attacking(): boolean; cursor(): {x, y} | undefined }`.

- [ ] Write `scripts/abilities_check.ts` with literal cases; run (fails: module missing); implement; run (PASS); typecheck; commit.

### Task 6: Client — ability bar, active-ability ring, aim line, variable cooldown

- [ ] `AbilityBar` (5 slots, key digits, kind icons ◆ melee / ● projectile, active glow, visible while alive); keys 1–5 select; ring radius = active ability range; cooldown arc uses `SelfState.attack_cooldown_ticks`; projectile ability draws an aim line (length = range). Typecheck; manual check in the browser; commit.

### Task 7: Client — faction picker

- [ ] `FactionPicker` (cards: diamond badge + name, selected card gold-bordered; bottom centre while not alive); canvas clicks on a card select the faction instead of spawning; digits no longer pick factions; hint text updated. Typecheck; commit.

### Task 8: Client — projectile rendering

- [ ] `ProjectileView` draws each projectile (faction colour, radius = `projectile_radius × scale` (≥ 2.5 px), short trail along velocity); positions from a second `InterpolationBuffer` keyed by projectile id. Typecheck; commit.

### Task 9: End-to-end smoke, docs, final verification

- [ ] `scripts/ranged_smoke.ts`: shooter + target 3 cells apart, different factions; shooter fires ability 2 toward the target → a projectile appears in snapshots, then a HitEvent with the ranged damage and the target's hp drops. Run against a fresh server.
- [ ] All unit tests, every smoke/check, client typecheck, ASan clean shutdown, stress harness.
- [ ] Docs: protocol comments, server README + sequence diagram (tick order, projectiles, abilities), client README + root README (controls: 1–5 abilities, faction picker, aim with the mouse).
- [ ] Commit; offer merge.
