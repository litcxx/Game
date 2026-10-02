import { Application, Container, Graphics, Text } from "pixi.js";

import type { CellSight } from "../fog.js";
import type { Effect } from "../effects.js";
import type { GameState } from "../state/gameState.js";
import { UNITS_PER_CELL } from "../state/territory.js";
import { Camera } from "./camera.js";
import { capitalCentre, CapitalsView, drawCapitalMarker } from "./capitals.js";
import { CaptureHintView } from "./captureHint.js";
import { EffectsView } from "./effects.js";
import { FogView } from "./fog.js";
import { nameplate } from "./nameplate.js";
import { ProjectileView, type ProjectileSprite } from "./projectiles.js";

interface PlayerSprite {
  root: Container;
  gfx: Graphics;
  label: Text;
  name: string; // last name set on the label (avoid re-rasterizing text each frame)
}

// Minimap cell fill by sight: owned cells take their faction colour at `owned`
// alpha; the rest (neutral, or unexplored whatever it is) the flat `color`.
const MINIMAP_CELL: Record<CellSight, { owned: number; color: number; alpha: number }> = {
  visible: { owned: 0.75, color: 0x1c1c24, alpha: 0.85 },
  explored: { owned: 0.3, color: 0x131318, alpha: 0.85 },
  unexplored: { owned: 0, color: 0x060608, alpha: 0.95 },
};

// Renders the territory grid and a coloured dot per player through a Camera
// (follow / full-map), under fog of war (see ../fog.ts). A view of GameState:
// it draws what the state holds and keeps only what drawing needs (sprites, the
// camera). Driven per frame by main's ticker: the local player uses its
// predicted position, remote players their interpolated position. Markers stay
// a fixed pixel size; the attack-range ring scales with the world. The world
// (grid + fog) is only redrawn when the camera moved or the state's territory or
// sight changed (GameState.worldRevision).
export class Scene {
  private readonly field = new Graphics();
  private readonly grid = new Graphics();
  private readonly coordsLayer = new Container();
  private readonly fogView = new FogView();
  private readonly captureHintView = new CaptureHintView();
  private readonly capitalsView = new CapitalsView();
  private readonly playersLayer = new Container();
  private readonly projectileView = new ProjectileView();
  private readonly effectsView = new EffectsView();
  private projectiles: readonly ProjectileSprite[] = [];
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

  private readonly camera = new Camera();
  private aimTarget: { x: number; y: number } | undefined; // cursor in world units

  // Per-frame positions: self is predicted, remotes are interpolated.
  private selfPredicted = { x: 0, y: 0 };
  private remotePositions = new Map<number, { x: number; y: number }>();

  // Redraw the world only when the camera moved or the territory / sight changed.
  private worldDirty = true;
  private lastWorld = { scale: -1, cx: Number.NaN, cy: Number.NaN };
  private drawnRevision = 0; // GameState.worldRevision last drawn
  private worldCols = 0; // map size the camera was last given
  private worldRows = 0;

