// The smoke suite's map (server/config/smoke-map.json, run by the server through
// server/scripts/smoke-config.sh): one faction per script role, named after it,
// its capital where that role's players come into the world — spawns are only at
// the capital (GAME-016). A script finds its spot by the role's name.
import { readFileSync } from "node:fs";

interface SmokeMap {
  factions: { id: number; name: string }[];
  capitals: { faction_id: number; cell: number }[];
}

const MAP_WIDTH = 100; // cells, as config.json's map
const map = JSON.parse(readFileSync(new URL("../../server/config/smoke-map.json", import.meta.url), "utf8")) as SmokeMap;

export interface Spot {
  factionId: number;
  cell: number;
  col: number;
  row: number;
  x: number; // the cell's centre, world units: where a spawned body stands
  y: number;
}

export function spot(role: string): Spot {
  const faction = map.factions.find((f) => f.name === role);
  const capital = faction && map.capitals.find((c) => c.faction_id === faction.id);
  if (!faction || !capital) throw new Error(`smoke map: no role '${role}'`);
  const col = capital.cell % MAP_WIDTH;
  const row = Math.floor(capital.cell / MAP_WIDTH);
  return { factionId: faction.id, cell: capital.cell, col, row, x: col * 100 + 50, y: row * 100 + 50 };
}
