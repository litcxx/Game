import { Application, Container, Graphics } from "pixi.js";

import type { CellUpdate, PlayerInfo, PlayerState } from "../gen/game/v1/protocol_pb.js";
import { Camera, UNITS_PER_CELL } from "./camera.js";

// Renders the territory grid and a coloured dot per player through a Camera
// (follow / full-map). Driven per frame by main's ticker: the local player uses
// its predicted position, remote players their interpolated position. Markers
// stay a fixed pixel size; the attack-range ring scales with the world. The world
// (grid) is only redrawn when the camera moved or territory changed.
export class Scene {
  private readonly field = new Graphics();
  private readonly grid = new Graphics();
  private readonly playersLayer = new Container();
  private readonly sprites = new Map<number, Graphics>();
  private readonly factionColors = new Map<number, number>();
  private readonly playerFaction = new Map<number, number>();
  private readonly camera = new Camera();

  private mapCols = 1;
  private mapRows = 1;
  private owners: Uint8Array = new Uint8Array(0);           // owner faction id (0 = neutral)
  private captureFaction: Uint8Array = new Uint8Array(0);   // who is capturing (0 = none)
  private captureProgress: Uint8Array = new Uint8Array(0);  // 0..100
  private selfId = 0;
  private maxHp = 100;
  private attackRange = 0;          // world units; ring around self
  private attackCooldownTicks = 1;  // for the self cooldown bar

  // Per-frame positions: self is predicted, remotes are interpolated.
  private selfPredicted = { x: 0, y: 0 };
  private remotePositions = new Map<number, { x: number; y: number }>();
  // Non-positional per-player state from snapshots (hp, fallback position).
  private readonly meta = new Map<number, { x: number; y: number; hp: number }>();
  private selfReadyTick = 0;
  private serverTick = 0;

  // Redraw the world only when the camera moved or territory changed.
  private worldDirty = true;
  private lastWorld = { scale: -1, cx: Number.NaN, cy: Number.NaN };

  constructor(private readonly app: Application) {
    app.stage.addChild(this.field);
    app.stage.addChild(this.grid);
    app.stage.addChild(this.playersLayer);
    this.camera.setScreen(app.screen.width, app.screen.height);
    app.renderer.on("resize", () => {
      this.camera.setScreen(this.app.screen.width, this.app.screen.height);
      this.worldDirty = true;
    });
  }

  setSelf(id: number): void {
    this.selfId = id;
  }

  // The local player's own faction (spawn/roster don't echo it back to us).
  setSelfFaction(factionId: number): void {
    this.playerFaction.set(this.selfId, factionId);
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
    this.camera.setWorld(this.mapCols, this.mapRows);
    this.camera.setScreen(this.app.screen.width, this.app.screen.height);
    this.worldDirty = true;
  }

  // Toggle between following the player and the full-map overview (M key).
  toggleMap(): void {
    this.camera.toggle();
    this.worldDirty = true;
  }

  get mapMode(): boolean {
    return this.camera.mode === "map";
  }

  // Full territory state (owners + any in-progress captures).
  setMapState(owners: Uint8Array, captures: readonly CellUpdate[]): void {
    this.owners = owners;
    this.captureFaction = new Uint8Array(owners.length);
    this.captureProgress = new Uint8Array(owners.length);
    for (const c of captures) this.setCapture(c);
    this.worldDirty = true;
  }

  // Incremental territory changes from Snapshot.cells.
  applyCellUpdates(cells: readonly CellUpdate[]): void {
    if (cells.length === 0) return;
    for (const c of cells) {
      if (c.index < this.owners.length) this.owners[c.index] = c.owner;
      this.setCapture(c);
    }
    this.worldDirty = true;
  }

  upsertRoster(players: readonly PlayerInfo[]): void {
    for (const p of players) this.playerFaction.set(p.id, p.factionId);
  }

  removeFromRoster(ids: readonly number[]): void {
    for (const id of ids) this.playerFaction.delete(id);
  }

  // Per-frame positions supplied by main.
  setSelfPredicted(x: number, y: number): void {
    this.selfPredicted = { x, y };
  }
  setRemotePositions(positions: Map<number, { x: number; y: number }>): void {
    this.remotePositions = positions;
  }

  // Snapshot meta: hp + fallback position per player, and the self cooldown.
  updateMeta(players: readonly PlayerState[], selfReadyTick: number, serverTick: number): void {
    this.selfReadyTick = selfReadyTick;
    this.serverTick = serverTick;
    const seen = new Set<number>();
    for (const p of players) {
      seen.add(p.id);
      this.meta.set(p.id, { x: p.x, y: p.y, hp: p.hp });
    }
    for (const id of [...this.meta.keys()]) if (!seen.has(id)) this.meta.delete(id);
  }

  remoteIds(): number[] {
    return [...this.meta.keys()].filter((id) => id !== this.selfId);
  }

  // Canvas pixel -> cell (col, row). Caller validates bounds.
  screenToCell(sx: number, sy: number): [number, number] {
    const [wx, wy] = this.camera.screenToWorld(sx, sy);
    return [Math.floor(wx / UNITS_PER_CELL), Math.floor(wy / UNITS_PER_CELL)];
  }

