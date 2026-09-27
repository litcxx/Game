import { Container, Graphics, Text } from "pixi.js";

import { effectProgress, type Effect } from "../effects.js";

const SHIELD = 0xffe9a8; // pale gold: a block reads the same for every faction

// Draws the timed effects (see ../effects.ts) on the players' tokens:
//   swing   — a ring expanding to the melee reach in the swinger's colour;
//   shield  — a bright ring around the token while the block holds;
//   blocked — a white burst plus a rising "БЛОК" on the defender.
export class EffectsView {
  readonly root = new Container();
  private readonly gfx = new Graphics();
  private readonly labels: Text[] = []; // pooled "БЛОК" texts
  private effects: Effect[] = [];

  constructor() {
    this.root.addChild(this.gfx);
  }

  add(effects: readonly Effect[]): void {
    this.effects.push(...effects);
  }

  draw(
    nowMs: number,
    positionOf: (playerId: number) => { x: number; y: number } | undefined,
    toScreen: (x: number, y: number) => [number, number],
    scale: number,
    colorOf: (playerId: number) => number,
  ): void {
    const g = this.gfx;
    g.clear();
    this.effects = this.effects.filter((e) => nowMs < e.startMs + e.durationMs); // drop finished
    let used = 0;
    for (const e of this.effects) {
      const t = effectProgress(e, nowMs);
      if (t === undefined) continue; // scheduled for later (remote delay)
      const pos = positionOf(e.playerId);
      if (pos === undefined) continue;
      const [sx, sy] = toScreen(pos.x, pos.y);
      if (e.kind === "swing") {
        const reach = Math.max(12, e.radius * scale);
        const r = 10 + (reach - 10) * (1 - (1 - t) * (1 - t)); // ease-out
        const color = lighten(colorOf(e.playerId), 0.3);
        g.circle(sx, sy, r).fill({ color, alpha: 0.12 * (1 - t) });
        g.circle(sx, sy, r).stroke({ width: 1 + 3 * (1 - t), color, alpha: 0.85 * (1 - t) });
      } else if (e.kind === "shield") {
        const a = 1 - 0.35 * t;
        g.circle(sx, sy, 26).stroke({ width: 7, color: SHIELD, alpha: 0.18 * a });
        g.circle(sx, sy, 20).stroke({ width: 3, color: SHIELD, alpha: 0.95 * a });
      } else {
        g.circle(sx, sy, 12 + 20 * t).stroke({ width: 2, color: 0xffffff, alpha: 1 - t });
        const label = this.label(used++);
        label.position.set(Math.round(sx), Math.round(sy - 34 - 18 * t));
        label.alpha = 1 - t;
        label.visible = true;
      }
    }
    for (let i = used; i < this.labels.length; i++) this.labels[i]!.visible = false;
  }

  private label(i: number): Text {
    let t = this.labels[i];
    if (t === undefined) {
      t = new Text({
        text: "БЛОК",
        style: {
          fill: "#fff4d0",
          fontFamily: "monospace",
          fontWeight: "bold",
          fontSize: 13,
          stroke: { color: 0x14141a, width: 3 },
        },
      });
      t.anchor.set(0.5, 1);
      this.root.addChild(t);
      this.labels[i] = t;
    }
    return t;
  }
}

function lighten(color: number, amt: number): number {
  const r = (color >> 16) & 0xff;
  const g = (color >> 8) & 0xff;
  const b = color & 0xff;
  const m = (c: number) => Math.round(c + (255 - c) * amt);
  return (m(r) << 16) | (m(g) << 8) | m(b);
}
