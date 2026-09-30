import { cooldownProgress, type AbilityInfo } from "../abilities.js";
import type { Effect } from "../effects.js";
import { Notices } from "../errors.js";
import { FogOfWar } from "../fog.js";
import { LifeState } from "../gen/game/v1/protocol_pb.js";
import { InterpolationBuffer } from "../net/interpolation.js";
import type { Predictor } from "../net/prediction.js";
import { SnapshotClock } from "../net/snapshotClock.js";
import { Roster } from "./roster.js";
import { Territory, UNITS_PER_CELL, type CellState } from "./territory.js";

export const FIXED_DT = 1 / 60;
// Remote players are drawn this far in the past (~2 snapshots) to interpolate.
export const INTERP_DELAY_MS = 100;
// Your own shots run on from the latest snapshot by their velocity, at most this
// far (late snapshots don't send them off ahead).
export const OWN_SHOT_LEAD_MS = 100;
// A projectile that flew out of sight (the server stops sending it) flies on,
// fading out over this long, instead of stopping at the fog as at a wall.
export const FADE_MS = 200;

export interface Faction {
  id: number;
  name: string;
  color: number;
}

// A player's body in the latest snapshot.
export interface Body {
  x: number;
  y: number;
  hp: number; // 0 for a dead body
}

// A projectile's velocity and faction from the newest snapshot that had it; its
// position is interpolated (GameState.shots).
export interface ShotMeta {
  vx: number;
  vy: number;
  factionId: number;
  seenMs: number;
}

// Your own projectile in the latest snapshot (ProjectileState.mine), and when
// that snapshot came.
export interface OwnShot {
  x: number;
  y: number;
  vx: number;
  vy: number;
  factionId: number;
  seenMs: number;
}

// A projectile to draw now.
export interface ShotView {
  x: number;
  y: number;
  vx: number;
  vy: number;
  factionId: number;
  alpha: number; // 1 in flight; less while fading out of sight
}

// A projectile last seen where it was about to leave sight: from there it flies on
// and fades out. `atMs`: when it was there — yours on the local clock (arrival),
// others' on the snapshots' clock (SnapshotClock), drawn INTERP_DELAY_MS behind.
export interface FadingShot {
  x: number;
  y: number;
  vx: number;
  vy: number;
  factionId: number;
  atMs: number;
  own: boolean;
}

// Everything the client knows, as data: what the server said (net/router.ts
// writes it), the player's local choices, and the client's own simulation
// (prediction, interpolation). No PixiJS and no socket here: views read it,
// input writes the choices.
export class GameState {
  // --- From Welcome
  myId = 0;
  mapWidth = 0;
  mapHeight = 0;
  maxHp = 100;
  tickRate = 60;
  factions: readonly Faction[] = []; // in Welcome order (the picker's cards); see setFactions
  abilities: AbilityInfo[] = []; // the ability bar, slot 1 first
  projectileRadius = 0; // units, of the projectile ability
  predictor: Predictor | undefined; // the local player's movement, predicted

  // --- Local choices
  activeSlot = 0; // ability bar slot in use (keys 1–5)
  selectedFaction = 1; // picked on the cards, for the first spawn
  myFaction = 0; // locked at the first spawn for the season (the server refuses another); 0 until then
  captureLearned = false; // captured a cell once: no capture hint (onboarding.ts); outlives sessions

  // --- From snapshots
  life: LifeState = LifeState.NOT_SPAWNED;
  hp = 0;
  respawnTick = 0;
  attackReadyTick = 0; // the shared attack cooldown: ready at, length
  attackCooldownTicks = 0;
  blockReadyTick = 0; // the block's own cooldown
  blockCooldownTicks = 0;
  serverTick = 0;
  serverTickAtMs = 0; // performance.now() when serverTick arrived
  rttMs: number | undefined; // the last Ping's round trip; unknown before its Pong
  readonly players = new Map<number, Body>(); // everyone in the last snapshot
  readonly territory = new Territory();
  readonly roster = new Roster();
  readonly fog = new FogOfWar();
  // Bumped whenever the territory or the sight changes: the world view redraws.
  worldRevision = 0;
  readonly interp = new InterpolationBuffer(INTERP_DELAY_MS); // remote players
  readonly shots = new InterpolationBuffer(INTERP_DELAY_MS); // others' projectiles
  readonly shotMeta = new Map<number, ShotMeta>();
  ownShots = new Map<number, OwnShot>(); // yours, from the latest snapshot
  readonly fadingShots = new Map<number, FadingShot>(); // gone out of sight, fading
  lastOtherShots = new Map<number, Omit<FadingShot, "atMs" | "own">>(); // others', in the last snapshot
  lastShotsStamp = 0; // that snapshot's place on the snapshots' clock
  readonly clock = new SnapshotClock(); // where snapshots go on the interpolation's clock
  readonly notices = new Notices(); // server errors for the hint line
  private effects: Effect[] = []; // new ones, for the view to take
  private readonly colors = new Map<number, number>(); // faction id -> colour

