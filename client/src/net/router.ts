import { abilitiesFromWelcome } from "../abilities.js";
import { effectsFromEvents, hitTakenEffect } from "../effects.js";
import { errorText } from "../errors.js";
import {
  LifeState,
  type MapState,
  type Pong,
  type Roster,
  type ServerError,
  type ServerMessage,
  type Snapshot,
  type Welcome,
} from "../gen/game/v1/protocol_pb.js";
import { FADE_MS, FIXED_DT, INTERP_DELAY_MS, type GameState } from "../state/gameState.js";
import { UNITS_PER_CELL } from "../state/territory.js";
import { roundTripMs } from "./client.js";
import type { RemoteState } from "./interpolation.js";
import { Predictor } from "./prediction.js";

// A projectile not seen for this long is forgotten.
const SHOT_MEMORY_MS = 2000;

// Apply one ServerMessage to the game state, one handler per kind. Pure data:
// the views read the state afterwards. `nowMs` is performance.now().
export function routeMessage(state: GameState, msg: ServerMessage, nowMs: number): void {
  switch (msg.payload.case) {
    case "welcome":
      onWelcome(state, msg.payload.value);
      break;
    case "mapState":
      onMapState(state, msg.payload.value);
      break;
    case "roster":
      onRoster(state, msg.payload.value);
      break;
    case "snapshot":
      onSnapshot(state, msg.payload.value, nowMs);
      break;
    case "error":
      onError(state, msg.payload.value, nowMs);
      break;
    case "pong":
      onPong(state, msg.payload.value, nowMs);
      break;
    default:
      break;
  }
}

// Welcome starts a session — the first one or a reconnect.
function onWelcome(state: GameState, w: Welcome): void {
  state.startSession();
  state.myId = w.playerId;
  state.mapWidth = w.config?.mapWidth ?? 0;
  state.mapHeight = w.config?.mapHeight ?? 0;
  state.maxHp = w.config?.maxHp ?? 100;
  state.tickRate = w.config?.tickRate || 60;
  state.setFactions(w.factions.map((f) => ({ id: f.id, name: f.name, color: f.color })));
  state.capitals = (w.map?.capitals ?? []).map((c) => ({ factionId: c.factionId, cell: c.cell, protectedRadius: c.protectedRadius }));
  state.selectedFaction = w.factions[0]?.id ?? 1;
  state.abilities = abilitiesFromWelcome(w.abilities);
  state.projectileRadius = state.abilities.find((a) => a.kind === "projectile")?.projectileRadius ?? 0;
  state.activeSlot = 0;
  if (w.config) {
    state.territory.resize(w.config.mapWidth, w.config.mapHeight);
    state.fog.reset(state.territory.cols, state.territory.rows); // a new map: nothing explored yet
    state.worldRevision++;
  }
  const predictor = new Predictor(w.config?.moveSpeed ?? 300, FIXED_DT, {
    maxX: state.mapWidth * 100 - 1,
    maxY: state.mapHeight * 100 - 1,
  });
  predictor.reset({ x: (state.mapWidth * 100) / 2, y: (state.mapHeight * 100) / 2 });
  state.predictor = predictor;
}

function onMapState(state: GameState, m: MapState): void {
  state.territory.setMapState(m.ownerFactionIds, m.captures);
  state.worldRevision++;
}

function onRoster(state: GameState, r: Roster): void {
  state.roster.apply(r.upsert, r.full, r.removedPlayerIds);
  // A resumed character is back with its faction: the full roster after Welcome
  // is the first to tell it.
  const mine = state.roster.factionOf(state.myId);
  if (r.full && state.myFaction === 0 && mine !== 0) state.myFaction = state.selectedFaction = mine;
}

