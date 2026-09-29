import { cooldownProgress, type AbilityInfo } from "../abilities.js";
import type { Effect } from "../effects.js";
import { Notices } from "../errors.js";
import { FogOfWar } from "../fog.js";
import { LifeState } from "../gen/game/v1/protocol_pb.js";
import { InterpolationBuffer } from "../net/interpolation.js";
import type { Predictor } from "../net/prediction.js";
import { Roster } from "./roster.js";
import { Territory } from "./territory.js";

export const FIXED_DT = 1 / 60;
// Remote players are drawn this far in the past (~2 snapshots) to interpolate.
export const INTERP_DELAY_MS = 100;

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
  selectedFaction = 1; // for the next spawn
  myFaction = 0; // of the current life

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
  readonly players = new Map<number, Body>(); // everyone in the last snapshot
  readonly territory = new Territory();
  readonly roster = new Roster();
  readonly fog = new FogOfWar();
  // Bumped whenever the territory or the sight changes: the world view redraws.
  worldRevision = 0;
  readonly interp = new InterpolationBuffer(INTERP_DELAY_MS); // remote players
  readonly shots = new InterpolationBuffer(INTERP_DELAY_MS); // projectiles
  readonly shotMeta = new Map<number, ShotMeta>();
  readonly notices = new Notices(); // server errors for the hint line
  private effects: Effect[] = []; // new ones, for the view to take
  private readonly colors = new Map<number, number>(); // faction id -> colour

  get alive(): boolean {
    return this.life === LifeState.ALIVE;
  }

  get activeAbility(): AbilityInfo | undefined {
    return this.abilities[this.activeSlot];
  }

  setFactions(factions: readonly Faction[]): void {
    this.factions = factions;
    this.colors.clear();
    for (const f of factions) this.colors.set(f.id, f.color);
  }

  factionColor(id: number): number | undefined {
    return this.colors.get(id);
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
