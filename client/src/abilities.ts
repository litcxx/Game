import { AbilityKind, type Ability } from "./gen/game/v1/protocol_pb.js";

// An ability on the 1–5 bar, as the client needs it (Welcome.abilities, bar order).
export interface AbilityInfo {
  id: number; // sent as InputFrame.ability
  kind: "melee" | "projectile" | "block";
  name: string;
  range: number; // units: melee radius / projectile flight distance
  cooldownTicks: number;
  projectileRadius: number; // units; 0 for melee
  durationTicks: number; // block: how long it lasts; 0 for attacks
}

export const BAR_SLOTS = 5;

// Map Welcome.abilities to the bar, keeping order; kinds this client doesn't know
// are skipped.
export function abilitiesFromWelcome(list: readonly Ability[]): AbilityInfo[] {
  const out: AbilityInfo[] = [];
  for (const a of list) {
    const kind =
      a.kind === AbilityKind.MELEE
        ? "melee"
        : a.kind === AbilityKind.PROJECTILE
          ? "projectile"
          : a.kind === AbilityKind.BLOCK
            ? "block"
            : undefined;
    if (kind === undefined) continue;
    out.push({
      id: a.id,
      kind,
      name: a.name,
      range: a.range,
      cooldownTicks: a.cooldownTicks,
      projectileRadius: a.projectileRadius,
      durationTicks: a.durationTicks,
    });
  }
  return out;
}

// Digit keys 1..5 — the top row or the numpad, by KeyboardEvent.code, so any
// layout — pick that bar slot (0-based index); an empty slot or any other key
// keeps the current one.
export function selectSlot(abilities: readonly AbilityInfo[], current: number, code: string): number {
  const digit = /^(?:Digit|Numpad)(\d)$/.exec(code);
  const n = digit ? Number(digit[1]) : 0;
  if (n < 1 || n > BAR_SLOTS || n > abilities.length) return current;
  return n - 1;
}

// Aim from the player toward the cursor (world units) as a unit vector x 1000,
// rounded for InputFrame.aim_x/aim_y.
export function aimVector(from: { x: number; y: number }, to: { x: number; y: number }): { x: number; y: number } {
  const dx = to.x - from.x;
  const dy = to.y - from.y;
  const len = Math.hypot(dx, dy);
  if (len < 1) return { x: 0, y: 0 }; // cursor on the player: no direction
  return { x: Math.round((dx / len) * 1000), y: Math.round((dy / len) * 1000) };
}

// Progress of the shared cooldown, for the ring's arc: 0 right after an ability was
// used, rising to 1 when the next use is ready. `lengthTicks` is the cooldown of the
// ability used last (0 before any attack: ready).
export function cooldownProgress(readyTick: number, nowTick: number, lengthTicks: number): number {
  if (lengthTicks <= 0) return 1;
  const remaining = Math.max(0, readyTick - nowTick);
  return Math.max(0, Math.min(1, 1 - remaining / lengthTicks));
}
