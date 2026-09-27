import { Application, Container, Graphics, Text } from "pixi.js";

import type { CellUpdate, PlayerInfo, PlayerState } from "../gen/game/v1/protocol_pb.js";
import { Camera, UNITS_PER_CELL } from "./camera.js";

interface PlayerSprite {
  root: Container;
  gfx: Graphics;
  label: Text;
  name: string; // last name set on the label (avoid re-rasterizing text each frame)
}

// Renders the territory grid and a coloured dot per player through a Camera
// (follow / full-map). Driven per frame by main's ticker: the local player uses
// its predicted position, remote players their interpolated position. Markers
// stay a fixed pixel size; the attack-range ring scales with the world. The world
// (grid) is only redrawn when the camera moved or territory changed.
export class Scene {
  private readonly field = new Graphics();
  private readonly grid = new Graphics();
  private readonly coordsLayer = new Container();
  private readonly playersLayer = new Container();
  private readonly sprites = new Map<number, PlayerSprite>();
  private readonly coordPool: Text[] = [];      // reused per-cell coordinate labels
  private readonly coordCellIdx: number[] = [];  // cell index each pooled label shows

  private readonly minimapLayer = new Container();
  private readonly minimapBg = new Graphics();
  private readonly minimapMask = new Graphics();
  private readonly minimapTerritory = new Graphics();
  private readonly minimapOverlay = new Graphics();
  private readonly minimapW = 176;  // fixed width; height follows the screen aspect
  private minimapH = 100;

  private readonly factionColors = new Map<number, number>();
  private readonly playerFaction = new Map<number, number>();
  private readonly playerNames = new Map<number, string>();
  private readonly camera = new Camera();

