import { Application, Container, Graphics } from "pixi.js";

import type { CellUpdate, PlayerInfo, PlayerState } from "../gen/game/v1/protocol_pb.js";

const UNITS_PER_CELL = 100;

// Renders the whole map scaled to fit the viewport: the territory grid (owned
// cells tinted by faction, in-progress captures tinted by progress) beneath a
// coloured dot per player.
export class Scene {
  private readonly field = new Graphics();
  private readonly grid = new Graphics();
  private readonly playersLayer = new Container();
  private readonly sprites = new Map<number, Graphics>();
  private readonly factionColors = new Map<number, number>();
  private readonly playerFaction = new Map<number, number>();
  private mapCols = 1;
  private mapRows = 1;
  private owners: Uint8Array = new Uint8Array(0);           // owner faction id (0 = neutral)
  private captureFaction: Uint8Array = new Uint8Array(0);   // who is capturing (0 = none)
  private captureProgress: Uint8Array = new Uint8Array(0);  // 0..100
  private scale = 1;
  private selfId = 0;
  private maxHp = 100;
  private attackRange = 0;          // world units; shown as a ring around self
  private attackCooldownTicks = 1;  // ticks between swings, for the self cooldown bar

  constructor(private readonly app: Application) {
    app.stage.addChild(this.field);
    app.stage.addChild(this.grid);
    app.stage.addChild(this.playersLayer);
    app.renderer.on("resize", () => this.layout());
  }

  setSelf(id: number): void {
    this.selfId = id;
  }

  setFactions(factions: readonly { id: number; color: number }[]): void {
    for (const f of factions) this.factionColors.set(f.id, f.color);
  }

  setConfig(
    mapWidth: number,
    mapHeight: number,
    maxHp: number,
    attackRange: number,
    attackCooldownTicks: number,
  ): void {
    this.mapCols = Math.max(1, mapWidth);
    this.mapRows = Math.max(1, mapHeight);
    this.maxHp = Math.max(1, maxHp);
    this.attackRange = Math.max(0, attackRange);
    this.attackCooldownTicks = Math.max(1, attackCooldownTicks);
    this.layout();
  }

  // Full territory state (owners + any in-progress captures).
  setMapState(owners: Uint8Array, captures: readonly CellUpdate[]): void {
    this.owners = owners;
    this.captureFaction = new Uint8Array(owners.length);
    this.captureProgress = new Uint8Array(owners.length);
    for (const c of captures) this.setCapture(c);
    this.drawGrid();
  }

  // Incremental territory changes from Snapshot.cells.
  applyCellUpdates(cells: readonly CellUpdate[]): void {
    if (cells.length === 0) return;
    for (const c of cells) {
      if (c.index < this.owners.length) this.owners[c.index] = c.owner;
      this.setCapture(c);
    }
    this.drawGrid();
  }

  upsertRoster(players: readonly PlayerInfo[]): void {
    for (const p of players) this.playerFaction.set(p.id, p.factionId);
  }

  removeFromRoster(ids: readonly number[]): void {
    for (const id of ids) this.playerFaction.delete(id);
  }

  // `selfReadyTick`/`serverTick` drive the self cooldown bar.
  applySnapshot(players: readonly PlayerState[], selfReadyTick = 0, serverTick = 0): void {
    const seen = new Set<number>();
    for (const p of players) {
      seen.add(p.id);
      let g = this.sprites.get(p.id);
      if (!g) {
        g = new Graphics();
        this.playersLayer.addChild(g);
        this.sprites.set(p.id, g);
      }
      const isSelf = p.id === this.selfId;
      g.clear();
      if (p.hp <= 0) {
        // Dead body: a faded, crossed-out marker with no bars until respawn.
        g.circle(0, 0, 5).fill({ color: 0x555555, alpha: 0.6 });
        g.moveTo(-4, -4).lineTo(4, 4).moveTo(-4, 4).lineTo(4, -4).stroke({ width: 1.5, color: 0x1a1a1a });
      } else {
        const color = this.factionColors.get(this.playerFaction.get(p.id) ?? 0) ?? 0xaaaaaa;
        if (isSelf && this.attackRange > 0) {
          // Own attack area (drawn under the dot).
          g.circle(0, 0, this.attackRange * this.scale).stroke({ width: 1, color: 0xffffff, alpha: 0.22 });
        }
        g.circle(0, 0, 5).fill(color);
        if (isSelf) g.circle(0, 0, 8).stroke({ width: 2, color: 0xffffff });
        this.drawHpBar(g, p.hp);
        if (isSelf) this.drawCooldownBar(g, selfReadyTick, serverTick);
      }
      g.position.set(p.x * this.scale, p.y * this.scale);
    }
    for (const [id, g] of this.sprites) {
      if (!seen.has(id)) {
        g.destroy();
        this.sprites.delete(id);
      }
    }
  }

