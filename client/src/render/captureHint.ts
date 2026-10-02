import { AlphaFilter, BlurFilter, Container, Graphics } from "pixi.js";

import { cellBorders } from "./borders.js";

// The cells you may capture (GAME-021, GameState.mayCapture): their borders glow
// softly in your faction's colour — a thin line on the gridline and a faint light
// around it; two such neighbours share one line.
const LINE = { width: 1, alpha: 0.6 }; // px
// Drawn opaque, blurred, then faded as a whole, so the corners are no brighter.
const GLOW = { width: 3, blur: 3, alpha: 0.5 };
const MIN_CELL_PX = 12; // smaller (the full-map view): nothing to mark

export class CaptureHintView {
  readonly gfx = new Container();
  private readonly glow = new Graphics();
  // A blur each frame is costly: the glow is kept as a texture, redrawn only when
  // the cells, their size or the colour change; the camera only moves it. So it is
  // drawn whole, not cut to the screen of the moment (clipToViewport).
  private readonly glowLayer = new Container();
  private readonly line = new Graphics();
  private drawn = ""; // what the layers hold: cell size, colour, cells

  constructor() {
    this.glow.filters = [
      new BlurFilter({ strength: GLOW.blur, quality: 3, clipToViewport: false }),
      new AlphaFilter({ alpha: GLOW.alpha, clipToViewport: false }),
    ];
    this.glowLayer.addChild(this.glow);
    this.glowLayer.cacheAsTexture(true);
    this.gfx.addChild(this.glowLayer, this.line);
  }

  // `cols`: the map width in cells; `cellPx`: a cell's size on screen.
  draw(
    cells: readonly number[],
    cols: number,
    toScreen: (x: number, y: number) => [number, number],
    cellPx: number,
    color: number,
  ): void {
    this.gfx.visible = cells.length > 0 && cellPx >= MIN_CELL_PX;
    if (!this.gfx.visible) return;
    // Drawn from the map's corner, where the camera puts it.
    const [x, y] = toScreen(0, 0);
    this.gfx.position.set(x, y);
    const what = `${cellPx}|${color}|${cells.join(",")}`;
    if (what === this.drawn) return;
    this.drawn = what;
    this.glow.clear();
    this.line.clear();
    for (const [x0, y0, x1, y1] of cellBorders(cells, cols)) {
      this.glow.moveTo(x0 * cellPx, y0 * cellPx).lineTo(x1 * cellPx, y1 * cellPx);
      this.line.moveTo(x0 * cellPx, y0 * cellPx).lineTo(x1 * cellPx, y1 * cellPx);
    }
    this.glow.stroke({ width: GLOW.width, color });
    this.line.stroke({ width: LINE.width, color, alpha: LINE.alpha });
    this.glowLayer.updateCacheTexture();
  }
}
