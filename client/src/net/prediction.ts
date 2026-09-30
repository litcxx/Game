export interface Vec2 {
  x: number;
  y: number;
}
export interface Bounds {
  maxX: number;
  maxY: number;
}
export interface InputSample {
  moveX: number;
  moveY: number;
  capturing: boolean;
  attack: boolean;
  ability: number; // Ability.id of the active bar slot
  aimX: number; // aim direction (unit vector x 1000), see aimVector()
  aimY: number;
}
export interface PendingInput extends InputSample {
  seq: number;
}

// MUST match the server's World::update integration exactly.
export function integrate(
  pos: Vec2,
  moveX: number,
  moveY: number,
  dt: number,
  speed: number,
  bounds: Bounds,
): Vec2 {
  if (moveX === 0 && moveY === 0) return pos;
  const len = Math.hypot(moveX, moveY);
  return {
    x: Math.max(0, Math.min(pos.x + (moveX / len) * speed * dt, bounds.maxX)),
    y: Math.max(0, Math.min(pos.y + (moveY / len) * speed * dt, bounds.maxY)),
  };
}

// A misprediction smaller than this (world units) is not counted as a
// correction: a quarter of a player's radius, below what the eye notices.
export const CORRECTION_UNITS = 4;

// Corrections since they were last taken: how many, and the largest (units).
export interface Corrections {
  count: number;
  max: number;
}

// Predicts the local player and reconciles against authoritative snapshots.
// `renderPosition` = predicted + a correction error that decays to zero, so a
// divergence eases in over ~100 ms instead of popping.
export class Predictor {
  private predicted: Vec2 = { x: 0, y: 0 };
  private error: Vec2 = { x: 0, y: 0 };
  private pending: PendingInput[] = [];
  private nextSeq = 1;
  private corrections: Corrections = { count: 0, max: 0 };

  constructor(
    private readonly speed: number,
    private readonly fixedDt: number,
    private readonly bounds: Bounds,
  ) {}

  // Hard snap (spawn / respawn / death / teleport): no easing.
  reset(pos: Vec2): void {
    this.predicted = { x: pos.x, y: pos.y };
    this.error = { x: 0, y: 0 };
    this.pending = [];
  }

  get position(): Vec2 {
    return this.predicted;
  }

  // What to draw: predicted plus the (decaying) correction error.
  get renderPosition(): Vec2 {
    return { x: this.predicted.x + this.error.x, y: this.predicted.y + this.error.y };
  }

  // Apply one fixed-step input locally; returns the frame to send.
  step(sample: InputSample): PendingInput {
    const frame: PendingInput = { seq: this.nextSeq++, ...sample };
    this.predicted = integrate(this.predicted, sample.moveX, sample.moveY, this.fixedDt, this.speed, this.bounds);
    this.pending.push(frame);
    return frame;
  }

  // Drop acked inputs, replay the remainder from the authoritative pos, and keep
  // the currently-displayed point continuous by folding the delta into `error`.
  // A prediction off by more than CORRECTION_UNITS counts as a correction.
  reconcile(authoritative: Vec2, lastInputSeq: number): void {
    const displayed = this.renderPosition;
    this.pending = this.pending.filter((f) => f.seq > lastInputSeq);
    let pos: Vec2 = { x: authoritative.x, y: authoritative.y };
    for (const f of this.pending) {
      pos = integrate(pos, f.moveX, f.moveY, this.fixedDt, this.speed, this.bounds);
    }
    const off = Math.hypot(this.predicted.x - pos.x, this.predicted.y - pos.y);
    if (off > CORRECTION_UNITS) {
      const { count, max } = this.corrections;
      this.corrections = { count: count + 1, max: Math.max(max, Math.round(off)) };
    }
    this.predicted = pos;
    this.error = { x: displayed.x - pos.x, y: displayed.y - pos.y };
  }

  // The corrections since the last call; counting starts over.
  takeCorrections(): Corrections {
    const taken = this.corrections;
    this.corrections = { count: 0, max: 0 };
    return taken;
  }

  // Ease the correction error toward zero (tau ≈ 80 ms). Call once per frame.
  decayError(dtSeconds: number): void {
    const a = Math.exp(-dtSeconds / 0.08);
    this.error = { x: this.error.x * a, y: this.error.y * a };
  }
}
