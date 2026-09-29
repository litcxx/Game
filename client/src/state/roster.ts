import type { PlayerInfo } from "../gen/game/v1/protocol_pb.js";

// Who is in the game: faction and name per player id, from Roster messages.
// The local player's own faction is set when it spawns — the roster doesn't
// echo it back to us.
export class Roster {
  private readonly factions = new Map<number, number>();
  private readonly names = new Map<number, string>();

  // Roster.upsert / removed; a full roster replaces the whole list (on join and
  // on resync).
  apply(upsert: readonly PlayerInfo[], full: boolean, removed: readonly number[]): void {
    if (full) {
      this.factions.clear();
      this.names.clear();
    }
    for (const p of upsert) {
      this.factions.set(p.id, p.factionId);
      this.names.set(p.id, p.name);
    }
    for (const id of removed) {
      this.factions.delete(id);
      this.names.delete(id);
    }
  }

  setFaction(id: number, factionId: number): void {
    this.factions.set(id, factionId);
  }

  // 0 / "" for someone not (or no longer) in the roster.
  factionOf(id: number): number {
    return this.factions.get(id) ?? 0;
  }
  nameOf(id: number): string {
    return this.names.get(id) ?? "";
  }

  get online(): number {
    return this.factions.size;
  }
}
