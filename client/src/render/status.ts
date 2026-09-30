import { LifeState } from "../gen/game/v1/protocol_pb.js";
import { usualHint } from "../hint.js";
import type { GameState } from "../state/gameState.js";
import type { AbilityBar } from "./abilityBar.js";
import type { FactionPicker } from "./factionPicker.js";
import type { Hud } from "./hud.js";
import type { SpawnButton } from "./spawnButton.js";

// The canvas UI around the world: HUD, ability bar, faction picker and the
// respawn button.
export interface StatusViews {
  hud: Hud;
  bar: AbilityBar;
  picker: FactionPicker;
  respawn: SpawnButton;
}

// Set the views up for a new game (after Welcome): the faction cards and the
// ability bar.
export function showWelcome(state: GameState, views: StatusViews): void {
  views.picker.setFactions(state.factions);
  views.picker.setSelected(state.selectedFaction);
  views.bar.setAbilities(state.abilities);
  views.bar.setActive(state.activeSlot);
}

// Bring the HUD, the bar, the picker and the respawn button up to date with the
// state (after each snapshot). A refused request shows on the hint line from here on; after a
// fatal error the bar and picker stay hidden.
export function showStatus(state: GameState, views: StatusViews, mapMode: boolean, nowMs: number): void {
  const { hud, bar, picker } = views;
  // In play: the faction of this life; otherwise the one picked for the next spawn.
  const shownFaction = state.alive ? state.myFaction : state.selectedFaction;
  const faction = state.factions.find((f) => f.id === shownFaction);
  if (faction) {
    hud.setFaction(faction.name, faction.color);
    bar.setAccent(faction.color);
  }
  const playing = !state.notices.failed;
  bar.setVisible(playing && state.alive);
  picker.setVisible(playing && state.choosingFaction); // once: never after a death
  // Once the faction is chosen, the way back into the world when it opens.
  const again = playing && !state.choosingFaction && state.maySpawn;
  views.respawn.show(again ? (state.life === LifeState.DEAD ? "Возродиться" : "В бой") : undefined);
  hud.setNetwork(state.roster.online, state.rttMs);
  const stats = state.territory.stats(shownFaction);
  hud.setTerritory(stats.cells, stats.percent);
  hud.setHp(state.hp, state.maxHp, state.alive);

  const cell = state.cellUnderMe();
  if (cell) {
    hud.setCell(
      cell.index,
      cell.owner !== 0 ? state.factionColor(cell.owner) : undefined,
      cell.captureFaction !== 0 ? state.factionColor(cell.captureFaction) : undefined,
      cell.captureProgress,
    );
  } else {
    hud.hideCell();
  }

  hud.setHint(state.notices.hint(playing ? usualHint(state, mapMode) : "", nowMs));
}

// A fatal error (or a close carrying one) ends the game here: clear the controls
// and show why, at once. Nothing to do for anything else.
export function showFailure(state: GameState, views: StatusViews, nowMs: number): void {
  if (!state.notices.failed) return;
  views.bar.setVisible(false);
  views.picker.setVisible(false);
  views.respawn.show(undefined);
  views.hud.setHint(state.notices.hint("", nowMs));
}
