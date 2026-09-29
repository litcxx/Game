import { LifeState } from "./gen/game/v1/protocol_pb.js";
import type { GameState } from "./state/gameState.js";

// The hint line's usual text (server errors take its place, see errors.ts):
// nothing while alive; otherwise how to (re)spawn, the respawn countdown, and
// the M key. `mapMode`: the full-map view is on.
export function usualHint(state: GameState, mapMode: boolean): string {
  if (state.alive) return "";
  const mapHint = `M — ${mapMode ? "к игроку" : "вся карта"}`;
  if (state.life === LifeState.DEAD) {
    const left = Math.max(0, Math.ceil((state.respawnTick - state.serverTick) / state.tickRate));
    return left > 0
      ? `Убит · возрождение через ${left}с · ${mapHint}`
      : `Убит · выберите фракцию и кликните по клетке — возрождение · ${mapHint}`;
  }
  return `Выберите фракцию и кликните по клетке — старт · ${mapHint}`;
}
