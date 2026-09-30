import { Graphics } from "pixi.js";

export interface ProjectileSprite {
  x: number; // world units (see GameState.shotsAt)
  y: number;
  vx: number; // units/s: the trail points against it
  vy: number;
  factionId: number;
  radius: number; // world units
}

// Projectiles in flight: a glowing dot in the shooter's faction colour, sized by
// its real radius (at least 2.5 px) so what you see is what can hit, with a
// short trail behind it.
export class ProjectileView {
  readonly gfx = new Graphics();

  draw(
    items: readonly ProjectileSprite[],
    toScreen: (x: number, y: number) => [number, number],
    scale: number,
    colorOf: (factionId: number) => number,
  ): void {
    const g = this.gfx;
    g.clear();
    for (const p of items) {
      const [sx, sy] = toScreen(p.x, p.y);
      const r = Math.max(2.5, p.radius * scale);
      const color = colorOf(p.factionId);
      const speed = Math.hypot(p.vx, p.vy);
      if (speed > 0) {
        const trail = Math.min(28, speed * 0.035 * scale); // ~35 ms of flight
        g.moveTo(sx - (p.vx / speed) * trail, sy - (p.vy / speed) * trail)
          .lineTo(sx, sy)
          .stroke({ width: r * 1.2, color, alpha: 0.35, cap: "round" });
      }
      g.circle(sx, sy, r * 2).fill({ color, alpha: 0.15 });
      g.circle(sx, sy, r).fill(color);
      g.circle(sx, sy, r * 0.45).fill({ color: 0xffffff, alpha: 0.85 });
    }
  }
}
