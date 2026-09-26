import { Application, Container, Graphics } from "pixi.js";

import type { PlayerInfo, PlayerState } from "../gen/game/v1/protocol_pb.js";

const UNITS_PER_CELL = 100;

// Renders the whole map scaled to fit the viewport: a field background plus a
// coloured dot per player (colour = faction). No territory colouring yet (M4).
export class Scene {
  private readonly field = new Graphics();
  private readonly playersLayer = new Container();
  private readonly sprites = new Map<number, Graphics>();
  private readonly factionColors = new Map<number, number>();
  private readonly playerFaction = new Map<number, number>();
  private worldW = UNITS_PER_CELL;
  private worldH = UNITS_PER_CELL;
  private scale = 1;
  private selfId = 0;

  constructor(private readonly app: Application) {
    app.stage.addChild(this.field);
    app.stage.addChild(this.playersLayer);
    app.renderer.on("resize", () => this.layout());
  }

  setSelf(id: number): void {
    this.selfId = id;
  }

  setFactions(factions: readonly { id: number; color: number }[]): void {
    for (const f of factions) this.factionColors.set(f.id, f.color);
  }

  setConfig(mapWidth: number, mapHeight: number): void {
    this.worldW = mapWidth * UNITS_PER_CELL;
    this.worldH = mapHeight * UNITS_PER_CELL;
    this.layout();
  }

  upsertRoster(players: readonly PlayerInfo[]): void {
    for (const p of players) this.playerFaction.set(p.id, p.factionId);
  }

  removeFromRoster(ids: readonly number[]): void {
    for (const id of ids) this.playerFaction.delete(id);
  }

  applySnapshot(players: readonly PlayerState[]): void {
    const seen = new Set<number>();
    for (const p of players) {
      seen.add(p.id);
      let g = this.sprites.get(p.id);
      if (!g) {
        g = new Graphics();
        this.playersLayer.addChild(g);
        this.sprites.set(p.id, g);
      }
      const color = this.factionColors.get(this.playerFaction.get(p.id) ?? 0) ?? 0xaaaaaa;
      g.clear();
      g.circle(0, 0, 5).fill(color);
      if (p.id === this.selfId) g.circle(0, 0, 8).stroke({ width: 2, color: 0xffffff });
      g.position.set(p.x * this.scale, p.y * this.scale);
    }
    for (const [id, g] of this.sprites) {
      if (!seen.has(id)) {
        g.destroy();
        this.sprites.delete(id);
      }
    }
  }

  // Canvas pixel -> cell (col, row). Caller validates bounds.
  screenToCell(sx: number, sy: number): [number, number] {
    const col = Math.floor(sx / this.scale / UNITS_PER_CELL);
    const row = Math.floor(sy / this.scale / UNITS_PER_CELL);
    return [col, row];
  }

  private layout(): void {
    this.scale = Math.min(this.app.screen.width / this.worldW, this.app.screen.height / this.worldH);
    this.field.clear();
    this.field
      .rect(0, 0, this.worldW * this.scale, this.worldH * this.scale)
      .fill(0x181820)
      .stroke({ width: 1, color: 0x3a3a48 });
  }
}