  constructor(
    private readonly app: Application,
    private readonly state: GameState,
  ) {
    app.stage.addChild(this.field);
    app.stage.addChild(this.grid);
    app.stage.addChild(this.coordsLayer);
    app.stage.addChild(this.fogView.gfx); // over the territory, under the players
    app.stage.addChild(this.captureHintView.gfx); // the cells you may capture
    app.stage.addChild(this.capitalsView.gfx); // over the fog: known from the start
    app.stage.addChild(this.playersLayer);
    app.stage.addChild(this.projectileView.gfx); // shots fly over the tokens
    app.stage.addChild(this.effectsView.root); // swings, shields, blocked hits
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

  // Cursor position in world units (aim line for projectile abilities).
  setAimTarget(target: { x: number; y: number } | undefined): void {
    this.aimTarget = target;
  }

  // Toggle between following the player and the full-map overview (M key).
  toggleMap(): void {
    this.camera.toggle();
    this.worldDirty = true;
  }

  get mapMode(): boolean {
    return this.camera.mode === "map";
  }

  // Effects from snapshot events (see ../effects.ts), drawn until they end.
  addEffects(effects: readonly Effect[]): void {
    this.effectsView.add(effects);
  }

  // Projectiles to draw this frame (interpolated positions), supplied by main.
  setProjectiles(items: readonly ProjectileSprite[]): void {
    this.projectiles = items;
  }

  // Per-frame positions supplied by main.
  setSelfPredicted(x: number, y: number): void {
    this.selfPredicted = { x, y };
  }
  setRemotePositions(positions: Map<number, { x: number; y: number }>): void {
    this.remotePositions = positions;
  }

  // Canvas pixel -> world units (for aiming).
  screenToWorld(sx: number, sy: number): [number, number] {
    return this.camera.screenToWorld(sx, sy);
  }

  // The territory or the sight changed since the last frame: redraw the world,
  // resizing the camera's map first if a Welcome brought a new one.
  private syncWorld(): void {
    if (this.state.worldRevision === this.drawnRevision) return;
    this.drawnRevision = this.state.worldRevision;
    const { cols, rows } = this.state.territory;
    if (cols !== this.worldCols || rows !== this.worldRows) {
      this.worldCols = cols;
      this.worldRows = rows;
      this.camera.setWorld(cols, rows);
      this.camera.setScreen(this.app.screen.width, this.app.screen.height);
    }
    this.worldDirty = true;
  }

  // Draw one frame (called by main's ticker).
  frame(): void {
    this.syncWorld();
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
    this.drawCaptureHint();
    this.drawPlayers();
    this.projectileView.draw(
      this.projectiles,
      (x, y) => cam.worldToScreen(x, y),
      cam.scale,
      (faction) => this.state.factionColor(faction) ?? 0xd8d8d0,
    );
    this.effectsView.draw(
      performance.now(),
      (id) => this.positionOf(id),
      (x, y) => cam.worldToScreen(x, y),
      cam.scale,
      (id) => this.state.factionColor(this.state.roster.factionOf(id)) ?? 0xd8d8d0,
    );
    this.drawMinimap();
  }

  private drawWorld(): void {
    const cam = this.camera;

    // Map background rectangle.
    const [x0, y0] = cam.worldToScreen(0, 0);
    const [x1, y1] = cam.worldToScreen(this.state.territory.cols * UNITS_PER_CELL, this.state.territory.rows * UNITS_PER_CELL);
    this.field.clear();
    this.field.rect(x0, y0, x1 - x0, y1 - y0).fill(0x14141a).stroke({ width: 1, color: 0x33333f });

    // Only the cells currently on screen.
    this.grid.clear();
    const [c0, c1] = cam.visibleCols();
    const [r0, r1] = cam.visibleRows();
    const cellPx = UNITS_PER_CELL * cam.scale;
    for (let row = r0; row <= r1; row++) {
      for (let col = c0; col <= c1; col++) {
        const i = row * this.state.territory.cols + col;
        const owner = this.state.territory.owners[i] ?? 0;
        const capFaction = this.state.territory.captureFaction[i] ?? 0;
        const capProgress = this.state.territory.captureProgress[i] ?? 0;
        if (owner === 0 && capProgress === 0) continue; // neutral, untouched

        const [sx, sy] = cam.worldToScreen(col * UNITS_PER_CELL, row * UNITS_PER_CELL);
        const taking = capProgress > 0 && capFaction !== 0;
        const p = Math.min(100, capProgress) / 100;

        // Current owner: fades out as a takeover progresses (cross-fade).
        if (owner !== 0) {
          const color = this.state.factionColor(owner);
          const ownAlpha = 0.26 + 0.3 * this.cellShade(i); // per-cell tonal variation
          const alpha = taking ? ownAlpha * (1 - p) : ownAlpha;
          if (color !== undefined && alpha > 0) {
            this.grid.rect(sx, sy, cellPx, cellPx).fill({ color, alpha });
          }
        }
        // Capturing faction: fades in with progress (neutral claim or takeover).
        if (taking) {
          const color = this.state.factionColor(capFaction);
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
    this.fogView.draw(this.state.fog, [c0, c1], [r0, r1], (x, y) => cam.worldToScreen(x, y), cellPx, {
      cols: this.state.territory.cols,
      rows: this.state.territory.rows,
    });
    this.capitalsView.draw(
      this.state.capitals,
      this.state.territory.cols,
      (x, y) => cam.worldToScreen(x, y),
      cellPx,
      (id) => this.state.factionColor(id) ?? 0xd8d8d0,
    );
  }

  // The on-screen cells you may capture, each frame (you move; it is a few hundred
  // cells at most, most of them not). None looked for when too small to mark: the
  // full map is the whole grid.
  private drawCaptureHint(): void {
    const cam = this.camera;
    const cols = this.state.territory.cols;
    const cellPx = UNITS_PER_CELL * cam.scale;
    const cells: number[] = [];
    if (this.state.alive && CaptureHintView.marks(cellPx)) {
      const [c0, c1] = cam.visibleCols();
      const [r0, r1] = cam.visibleRows();
      for (let row = r0; row <= r1; row++) {
        for (let col = c0; col <= c1; col++) {
          if (this.state.mayCapture(row * cols + col)) cells.push(row * cols + col);
        }
      }
    }
    this.captureHintView.draw(
      cells,
      cols,
      (x, y) => cam.worldToScreen(x, y),
      cellPx,
      this.state.factionColor(this.state.myFaction) ?? 0xd8d8d0,
    );
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
        const idx = row * this.state.territory.cols + col;
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
    for (const [id, mp] of this.state.players) {
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
      const isSelf = id === this.state.myId;
      const pos = isSelf ? this.selfPredicted : (this.remotePositions.get(id) ?? { x: mp.x, y: mp.y });
      gfx.clear();

      if (mp.hp <= 0) {
        // Dead body: faded, crossed-out marker; no name/bars until respawn.
        gfx.circle(0, 0, 5).fill({ color: 0x555555, alpha: 0.6 });
        gfx.moveTo(-4, -4).lineTo(4, 4).moveTo(-4, 4).lineTo(4, -4).stroke({ width: 1.5, color: 0x1a1a1a });
        label.visible = false;
      } else {
        const color = this.state.factionColor(this.state.roster.factionOf(id)) ?? 0xaaaaaa;
        // Soft glow (layered translucent discs — cheaper than a blur filter).
        gfx.circle(0, 0, 11).fill({ color, alpha: 0.06 });
        gfx.circle(0, 0, 8).fill({ color, alpha: 0.1 });
        // Active ability's reach (faint) + its cooldown arc sweeping around it; a
        // projectile ability also shows the aim line toward the cursor. A block
        // has no reach: a small guard circle carries the block's own cooldown.
        const ability = this.state.activeAbility;
        if (isSelf && ability !== undefined) {
          const cd = this.state.cooldownProgress(performance.now());
          if (ability.kind === "block") {
            const rr = 22;
            gfx.circle(0, 0, rr).stroke({ width: 1, color: 0xffffff, alpha: 0.18 });
            this.drawCooldownRing(gfx, rr, color, cd.block);
          } else if (ability.range > 0) {
            const rr = ability.range * cam.scale;
            gfx.circle(0, 0, rr).stroke({ width: 1, color: 0xffffff, alpha: 0.18 });
            this.drawCooldownRing(gfx, rr, color, cd.attack);
            if (ability.kind === "projectile") this.drawAimLine(gfx, pos, rr, color);
          }
        }
        // Grounding shadow: over the glow (so it isn't washed out) and under the
        // token; darker/larger so it reads against the dark map.
        gfx.ellipse(0, 8, 9, 3.5).fill({ color: 0x000000, alpha: 0.5 });
        // Token — identical for every player; the attack-range ring marks "you".
        gfx.circle(0, 0, 6).fill(color).stroke({ width: 1, color: this.lighten(color, 0.45), alpha: 0.9 });

        // Name pill (above the head) + hp bar beneath it.
        const name = this.state.roster.nameOf(id);
        if (s.name !== name) {
          label.text = name;
          s.name = name;
        }
        const hasName = name.length > 0;
        label.visible = hasName;
        const frac = mp.hp / Math.max(1, this.state.maxHp);
        const plate = nameplate(hasName ? label.width : undefined, frac);
        if (plate.pill) {
          const { x, y, w, h } = plate.pill;
          gfx.roundRect(x, y, w, h, 3).fill({ color: 0x0a0a0f, alpha: 0.6 });
          label.position.set(0, y + 2);
        }
        const { x: bx, y: by, w: bw, h: bh } = plate.bar;
        gfx.rect(bx, by, bw, bh).fill({ color: 0x000000, alpha: 0.5 });
        if (plate.fillW > 0) {
          const hc = frac > 0.5 ? 0x37c837 : frac > 0.25 ? 0xd8a038 : 0xd83838;
          gfx.rect(bx, by, plate.fillW, bh).fill(hc);
        }
      }

      const [sx, sy] = cam.worldToScreen(pos.x, pos.y);
      root.position.set(Math.round(sx), Math.round(sy)); // whole pixels: the name stays crisp in motion
    }
    for (const [id, s] of this.sprites) {
      if (!seen.has(id)) {
        s.root.destroy({ children: true });
        this.sprites.delete(id);
      }
    }
  }

  // Where a player's token is drawn this frame: self predicted, others interpolated.
  private positionOf(id: number): { x: number; y: number } | undefined {
    if (id === this.state.myId) return this.selfPredicted;
    const mp = this.state.players.get(id);
    return this.remotePositions.get(id) ?? (mp ? { x: mp.x, y: mp.y } : undefined);
  }

  // Mix a colour toward white by `amt` (0..1) — used for the token rim.
  private lighten(color: number, amt: number): number {
    const r = (color >> 16) & 0xff;
    const g = (color >> 8) & 0xff;
    const b = color & 0xff;
    const m = (c: number) => Math.round(c + (255 - c) * amt);
    return (m(r) << 16) | (m(g) << 8) | m(b);
  }

  // Cooldown as an arc sweeping clockwise from the top around the ring; a full
  // ring means the ability is ready. `frac` comes from GameState.cooldownProgress().
  private drawCooldownRing(gfx: Graphics, radius: number, color: number, frac: number): void {
    if (frac <= 0) return; // just used -> empty
    const start = -Math.PI / 2; // top (12 o'clock)
    const arcColor = this.lighten(color, 0.4);
    if (frac >= 1) {
      gfx.circle(0, 0, radius).stroke({ width: 2, color: arcColor, alpha: 0.76 });
    } else {
      gfx.moveTo(0, -radius); // begin at the arc's start -> no line from the centre
      gfx.arc(0, 0, radius, start, start + frac * Math.PI * 2).stroke({ width: 2, color: arcColor, alpha: 0.76 });
    }
  }

  // Thin line from the token toward the cursor, out to the projectile's range.
  private drawAimLine(gfx: Graphics, self: { x: number; y: number }, rangePx: number, color: number): void {
    const t = this.aimTarget;
    if (t === undefined) return;
    const dx = t.x - self.x;
    const dy = t.y - self.y;
    const len = Math.hypot(dx, dy);
    if (len < 1) return;
    const ux = dx / len;
    const uy = dy / len;
    const start = 12; // px: clear of the token and its glow
    if (rangePx <= start) return;
    gfx
      .moveTo(ux * start, uy * start)
      .lineTo(ux * rangePx, uy * rangePx)
      .stroke({ width: 1.5, color: this.lighten(color, 0.4), alpha: 0.45 });
    gfx.circle(ux * rangePx, uy * rangePx, 2.5).fill({ color: this.lighten(color, 0.4), alpha: 0.7 });
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
  // under fog of war (visible cells bright — neutral ones dark-but-visible, not
  // a black void —, explored ones dimmed, unexplored ones covered) with a
  // viewport rect and player dots. Redrawn each frame — small (~a few hundred cells).
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
    const c1 = Math.min(this.state.territory.cols - 1, Math.floor((originX + regionW) / UNITS_PER_CELL));
    const r0 = Math.max(0, Math.floor(originY / UNITS_PER_CELL));
    const r1 = Math.min(this.state.territory.rows - 1, Math.floor((originY + regionH) / UNITS_PER_CELL));

    const t = this.minimapTerritory;
    t.clear();
    for (let row = r0; row <= r1; row++) {
      for (let col = c0; col <= c1; col++) {
        const i = row * this.state.territory.cols + col;
        const look = MINIMAP_CELL[this.state.fog.sightAt(i)];
        const o = look.owned > 0 ? (this.state.territory.owners[i] ?? 0) : 0; // unexplored: nothing known
        const x = toX(col * UNITS_PER_CELL);
        const y = toY(row * UNITS_PER_CELL);
        if (o !== 0) {
          const color = this.state.factionColor(o);
          if (color !== undefined) t.rect(x, y, cellPx + 0.6, cellPx + 0.6).fill({ color, alpha: look.owned });
        } else {
          t.rect(x, y, cellPx + 0.6, cellPx + 0.6).fill({ color: look.color, alpha: look.alpha });
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

    // Viewport rect, the capitals' markers (known through the fog), player dots.
    const g = this.minimapOverlay;
    g.clear();
    g.rect(toX(cam.centerX - viewW / 2), toY(cam.centerY - viewH / 2), viewW * mm, viewH * mm)
      .stroke({ width: 1, color: 0xffffff, alpha: 0.7 });
    for (const c of this.state.capitals) {
      const [wx, wy] = capitalCentre(c.cell, this.state.territory.cols);
      const x = toX(wx);
      const y = toY(wy);
      if (x < 0 || y < 0 || x > this.minimapW || y > this.minimapH) continue;
      drawCapitalMarker(g, x, y, 3.5, this.state.factionColor(c.factionId) ?? 0xd8d8d0, 1);
    }
    for (const [id, mp] of this.state.players) {
      if (mp.hp <= 0) continue;
      const isSelf = id === this.state.myId;
      const pos = isSelf ? this.selfPredicted : (this.remotePositions.get(id) ?? { x: mp.x, y: mp.y });
      const color = isSelf ? 0xffffff : (this.state.factionColor(this.state.roster.factionOf(id)) ?? 0xaaaaaa);
      g.circle(toX(pos.x), toY(pos.y), isSelf ? 2.5 : 1.8).fill({ color });
    }
  }
}
