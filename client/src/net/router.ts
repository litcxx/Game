import { abilitiesFromWelcome } from "../abilities.js";
import { effectsFromEvents } from "../effects.js";
import { closeError, errorText } from "../errors.js";
import {
  LifeState,
  type MapState,
  type Roster,
  type ServerError,
  type ServerMessage,
  type Snapshot,
  type Welcome,
} from "../gen/game/v1/protocol_pb.js";
import { FIXED_DT, INTERP_DELAY_MS, type GameState } from "../state/gameState.js";
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
    default:
      break;
  }
}

// The socket closed: a close code 4000 + ErrorCode is fatal (the error frame
// usually came first; this covers a lost one).
export function routeClose(state: GameState, closeCode: number): void {
  const reason = closeError(closeCode);
  if (reason !== undefined) state.notices.fail(errorText(reason));
}

function onWelcome(state: GameState, w: Welcome): void {
  state.myId = w.playerId;
  state.mapWidth = w.config?.mapWidth ?? 0;
  state.mapHeight = w.config?.mapHeight ?? 0;
  state.maxHp = w.config?.maxHp ?? 100;
  state.tickRate = w.config?.tickRate || 60;
  state.setFactions(w.factions.map((f) => ({ id: f.id, name: f.name, color: f.color })));
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
  state.territory.setMapState(m.owners, m.captures);
  state.worldRevision++;
}

function onRoster(state: GameState, r: Roster): void {
  state.roster.apply(r.upsert, r.full, r.removed);
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

  // The local player: the server's word corrects the prediction.
  const self = s.players.find((p) => p.id === state.myId);
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

  const remotes = new Map<number, RemoteState>();
  for (const p of s.players) if (p.id !== state.myId) remotes.set(p.id, { x: p.x, y: p.y });
  state.interp.push(nowMs, remotes);

  const shots = new Map<number, RemoteState>();
  for (const p of s.projectiles) {
    shots.set(p.id, { x: p.x, y: p.y });
    state.shotMeta.set(p.id, { vx: p.vx, vy: p.vy, factionId: p.factionId, seenMs: nowMs });
  }
  state.shots.push(nowMs, shots);
  for (const [id, m] of state.shotMeta) if (nowMs - m.seenMs > SHOT_MEMORY_MS) state.shotMeta.delete(id);
}

function onError(state: GameState, e: ServerError, nowMs: number): void {
  if (e.fatal) state.notices.fail(errorText(e.code));
  else state.notices.refuse(errorText(e.code), nowMs);
}
