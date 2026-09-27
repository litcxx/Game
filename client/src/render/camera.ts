export const UNITS_PER_CELL = 100;

export type CameraMode = "follow" | "map";

// Maps world units <-> screen pixels for two modes:
//   follow — fixed zoom of exactly FOLLOW_COLS cells across the width, centred on
//            a target (the player), clamped so the view never leaves the map;
//   map    — the whole map fit into the viewport.
// Cells stay square (uniform scale). Pure logic, no rendering — unit-testable.
const FOLLOW_COLS = 15;

export class Camera {
  mode: CameraMode = "follow";
  scale = 1; // screen pixels per world unit

  private screenW = 1;
  private screenH = 1;
  private cols = 1; // map size in cells
  private rows = 1;
  private worldW = UNITS_PER_CELL; // map size in world units
  private worldH = UNITS_PER_CELL;
  private camX = 0; // world coords currently at the screen centre
  private camY = 0;
  private targetX = 0; // desired follow centre (self position)
  private targetY = 0;

  setScreen(width: number, height: number): void {
    this.screenW = Math.max(1, width);
    this.screenH = Math.max(1, height);
    this.recompute();
  }

  setWorld(cols: number, rows: number): void {
    this.cols = Math.max(1, cols);
    this.rows = Math.max(1, rows);
    this.worldW = this.cols * UNITS_PER_CELL;
    this.worldH = this.rows * UNITS_PER_CELL;
    this.recompute();
  }

  setTarget(x: number, y: number): void {
    this.targetX = x;
    this.targetY = y;
    this.recompute();
  }

  setMode(mode: CameraMode): void {
    this.mode = mode;
    this.recompute();
  }

  toggle(): void {
    this.setMode(this.mode === "follow" ? "map" : "follow");
  }

  get centerX(): number {
    return this.camX;
  }
  get centerY(): number {
    return this.camY;
  }

  worldToScreen(x: number, y: number): [number, number] {
    return [
      (x - this.camX) * this.scale + this.screenW / 2,
      (y - this.camY) * this.scale + this.screenH / 2,
    ];
  }

  screenToWorld(sx: number, sy: number): [number, number] {
    return [
      (sx - this.screenW / 2) / this.scale + this.camX,
      (sy - this.screenH / 2) / this.scale + this.camY,
    ];
  }

  // Inclusive cell-index range currently on screen, clamped to the map.
  visibleCols(): [number, number] {
    const halfW = this.screenW / (2 * this.scale);
    return [
      Math.max(0, Math.floor((this.camX - halfW) / UNITS_PER_CELL)),
      Math.min(this.cols - 1, Math.floor((this.camX + halfW) / UNITS_PER_CELL)),
    ];
  }

  visibleRows(): [number, number] {
    const halfH = this.screenH / (2 * this.scale);
    return [
      Math.max(0, Math.floor((this.camY - halfH) / UNITS_PER_CELL)),
      Math.min(this.rows - 1, Math.floor((this.camY + halfH) / UNITS_PER_CELL)),
    ];
  }

  private recompute(): void {
    if (this.mode === "map") {
      this.scale = Math.min(this.screenW / this.worldW, this.screenH / this.worldH);
      this.camX = this.worldW / 2;
      this.camY = this.worldH / 2;
      return;
    }
    // follow: exactly FOLLOW_COLS cells across the width
    this.scale = this.screenW / (FOLLOW_COLS * UNITS_PER_CELL);
    this.camX = this.clampAxis(this.targetX, this.worldW, this.screenW);
    this.camY = this.clampAxis(this.targetY, this.worldH, this.screenH);
  }

  // Keep the viewport inside [0, worldSize]; if the map is smaller than the view
  // on this axis, centre it.
  private clampAxis(target: number, worldSize: number, screenSize: number): number {
    const half = screenSize / (2 * this.scale);
    if (worldSize <= 2 * half) return worldSize / 2;
    return Math.min(Math.max(target, half), worldSize - half);
  }
}
