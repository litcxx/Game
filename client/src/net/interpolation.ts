export interface RemoteState {
  x: number;
  y: number;
}

interface Snap {
  time: number;
  states: Map<number, RemoteState>;
}

// Buffers recent snapshots and returns positions interpolated `delayMs` in the past.
export class InterpolationBuffer {
  private snaps: Snap[] = [];

  constructor(
    private readonly delayMs: number,
    private readonly keepMs = 1000,
  ) {}

  push(timeMs: number, states: Map<number, RemoteState>): void {
    this.snaps.push({ time: timeMs, states });
    const cutoff = timeMs - this.keepMs;
    while (this.snaps.length > 2 && this.snaps[0]!.time < cutoff) this.snaps.shift();
  }

  // Ids on screen at renderTime = nowMs - delayMs: those in either snapshot
  // around it — an entity gone from the newer one (e.g. a projectile that hit)
  // stays until the render time passes its last known position.
  ids(nowMs: number): Set<number> {
    const out = new Set<number>();
    const [s0, s1] = this.bracket(nowMs - this.delayMs);
    for (const s of [s0, s1]) if (s) for (const id of s.states.keys()) out.add(id);
    return out;
  }

  // Position of `id` at renderTime = nowMs - delayMs; undefined if unknown.
  sample(id: number, nowMs: number): RemoteState | undefined {
    if (this.snaps.length === 0) return undefined;
    const t = nowMs - this.delayMs;
    const [s0, s1] = this.bracket(t);
    if (s0 && s1 && s0 !== s1) {
      const a = s0.states.get(id);
      const b = s1.states.get(id);
      if (a && b) {
        const f = (t - s0.time) / (s1.time - s0.time);
        return { x: a.x + (b.x - a.x) * f, y: a.y + (b.y - a.y) * f };
      }
      return b ?? a; // one side missing (joined/left) -> use whichever exists
    }
    return (s1 ?? s0)!.states.get(id); // clamp to the nearest end
  }

  // The last snapshot at or before t and the first at or after it.
  private bracket(t: number): [Snap | undefined, Snap | undefined] {
    let s0: Snap | undefined;
    let s1: Snap | undefined;
    for (const s of this.snaps) {
      if (s.time <= t) s0 = s;
      if (s.time >= t && s1 === undefined) s1 = s;
    }
    return [s0, s1];
  }
}
