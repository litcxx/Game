import type { KeyValueStore } from "./net/session.js";
import type { GameState } from "./state/gameState.js";

// Set once the player has captured a cell; the capture hint is not shown again.
export const CAPTURED_KEY = "territory.captured";

// The first-capture lesson: until the player captures a cell themselves, the
// hint line tells how (hint.ts). It counts only their own capture: the cell
// under them turns their faction's between two snapshots while they held E
// (an ally's capture under an idle player doesn't teach them E). Learned sets
// GameState.captureLearned, for this visit and, through the store, later ones.
//
// The game calls noteCapturing() every fixed step and observe() after each
// snapshot.
export class CaptureLesson {
  private last: { index: number; owner: number } | undefined; // the cell under you at the last snapshot
  private holding = false; // E at the latest input step
  private heldE = false; // E at any input step since the last snapshot

  constructor(
    private readonly state: GameState,
    private readonly store: KeyValueStore,
  ) {
    if (store.getItem(CAPTURED_KEY) !== null) state.captureLearned = true;
  }

  // Several snapshots can land between two input steps (a low frame rate, or
  // two arriving together): E still held then counts for each of them.
  noteCapturing(held: boolean): void {
    this.holding = held;
    this.heldE ||= held;
  }

  observe(): void {
    const before = this.last;
    const heldE = this.heldE || this.holding;
    const cell = this.state.cellUnderMe();
    this.last = cell && { index: cell.index, owner: cell.owner };
    this.heldE = false;
    if (this.state.captureLearned || !cell || !before || !heldE) return;
    const mine = this.state.myFaction;
    if (cell.index === before.index && before.owner !== mine && cell.owner === mine) {
      this.state.captureLearned = true;
      this.store.setItem(CAPTURED_KEY, "1");
    }
  }
}