  // Small HP bar above the head, green -> amber -> red as hp drops.
  private drawHpBar(g: Graphics, hp: number): void {
    const frac = Math.max(0, Math.min(1, hp / this.maxHp));
    const w = 16;
    const h = 3;
    const y = -12;
    g.rect(-w / 2, y, w, h).fill({ color: 0x000000, alpha: 0.6 });
    if (frac > 0) {
      const color = frac > 0.5 ? 0x37c837 : frac > 0.25 ? 0xd8a038 : 0xd83838;
      g.rect(-w / 2, y, w * frac, h).fill(color);
    }
  }

  // Cooldown bar under the head: fills up as the next attack recharges
  // (blue while charging, green when ready).
  private drawCooldownBar(g: Graphics, readyTick: number, serverTick: number): void {
    const remaining = Math.max(0, readyTick - serverTick);
    const frac = Math.max(0, Math.min(1, 1 - remaining / this.attackCooldownTicks));
    const w = 16;
    const h = 3;
    const y = 9;
    g.rect(-w / 2, y, w, h).fill({ color: 0x000000, alpha: 0.6 });
    if (frac > 0) {
      const color = frac >= 1 ? 0x37c837 : 0x4a90d8;
      g.rect(-w / 2, y, w * frac, h).fill(color);
    }
  }

  // Canvas pixel -> cell (col, row). Caller validates bounds.
  screenToCell(sx: number, sy: number): [number, number] {
    const col = Math.floor(sx / this.scale / UNITS_PER_CELL);
    const row = Math.floor(sy / this.scale / UNITS_PER_CELL);
    return [col, row];
  }

  private setCapture(c: CellUpdate): void {
    if (c.index >= this.captureFaction.length) return;
    this.captureFaction[c.index] = c.captureFaction;
    this.captureProgress[c.index] = Math.min(100, c.captureProgress);
  }

  private layout(): void {
    const worldW = this.mapCols * UNITS_PER_CELL;
    const worldH = this.mapRows * UNITS_PER_CELL;
    this.scale = Math.min(this.app.screen.width / worldW, this.app.screen.height / worldH);

    this.field.clear();
    this.field
      .rect(0, 0, worldW * this.scale, worldH * this.scale)
      .fill(0x181820)
      .stroke({ width: 1, color: 0x3a3a48 });

    this.drawGrid();
  }

  private drawGrid(): void {
    this.grid.clear();
    const cell = UNITS_PER_CELL * this.scale;  // pixels per cell
    const w = this.mapCols * cell;
    const h = this.mapRows * cell;

    for (let i = 0; i < this.owners.length; i++) {
      const col = i % this.mapCols;
      const row = Math.floor(i / this.mapCols);
      const owner = this.owners[i]!;
      if (owner !== 0) {
        const color = this.factionColors.get(owner);
        if (color !== undefined) this.grid.rect(col * cell, row * cell, cell, cell).fill({ color, alpha: 0.4 });
      } else if (this.captureProgress[i]! > 0) {
        const color = this.factionColors.get(this.captureFaction[i]!);
        if (color !== undefined) {
          const alpha = 0.08 + 0.32 * (this.captureProgress[i]! / 100);
          this.grid.rect(col * cell, row * cell, cell, cell).fill({ color, alpha });
        }
      }
    }

    // Cell gridlines (one path, one stroke).
    for (let c = 0; c <= this.mapCols; c++) this.grid.moveTo(c * cell, 0).lineTo(c * cell, h);
    for (let r = 0; r <= this.mapRows; r++) this.grid.moveTo(0, r * cell).lineTo(w, r * cell);
    this.grid.stroke({ width: 1, color: 0x2c2c38, alpha: 0.6 });
  }
}