  // Draw one frame (called by main's ticker).
  frame(): void {
    const cam = this.camera;
    cam.setTarget(this.selfPredicted.x, this.selfPredicted.y);
    if (
      this.worldDirty ||
      cam.scale !== this.lastWorld.scale ||
      cam.centerX !== this.lastWorld.cx ||
      cam.centerY !== this.lastWorld.cy
    ) {
      this.drawWorld();
      this.lastWorld = { scale: cam.scale, cx: cam.centerX, cy: cam.centerY };
      this.worldDirty = false;
    }
    this.drawPlayers();
  }

  private drawWorld(): void {
    const cam = this.camera;

    // Map background rectangle.
    const [x0, y0] = cam.worldToScreen(0, 0);
    const [x1, y1] = cam.worldToScreen(this.mapCols * UNITS_PER_CELL, this.mapRows * UNITS_PER_CELL);
    this.field.clear();
    this.field.rect(x0, y0, x1 - x0, y1 - y0).fill(0x181820).stroke({ width: 1, color: 0x3a3a48 });

    // Only the cells currently on screen.
    this.grid.clear();
    const [c0, c1] = cam.visibleCols();
    const [r0, r1] = cam.visibleRows();
    const cellPx = UNITS_PER_CELL * cam.scale;
    for (let row = r0; row <= r1; row++) {
      for (let col = c0; col <= c1; col++) {
        const i = row * this.mapCols + col;
        const owner = this.owners[i] ?? 0;
        const capFaction = this.captureFaction[i] ?? 0;
        const capProgress = this.captureProgress[i] ?? 0;
        if (owner === 0 && capProgress === 0) continue; // neutral, untouched

        const [sx, sy] = cam.worldToScreen(col * UNITS_PER_CELL, row * UNITS_PER_CELL);
        const taking = capProgress > 0 && capFaction !== 0;
        const p = Math.min(100, capProgress) / 100;

        // Current owner: fades out as a takeover progresses (cross-fade).
        if (owner !== 0) {
          const color = this.factionColors.get(owner);
          const alpha = taking ? 0.4 * (1 - p) : 0.4;
          if (color !== undefined && alpha > 0) {
            this.grid.rect(sx, sy, cellPx, cellPx).fill({ color, alpha });
          }
        }
        // Capturing faction: fades in with progress (neutral claim or takeover).
        if (taking) {
          const color = this.factionColors.get(capFaction);
          if (color !== undefined) {
            this.grid.rect(sx, sy, cellPx, cellPx).fill({ color, alpha: 0.08 + 0.32 * p });
          }
        }
      }
    }

    // Gridlines across the visible block (constant 1px).
    const [, sTop] = cam.worldToScreen(0, r0 * UNITS_PER_CELL);
    const [, sBot] = cam.worldToScreen(0, (r1 + 1) * UNITS_PER_CELL);
    for (let col = c0; col <= c1 + 1; col++) {
      const [sx] = cam.worldToScreen(col * UNITS_PER_CELL, 0);
      this.grid.moveTo(sx, sTop).lineTo(sx, sBot);
    }
    const [sLeft] = cam.worldToScreen(c0 * UNITS_PER_CELL, 0);
    const [sRight] = cam.worldToScreen((c1 + 1) * UNITS_PER_CELL, 0);
    for (let row = r0; row <= r1 + 1; row++) {
      const [, sy] = cam.worldToScreen(0, row * UNITS_PER_CELL);
      this.grid.moveTo(sLeft, sy).lineTo(sRight, sy);
    }
    this.grid.stroke({ width: 1, color: 0x2c2c38, alpha: 0.6 });
  }

  private drawPlayers(): void {
    const cam = this.camera;
    const seen = new Set<number>();
    for (const [id, mp] of this.meta) {
      seen.add(id);
      let g = this.sprites.get(id);
      if (!g) {
        g = new Graphics();
        this.playersLayer.addChild(g);
        this.sprites.set(id, g);
      }
      const isSelf = id === this.selfId;
      const pos = isSelf ? this.selfPredicted : (this.remotePositions.get(id) ?? { x: mp.x, y: mp.y });
      g.clear();
      if (mp.hp <= 0) {
        // Dead body: a faded, crossed-out marker with no bars until respawn.
        g.circle(0, 0, 5).fill({ color: 0x555555, alpha: 0.6 });
        g.moveTo(-4, -4).lineTo(4, 4).moveTo(-4, 4).lineTo(4, -4).stroke({ width: 1.5, color: 0x1a1a1a });
      } else {
        const color = this.factionColors.get(this.playerFaction.get(id) ?? 0) ?? 0xaaaaaa;
        if (isSelf && this.attackRange > 0) {
          // Own attack area — scales with the world zoom.
          g.circle(0, 0, this.attackRange * cam.scale).stroke({ width: 1, color: 0xffffff, alpha: 0.22 });
        }
        g.circle(0, 0, 5).fill(color);
        if (isSelf) g.circle(0, 0, 8).stroke({ width: 2, color: 0xffffff });
        this.drawHpBar(g, mp.hp);
        if (isSelf) this.drawCooldownBar(g, this.selfReadyTick, this.serverTick);
      }
      const [sx, sy] = cam.worldToScreen(pos.x, pos.y);
      g.position.set(sx, sy);
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

  private setCapture(c: CellUpdate): void {
    if (c.index >= this.captureFaction.length) return;
    this.captureFaction[c.index] = c.captureFaction;
    this.captureProgress[c.index] = Math.min(100, c.captureProgress);
  }
}
