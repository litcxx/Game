import type { FactionScore } from "../gen/game/v1/protocol_pb.js";

// What the HUD and the picker show of a faction: its cells, its share of the
// map (percent, to a tenth: 49 cells of 10 000 are 0.5, not 0) and its players
// in the world.
export interface FactionStats {
  cells: number;
  percent: number;
  online: number;
}

// The faction scores as the server last sent them (FactionScores, on joining
// and then once a second): the real count over the whole map, not what this
// client has seen of it. Each message is whole: it replaces the last.
export class FactionScores {
  private readonly scores = new Map<number, { cells: number; online: number }>();

  set(scores: readonly FactionScore[]): void {
    this.scores.clear();
    for (const s of scores) this.scores.set(s.factionId, { cells: s.cells, online: s.online });
  }

  clear(): void {
    this.scores.clear();
  }

  // Zeros for a faction not in the last scores (or before any).
  of(factionId: number, mapCells: number): FactionStats {
    const s = this.scores.get(factionId);
    if (!s) return { cells: 0, percent: 0, online: 0 };
    const percent = mapCells > 0 ? Math.round((s.cells / mapCells) * 1000) / 10 : 0;
    return { cells: s.cells, percent, online: s.online };
  }
}