function onSnapshot(state: GameState, s: Snapshot, nowMs: number): void {
  const wasAlive = state.alive;
  state.serverTick = s.tick;
  state.serverTickAtMs = nowMs;
  state.life = s.you?.life ?? LifeState.NOT_SPAWNED;
  state.respawnTick = s.you?.respawnTick ?? 0;
  state.attackReadyTick = s.you?.attackReadyTick ?? 0;
  state.attackCooldownTicks = s.you?.attackCooldownTicks ?? 0;
  state.blockReadyTick = s.you?.blockReadyTick ?? 0;
  state.blockCooldownTicks = s.you?.blockCooldownTicks ?? 0;

  // The local player: the server's word corrects the prediction; a drop of your
  // hp flashes your token with the damage.
  const self = s.players.find((p) => p.id === state.myId);
  const taken = self && hitTakenEffect(state.hp, self.hp, state.myId, nowMs, INTERP_DELAY_MS);
  if (taken) state.addEffects([taken]);
  state.hp = self?.hp ?? 0;
  if (state.predictor && self) {
    if (state.alive && wasAlive) state.predictor.reconcile({ x: self.x, y: self.y }, s.you?.lastInputSeq ?? 0);
    else state.predictor.reset({ x: self.x, y: self.y });
  }

  // Everyone in the snapshot; the ones gone from it are dropped.
  const seen = new Set<number>();
  for (const p of s.players) {
    seen.add(p.id);
    state.players.set(p.id, { x: p.x, y: p.y, hp: p.hp });
  }
  for (const id of [...state.players.keys()]) if (!seen.has(id)) state.players.delete(id);

  // What others (and you) pressed: swings, blocks, blocked hits.
  state.addEffects(
    effectsFromEvents(s.events, state.abilities, state.myId, nowMs, INTERP_DELAY_MS, 1000 / state.tickRate),
  );

  // Fog of war: cells entering / leaving sight; revealed ones come with their
  // state. A resync (a frame to us was lost) re-reveals the whole sight.
  const forgot = s.resync && state.fog.forgetSight();
  const sightChanged = state.fog.apply(s.revealed, s.hidden) || forgot;
  const cellsChanged = state.territory.applyCellUpdates(s.cells);
  if (sightChanged || cellsChanged) state.worldRevision++;

  // Remote players and their projectiles go on the snapshots' clock: the server's
  // time, not the arrival's, so network jitter doesn't make their motion uneven.
  const stamp = state.clock.stamp((s.tick * 1000) / state.tickRate, nowMs);
  const remotes = new Map<number, RemoteState>();
  for (const p of s.players) if (p.id !== state.myId) remotes.set(p.id, { x: p.x, y: p.y });
  state.interp.push(stamp, remotes);

  // Projectiles: yours run on from this snapshot (GameState.shotsAt), others'
  // are interpolated.
  const lastOwn = state.ownShots;
  const lastOthers = state.lastOtherShots;
  const lastStamp = state.lastShotsStamp;
  const shots = new Map<number, RemoteState>();
  state.ownShots = new Map();
  state.lastOtherShots = new Map();
  state.lastShotsStamp = stamp;
  for (const p of s.projectiles) {
    const shot = { x: p.x, y: p.y, vx: p.vx, vy: p.vy, factionId: p.factionId };
    if (p.mine) {
      state.ownShots.set(p.id, { ...shot, seenMs: nowMs });
      continue;
    }
    shots.set(p.id, { x: p.x, y: p.y });
    state.lastOtherShots.set(p.id, shot);
    state.shotMeta.set(p.id, { vx: p.vx, vy: p.vy, factionId: p.factionId, seenMs: nowMs });
  }
  state.shots.push(stamp, shots);
  for (const [id, m] of state.shotMeta) if (nowMs - m.seenMs > SHOT_MEMORY_MS) state.shotMeta.delete(id);

  // Gone since the last snapshot: one that would now be out of sight flew out of
  // it (the server sends only what is in sight), so it fades on; one in sight hit
  // or ran its range, and is simply gone.
  for (const [id, p] of lastOwn) {
    if (!state.ownShots.has(id) && outOfSight(state, p, nowMs - p.seenMs)) {
      state.fadingShots.set(id, { ...p, atMs: p.seenMs, own: true });
    }
  }
  for (const [id, p] of lastOthers) {
    if (!state.lastOtherShots.has(id) && outOfSight(state, p, stamp - lastStamp)) {
      state.fadingShots.set(id, { ...p, atMs: lastStamp, own: false });
    }
  }
  for (const [id, f] of state.fadingShots) {
    if ((f.own ? nowMs : stamp) - f.atMs > FADE_MS + INTERP_DELAY_MS) state.fadingShots.delete(id);
  }
}

// Whether a projectile, `dtMs` on from where it was, is out of the faction's sight
// (or off the map).
function outOfSight(state: GameState, p: { x: number; y: number; vx: number; vy: number }, dtMs: number): boolean {
  const col = Math.floor((p.x + (p.vx * dtMs) / 1000) / UNITS_PER_CELL);
  const row = Math.floor((p.y + (p.vy * dtMs) / 1000) / UNITS_PER_CELL);
  const { cols, rows } = state.territory;
  if (col < 0 || row < 0 || col >= cols || row >= rows) return true;
  return state.fog.sightAt(row * cols + col) !== "visible";
}

// The round trip of the Ping it answers (shown in the HUD).
function onPong(state: GameState, p: Pong, nowMs: number): void {
  state.rttMs = roundTripMs(p.clientTimeMs, nowMs);
}

function onError(state: GameState, e: ServerError, nowMs: number): void {
  if (e.fatal) state.notices.fail(errorText(e.code));
  else state.notices.refuse(errorText(e.code), nowMs);
}
