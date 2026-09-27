import type { AbilityInfo } from "./abilities.js";
import type { GameEvent } from "./gen/game/v1/protocol_pb.js";

// Short visual effects driven by snapshot events, so everyone sees what others
// pressed: a melee "swing", a block's "shield", and a hit stopped by a block
// ("blocked"). Projectile launches need none — the projectile itself is drawn.
export type EffectKind = "swing" | "shield" | "blocked";

export interface Effect {
  kind: EffectKind;
  playerId: number; // whose token it is drawn on
  startMs: number; // performance.now() time it becomes visible
  durationMs: number;
  radius: number; // world units: the swing's reach; 0 otherwise
}

export const SWING_MS = 220;
export const BLOCKED_MS = 700;

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
        out.push({ kind: "swing", playerId, startMs: startFor(playerId), durationMs: SWING_MS, radius: a.range });
      } else if (a?.kind === "block") {
        const durationMs = a.durationTicks * tickMs; // exactly while the block holds
        out.push({ kind: "shield", playerId, startMs: startFor(playerId), durationMs, radius: 0 });
      }
    } else if (e.kind.case === "hit" && e.kind.value.blocked) {
      const id = e.kind.value.targetId;
      out.push({ kind: "blocked", playerId: id, startMs: startFor(id), durationMs: BLOCKED_MS, radius: 0 });
    }
  }
  return out;
}

// Progress 0..1 of an effect at `nowMs`; undefined before it starts and once over.
export function effectProgress(effect: Effect, nowMs: number): number | undefined {
  if (nowMs < effect.startMs || nowMs >= effect.startMs + effect.durationMs) return undefined;
  return (nowMs - effect.startMs) / effect.durationMs;
}