  // A new session (after Welcome, a reconnect too): forget the last one's life,
  // bodies, projectiles, effects and errors — the server sends them anew.
  startSession(): void {
    this.life = LifeState.NOT_SPAWNED;
    this.hp = 0;
    this.respawnTick = 0;
    this.attackReadyTick = this.attackCooldownTicks = 0;
    this.blockReadyTick = this.blockCooldownTicks = 0;
    this.rttMs = undefined;
    this.myFaction = 0; // the roster tells it for a resumed body
    this.players.clear();
    this.interp.clear();
    this.shots.clear();
    this.shotMeta.clear();
    this.ownShots.clear();
    this.fadingShots.clear();
    this.lastOtherShots.clear();
    this.clock.reset();
    this.effects = [];
    this.notices.clear();
  }

  get alive(): boolean {
    return this.life === LifeState.ALIVE;
  }

  // The faction is still to be chosen: not alive and never spawned. The choice
  // shows only then — once, not after a death.
  get choosingFaction(): boolean {
    return !this.alive && this.myFaction === 0;
  }

  // The faction to (re)spawn in: the locked one, or — at the first spawn — the
  // one picked, which this locks.
  spawnFaction(): number {
    if (this.myFaction === 0) {
      this.myFaction = this.selectedFaction;
      this.roster.setFaction(this.myId, this.myFaction); // the roster doesn't echo our own
    }
    return this.myFaction;
  }

  get activeAbility(): AbilityInfo | undefined {
    return this.abilities[this.activeSlot];
  }

  // The cell under the local player (at the predicted position); none unless alive.
  cellUnderMe(): CellState | undefined {
    if (!this.alive || !this.predictor) return undefined;
    const { x, y } = this.predictor.position;
    return this.territory.cell(Math.floor(x / UNITS_PER_CELL), Math.floor(y / UNITS_PER_CELL));
  }

  setFactions(factions: readonly Faction[]): void {
    this.factions = factions;
    this.colors.clear();
    for (const f of factions) this.colors.set(f.id, f.color);
  }

  factionColor(id: number): number | undefined {
    return this.colors.get(id);
  }

  // The projectiles to draw at `nowMs`. Others' are interpolated INTERP_DELAY_MS
  // in the past, in step with the players who fired them. Yours run on from the
  // latest snapshot by their velocity (at most OWN_SHOT_LEAD_MS), so a shot
  // leaves your token — drawn at the predicted position — along the aim line
  // rather than behind it. A shot gone from the snapshots in sight (a hit, the
  // end of its range) is gone; one that flew out of sight flies on from where it
  // was last seen, fading out over FADE_MS (router.ts tells which is which).
  shotsAt(nowMs: number): ShotView[] {
    const out: ShotView[] = [];
    const renderMs = nowMs - INTERP_DELAY_MS; // others' shots are drawn this far behind
    for (const id of this.shots.ids(nowMs)) {
      const fading = this.fadingShots.get(id);
      if (fading && renderMs >= fading.atMs) continue; // it fades on from here
      const pos = this.shots.sample(id, nowMs);
      const meta = this.shotMeta.get(id);
      if (pos && meta) out.push({ ...pos, vx: meta.vx, vy: meta.vy, factionId: meta.factionId, alpha: 1 });
    }
    for (const s of this.ownShots.values()) {
      const lead = Math.min(Math.max(0, nowMs - s.seenMs), OWN_SHOT_LEAD_MS) / 1000;
      out.push({ x: s.x + s.vx * lead, y: s.y + s.vy * lead, vx: s.vx, vy: s.vy, factionId: s.factionId, alpha: 1 });
    }
    for (const f of this.fadingShots.values()) {
      const age = (f.own ? nowMs : renderMs) - f.atMs;
      if (age < 0 || age >= FADE_MS) continue;
      const t = age / 1000;
      out.push({ x: f.x + f.vx * t, y: f.y + f.vy * t, vx: f.vx, vy: f.vy, factionId: f.factionId, alpha: 1 - age / FADE_MS });
    }
    return out;
  }

  // The server tick now, advancing smoothly between snapshots.
  currentTick(nowMs: number): number {
    return this.serverTick + ((nowMs - this.serverTickAtMs) / 1000) * this.tickRate;
  }

  // Cooldown progress (0 just used .. 1 ready): the shared attack one and the
  // block's own.
  cooldownProgress(nowMs: number): { attack: number; block: number } {
    const now = this.currentTick(nowMs);
    return {
      attack: cooldownProgress(this.attackReadyTick, now, this.attackCooldownTicks),
      block: cooldownProgress(this.blockReadyTick, now, this.blockCooldownTicks),
    };
  }

  // Everyone in the last snapshot but the local player.
  remoteIds(): number[] {
    return [...this.players.keys()].filter((id) => id !== this.myId);
  }

  addEffects(effects: readonly Effect[]): void {
    this.effects.push(...effects);
  }

  // The effects added since the last call.
  takeEffects(): Effect[] {
    const taken = this.effects;
    this.effects = [];
    return taken;
  }
}
