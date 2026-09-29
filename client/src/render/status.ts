import { usualHint } from "../hint.js";
import type { GameState } from "../state/gameState.js";
import type { AbilityBar } from "./abilityBar.js";
import { UNITS_PER_CELL } from "./camera.js";
import type { FactionPicker } from "./factionPicker.js";
import type { Hud } from "./hud.js";

// The canvas UI around the world: HUD, ability bar and faction picker.
export interface StatusViews {
  hud: Hud;
  bar: AbilityBar;
  picker: FactionPicker;
}

// Set the views up for a new game (after Welcome): the faction cards and the
// ability bar.
export function showWelcome(state: GameState, views: StatusViews): void {
  views.picker.setFactions(state.factions);
  views.picker.setSelected(state.selectedFaction);
  views.bar.setAbilities(state.abilities);
  views.bar.setActive(state.activeSlot);
}

// Bring the HUD, the bar and the picker up to date with the state (after each
// snapshot). A refused request shows on the hint line from here on; after a
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
  picker.setVisible(playing && !state.alive);
  hud.setOnline(state.roster.online);
  const stats = state.territory.stats(shownFaction);
  hud.setTerritory(stats.cells, stats.percent);
  hud.setHp(state.hp, state.maxHp, state.alive);

  if (state.alive && state.predictor) {
    const col = Math.floor(state.predictor.position.x / UNITS_PER_CELL);
    const row = Math.floor(state.predictor.position.y / UNITS_PER_CELL);
    const cell = state.territory.cell(col, row);
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
  views.hud.setHint(state.notices.hint("", nowMs));
}