  private mapCols = 1;
  private mapRows = 1;
  private owners: Uint8Array = new Uint8Array(0);           // owner faction id (0 = neutral)
  private captureFaction: Uint8Array = new Uint8Array(0);   // who is capturing (0 = none)
  private captureProgress: Uint8Array = new Uint8Array(0);  // 0..100
  private readonly ownedCount = new Map<number, number>();  // faction id -> owned cell count
  private selfId = 0;
  private maxHp = 100;
  private attackRange = 0;          // world units; ring around self
  private attackCooldownTicks = 1;  // for the self cooldown ring
  private tickRate = 60;            // to interpolate the current server tick between snapshots
  private serverTickAtMs = 0;       // performance.now() when serverTick was last set

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
    app.stage.addChild(this.coordsLayer);
    app.stage.addChild(this.playersLayer);
    app.stage.addChild(this.minimapLayer);
    this.minimapLayer.addChild(this.minimapBg, this.minimapTerritory, this.minimapOverlay, this.minimapMask);
    this.minimapLayer.mask = this.minimapMask; // clip cells/dots to the box
    this.camera.setScreen(app.screen.width, app.screen.height);
    this.layoutMinimap();
    app.renderer.on("resize", () => {
      this.camera.setScreen(this.app.screen.width, this.app.screen.height);
      this.layoutMinimap();
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
    tickRate: number,
  ): void {
    this.mapCols = Math.max(1, mapWidth);
    this.mapRows = Math.max(1, mapHeight);
    this.maxHp = Math.max(1, maxHp);
    this.attackRange = Math.max(0, attackRange);
    this.attackCooldownTicks = Math.max(1, attackCooldownTicks);
    this.tickRate = Math.max(1, tickRate);
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
    this.ownedCount.clear();
    for (let i = 0; i < owners.length; i++) {
      const o = owners[i]!;
      if (o !== 0) this.ownedCount.set(o, (this.ownedCount.get(o) ?? 0) + 1);
    }
    for (const c of captures) this.setCapture(c);
    this.worldDirty = true;
  }

  // Incremental territory changes from Snapshot.cells.
  applyCellUpdates(cells: readonly CellUpdate[]): void {
    if (cells.length === 0) return;
    for (const c of cells) {
      if (c.index < this.owners.length) {
        const old = this.owners[c.index]!;
        if (old !== c.owner) {
          if (old !== 0) this.ownedCount.set(old, (this.ownedCount.get(old) ?? 1) - 1);
          if (c.owner !== 0) this.ownedCount.set(c.owner, (this.ownedCount.get(c.owner) ?? 0) + 1);
          this.owners[c.index] = c.owner;
        }
      }
      this.setCapture(c);
    }
    this.worldDirty = true;
  }

  // --- HUD data accessors ---------------------------------------------------
  onlineCount(): number {
    return this.playerFaction.size;
  }

  factionStats(factionId: number): { cells: number; percent: number } {
    const cells = this.ownedCount.get(factionId) ?? 0;
    const total = this.mapCols * this.mapRows;
    return { cells, percent: total > 0 ? Math.round((cells / total) * 100) : 0 };
  }

  cellInfo(
    col: number,
    row: number,
  ): { index: number; ownerColor: number | undefined; captureColor: number | undefined; capturePercent: number } {
    const index = row * this.mapCols + col;
    if (col < 0 || row < 0 || col >= this.mapCols || row >= this.mapRows) {
      return { index, ownerColor: undefined, captureColor: undefined, capturePercent: 0 };
    }
    const owner = this.owners[index] ?? 0;
    const capFaction = this.captureFaction[index] ?? 0;
    return {
      index,
      ownerColor: owner !== 0 ? this.factionColors.get(owner) : undefined,
      captureColor: capFaction !== 0 ? this.factionColors.get(capFaction) : undefined,
      capturePercent: this.captureProgress[index] ?? 0,
    };
  }

  upsertRoster(players: readonly PlayerInfo[]): void {
    for (const p of players) {
      this.playerFaction.set(p.id, p.factionId);
      this.playerNames.set(p.id, p.name);
    }
  }

  removeFromRoster(ids: readonly number[]): void {
    for (const id of ids) {
      this.playerFaction.delete(id);
      this.playerNames.delete(id);
    }
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
    this.serverTickAtMs = performance.now();
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

  // Canvas pixel -> world units (for aiming).
  screenToWorld(sx: number, sy: number): [number, number] {
    return this.camera.screenToWorld(sx, sy);
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
    this.drawMinimap();
  }

  private drawWorld(): void {
    const cam = this.camera;

    // Map background rectangle.
    const [x0, y0] = cam.worldToScreen(0, 0);
    const [x1, y1] = cam.worldToScreen(this.mapCols * UNITS_PER_CELL, this.mapRows * UNITS_PER_CELL);
    this.field.clear();
    this.field.rect(x0, y0, x1 - x0, y1 - y0).fill(0x14141a).stroke({ width: 1, color: 0x33333f });

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
          const ownAlpha = 0.26 + 0.3 * this.cellShade(i); // per-cell tonal variation
          const alpha = taking ? ownAlpha * (1 - p) : ownAlpha;
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
    this.grid.stroke({ width: 1, color: 0x2a2a33, alpha: 0.35 });

    this.layoutCoords(c0, c1, r0, r1, cellPx);
  }

  // Deterministic per-cell brightness in [0,1) for tonal variation (no flicker).
  private cellShade(i: number): number {
    let h = (i * 2654435761) >>> 0;
    h ^= h >>> 15;
    h = Math.imul(h, 2246822519) >>> 0;
    h ^= h >>> 13;
    return (h >>> 8) / 0x1000000;
  }

  // Dim coordinate label per visible cell (follow mode, when cells are big enough).
  // Reuses a pool; only rewrites a label's text when its cell changes.
  private layoutCoords(c0: number, c1: number, r0: number, r1: number, cellPx: number): void {
    const cam = this.camera;
    if (cam.mode !== "follow" || cellPx < 40) {
      for (const t of this.coordPool) t.visible = false;
      return;
    }
    let k = 0;
    for (let row = r0; row <= r1; row++) {
      for (let col = c0; col <= c1; col++) {
        const idx = row * this.mapCols + col;
        let t = this.coordPool[k];
        if (t === undefined) {
          t = new Text({ text: "", style: { fill: "#9a9a88", fontFamily: "monospace", fontSize: 10 } });
          t.alpha = 0.5;
          this.coordsLayer.addChild(t);
          this.coordPool[k] = t;
          this.coordCellIdx[k] = -1;
        }
        if (this.coordCellIdx[k] !== idx) {
          t.text = String(idx);
          this.coordCellIdx[k] = idx;
        }
        const [sx, sy] = cam.worldToScreen(col * UNITS_PER_CELL + 8, row * UNITS_PER_CELL + 8);
        t.position.set(Math.round(sx), Math.round(sy)); // integer pixels -> no sub-pixel blur
        t.visible = true;
        k++;
      }
    }
    for (; k < this.coordPool.length; k++) this.coordPool[k].visible = false;
  }

  private drawPlayers(): void {
    const cam = this.camera;
    const seen = new Set<number>();
    for (const [id, mp] of this.meta) {
      seen.add(id);
      let s = this.sprites.get(id);
      if (s === undefined) {
        const root = new Container();
        const gfx = new Graphics();
        const label = new Text({ text: "", style: { fill: "#d8d8d0", fontFamily: "monospace", fontSize: 11 } });
        label.anchor.set(0.5, 0);
        root.addChild(gfx);
        root.addChild(label);
        this.playersLayer.addChild(root);
        s = { root, gfx, label, name: "" };
        this.sprites.set(id, s);
      }
      const { root, gfx, label } = s;
      const isSelf = id === this.selfId;
      const pos = isSelf ? this.selfPredicted : (this.remotePositions.get(id) ?? { x: mp.x, y: mp.y });
      gfx.clear();

      if (mp.hp <= 0) {
        // Dead body: faded, crossed-out marker; no name/bars until respawn.
        gfx.circle(0, 0, 5).fill({ color: 0x555555, alpha: 0.6 });
        gfx.moveTo(-4, -4).lineTo(4, 4).moveTo(-4, 4).lineTo(4, -4).stroke({ width: 1.5, color: 0x1a1a1a });
        label.visible = false;
      } else {
        const color = this.factionColors.get(this.playerFaction.get(id) ?? 0) ?? 0xaaaaaa;
        // Soft glow (layered translucent discs — cheaper than a blur filter).
        gfx.circle(0, 0, 11).fill({ color, alpha: 0.06 });
        gfx.circle(0, 0, 8).fill({ color, alpha: 0.1 });
        // Own attack area (faint) + the cooldown arc sweeping around it.
        if (isSelf && this.attackRange > 0) {
          const rr = this.attackRange * cam.scale;
          gfx.circle(0, 0, rr).stroke({ width: 1, color: 0xffffff, alpha: 0.18 });
          this.drawCooldownRing(gfx, rr, color);
        }
        // Grounding shadow: over the glow (so it isn't washed out) and under the
        // token; darker/larger so it reads against the dark map.
        gfx.ellipse(0, 8, 9, 3.5).fill({ color: 0x000000, alpha: 0.5 });
        // Token — identical for every player; the attack-range ring marks "you".
        gfx.circle(0, 0, 6).fill(color).stroke({ width: 1, color: this.lighten(color, 0.45), alpha: 0.9 });

        // Name pill (above the head) + hp bar beneath it.
        const name = this.playerNames.get(id) ?? "";
        if (s.name !== name) {
          label.text = name;
          s.name = name;
        }
        const hasName = name.length > 0;
        label.visible = hasName;
        const pillW = hasName ? Math.max(label.width + 10, 20) : 16;
        const pillY = -24;
        if (hasName) {
          gfx.roundRect(-pillW / 2, pillY, pillW, 15, 3).fill({ color: 0x0a0a0f, alpha: 0.6 });
          label.position.set(0, pillY + 2);
        }
        const barY = hasName ? pillY + 15 : -13;
        const frac = Math.max(0, Math.min(1, mp.hp / this.maxHp));
        gfx.rect(-pillW / 2, barY, pillW, 2).fill({ color: 0x000000, alpha: 0.5 });
        if (frac > 0) {
          const hc = frac > 0.5 ? 0x37c837 : frac > 0.25 ? 0xd8a038 : 0xd83838;
          gfx.rect(-pillW / 2, barY, pillW * frac, 2).fill(hc);
        }
      }

      const [sx, sy] = cam.worldToScreen(pos.x, pos.y);
      root.position.set(sx, sy);
    }
    for (const [id, s] of this.sprites) {
      if (!seen.has(id)) {
        s.root.destroy({ children: true });
        this.sprites.delete(id);
      }
    }
  }

  // Mix a colour toward white by `amt` (0..1) — used for the token rim.
  private lighten(color: number, amt: number): number {
    const r = (color >> 16) & 0xff;
    const g = (color >> 8) & 0xff;
    const b = color & 0xff;
    const m = (c: number) => Math.round(c + (255 - c) * amt);
    return (m(r) << 16) | (m(g) << 8) | m(b);
  }

  // Smoothly-interpolated current server tick (advances between snapshots).
  private currentTick(): number {
    return this.serverTick + ((performance.now() - this.serverTickAtMs) / 1000) * this.tickRate;
  }

  // Cooldown as an arc sweeping clockwise from the top around the attack-range
  // ring; a full ring means the next attack is ready.
  private drawCooldownRing(gfx: Graphics, radius: number, color: number): void {
    const remaining = Math.max(0, this.selfReadyTick - this.currentTick());
    const frac = Math.max(0, Math.min(1, 1 - remaining / this.attackCooldownTicks));
    if (frac <= 0) return; // just attacked -> empty
    const start = -Math.PI / 2; // top (12 o'clock)
    const arcColor = this.lighten(color, 0.4);
    if (frac >= 1) {
      gfx.circle(0, 0, radius).stroke({ width: 2, color: arcColor, alpha: 0.76 });
    } else {
      gfx.moveTo(0, -radius); // begin at the arc's start -> no line from the centre
      gfx.arc(0, 0, radius, start, start + frac * Math.PI * 2).stroke({ width: 2, color: arcColor, alpha: 0.76 });
    }
  }

  private layoutMinimap(): void {
    const w = this.minimapW;
    const h = Math.round(w * (this.app.screen.height / Math.max(1, this.app.screen.width)));
    this.minimapH = h;
    this.minimapLayer.position.set(this.app.screen.width - 16 - w, 52);
    this.minimapBg.clear().rect(0, 0, w, h).fill({ color: 0x0a0a0f, alpha: 0.7 }).stroke({ width: 1, color: 0x3a3a46 });
    this.minimapMask.clear().rect(0, 0, w, h).fill(0xffffff);
  }

  // Local minimap: ~1.5x the player's view around the camera, drawn as cells
  // (neutral cells are dark-but-visible, not a black void) with a viewport rect
  // and player dots. Redrawn each frame — small (~a few hundred cells).
  private drawMinimap(): void {
    const cam = this.camera;
    const viewW = this.app.screen.width / cam.scale; // visible world size
    const viewH = this.app.screen.height / cam.scale;
    const regionW = viewW * 1.5;
    const regionH = viewH * 1.5;
    const originX = cam.centerX - regionW / 2;
    const originY = cam.centerY - regionH / 2;
    const mm = this.minimapW / regionW; // px per world unit (uniform: aspects match)
    const toX = (wx: number) => (wx - originX) * mm;
    const toY = (wy: number) => (wy - originY) * mm;
    const cellPx = UNITS_PER_CELL * mm;

    const c0 = Math.max(0, Math.floor(originX / UNITS_PER_CELL));
    const c1 = Math.min(this.mapCols - 1, Math.floor((originX + regionW) / UNITS_PER_CELL));
    const r0 = Math.max(0, Math.floor(originY / UNITS_PER_CELL));
    const r1 = Math.min(this.mapRows - 1, Math.floor((originY + regionH) / UNITS_PER_CELL));

    const t = this.minimapTerritory;
    t.clear();
    for (let row = r0; row <= r1; row++) {
      for (let col = c0; col <= c1; col++) {
        const o = this.owners[row * this.mapCols + col] ?? 0;
        const x = toX(col * UNITS_PER_CELL);
        const y = toY(row * UNITS_PER_CELL);
        if (o !== 0) {
          const color = this.factionColors.get(o);
          if (color !== undefined) t.rect(x, y, cellPx + 0.6, cellPx + 0.6).fill({ color, alpha: 0.75 });
        } else {
          t.rect(x, y, cellPx + 0.6, cellPx + 0.6).fill({ color: 0x1c1c24, alpha: 0.85 });
        }
      }
    }
    // Cell gridlines.
    const top = toY(r0 * UNITS_PER_CELL);
    const bot = toY((r1 + 1) * UNITS_PER_CELL);
    for (let col = c0; col <= c1 + 1; col++) {
      const x = toX(col * UNITS_PER_CELL);
      t.moveTo(x, top).lineTo(x, bot);
    }
    const left = toX(c0 * UNITS_PER_CELL);
    const right = toX((c1 + 1) * UNITS_PER_CELL);
    for (let row = r0; row <= r1 + 1; row++) {
      const y = toY(row * UNITS_PER_CELL);
      t.moveTo(left, y).lineTo(right, y);
    }
    t.stroke({ width: 1, color: 0x33333f, alpha: 0.4 });

    // Viewport rect + player dots.
    const g = this.minimapOverlay;
    g.clear();
    g.rect(toX(cam.centerX - viewW / 2), toY(cam.centerY - viewH / 2), viewW * mm, viewH * mm)
      .stroke({ width: 1, color: 0xffffff, alpha: 0.7 });
    for (const [id, mp] of this.meta) {
      if (mp.hp <= 0) continue;
      const isSelf = id === this.selfId;
      const pos = isSelf ? this.selfPredicted : (this.remotePositions.get(id) ?? { x: mp.x, y: mp.y });
      const color = isSelf ? 0xffffff : (this.factionColors.get(this.playerFaction.get(id) ?? 0) ?? 0xaaaaaa);
      g.circle(toX(pos.x), toY(pos.y), isSelf ? 2.5 : 1.8).fill({ color });
    }
  }

  private setCapture(c: CellUpdate): void {
    if (c.index >= this.captureFaction.length) return;
    this.captureFaction[c.index] = c.captureFaction;
    this.captureProgress[c.index] = Math.min(100, c.captureProgress);
  }
}
