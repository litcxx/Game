import type { AbilityInfo } from "./abilities.js";
import type { GameEvent } from "./gen/game/v1/protocol_pb.js";

// Short visual effects driven by snapshot events, so everyone sees what others
// pressed: a melee "swing", a block's "shield", and a hit stopped by a block
// ("blocked"). Projectile launches need none — the projectile itself is drawn.
// A hit that landed ("hit": a flash and the damage on the target) shows only to
// whoever dealt it and whoever took it (hitTakenEffect); everyone else sees the hp
// bar drop, and a miss shows nothing.
export type EffectKind = "swing" | "shield" | "blocked" | "hit";

export interface Effect {
  kind: EffectKind;
  playerId: number; // whose token it is drawn on
  startMs: number; // performance.now() time it becomes visible
  durationMs: number;
  radius: number; // world units: the swing's reach; 0 otherwise
  damage: number; // hp the hit took; 0 otherwise
}

export const SWING_MS = 220;
export const BLOCKED_MS = 700;
export const HIT_MS = 600;

// Effects for one snapshot's events. Remote players are drawn `delayMs` in the
// past (interpolation), so their effects start `delayMs` after arrival and line
// up with what is on screen; the local player's start on arrival.
export function effectsFromEvents(
  events: readonly GameEvent[],
  abilities: readonly AbilityInfo[],
  selfId: number,
  arrivalMs: number,
  delayMs: number,
  tickMs: number,
): Effect[] {
  const out: Effect[] = [];
  const startFor = (id: number) => (id === selfId ? arrivalMs : arrivalMs + delayMs);
  for (const e of events) {
    if (e.kind.case === "ability") {
      const { playerId, abilityId } = e.kind.value;
      const a = abilities.find((x) => x.id === abilityId);
      if (a?.kind === "melee") {
        out.push({ kind: "swing", playerId, startMs: startFor(playerId), durationMs: SWING_MS, radius: a.range, damage: 0 });
      } else if (a?.kind === "block") {
        const durationMs = a.durationTicks * tickMs; // exactly while the block holds
        out.push({ kind: "shield", playerId, startMs: startFor(playerId), durationMs, radius: 0, damage: 0 });
      }
    } else if (e.kind.case === "hit") {
      const { attackerId, targetId, damage, blocked } = e.kind.value;
      const startMs = startFor(targetId);
      if (blocked) {
        out.push({ kind: "blocked", playerId: targetId, startMs, durationMs: BLOCKED_MS, radius: 0, damage: 0 });
      } else if (attackerId === selfId) {
        out.push({ kind: "hit", playerId: targetId, startMs, durationMs: HIT_MS, radius: 0, damage });
      }
    }
  }
  return out;
}

// A hit you took: the same flash and damage on your token as its dealer sees, when
// your hp dropped since the last snapshot. From your hp rather than HitEvent: it
// comes in every snapshot, a shot from the fog too (the server sends no event
// naming an attacker you can't see). Only damage lowers hp. It starts `delayMs`
// after arrival, like the dealer's: the attacker and its shot are drawn that far
// in the past, so the flash comes as the blow reaches your token on screen.
export function hitTakenEffect(
  prevHp: number,
  hp: number,
  selfId: number,
  arrivalMs: number,
  delayMs: number,
): Effect | undefined {
  if (hp >= prevHp) return undefined;
  return { kind: "hit", playerId: selfId, startMs: arrivalMs + delayMs, durationMs: HIT_MS, radius: 0, damage: prevHp - hp };
}

// Progress 0..1 of an effect at `nowMs`; undefined before it starts and once over.
export function effectProgress(effect: Effect, nowMs: number): number | undefined {
  if (nowMs < effect.startMs || nowMs >= effect.startMs + effect.durationMs) return undefined;
  return (nowMs - effect.startMs) / effect.durationMs;
}
