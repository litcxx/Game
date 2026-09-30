import { Graphics } from "pixi.js";

export interface ProjectileSprite {
  x: number; // world units (see GameState.shotsAt)
  y: number;
  vx: number; // units/s: the trail points against it
  vy: number;
  factionId: number;
  radius: number; // world units
  alpha: number; // 1 in flight; less while fading out of sight
}

// Projectiles in flight: a glowing dot in the shooter's faction colour, sized by
// its real radius (at least 2.5 px) so what you see is what can hit, with a
// short trail behind it; one flying out of sight fades out.
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
      const a = p.alpha;
      const speed = Math.hypot(p.vx, p.vy);
      if (speed > 0) {
        const trail = Math.min(28, speed * 0.035 * scale); // ~35 ms of flight
        g.moveTo(sx - (p.vx / speed) * trail, sy - (p.vy / speed) * trail)
          .lineTo(sx, sy)
          .stroke({ width: r * 1.2, color, alpha: 0.35 * a, cap: "round" });
      }
      g.circle(sx, sy, r * 2).fill({ color, alpha: 0.15 * a });
      g.circle(sx, sy, r).fill({ color, alpha: a });
      g.circle(sx, sy, r * 0.45).fill({ color: 0xffffff, alpha: 0.85 * a });
    }
  }
}
