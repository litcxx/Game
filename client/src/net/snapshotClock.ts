// How fast the offset may grow: ms per second of snapshots.
export const CREEP_MS_PER_S = 1;
// How far a stamp may be before its arrival: INTERP_DELAY_MS (100) less the
// snapshot interval (50), so the render time still falls between two snapshots.
export const MAX_LAG_MS = 50;

// Places each snapshot on the local clock (performance.now()) for interpolation.
// Stamped with its arrival time, network jitter turned into uneven motion — at
// ±15 ms a running player wobbled ~7 px against the camera, its name unreadable
// (playtest #0). Stamped with the server's time plus one offset, motion stays
// even. The offset (arrival − server time) is the smallest seen — the fastest
// delivery — so no stamp is later than its arrival and the interpolation delay
// covers the jitter; it creeps up by CREEP_MS_PER_S to follow a latency that has
// grown for good — and jumps up at once so that no stamp is more than MAX_LAG_MS
// before its arrival (a latency grown by more than the delay covers: creeping,
// remote players would freeze and jump for minutes). Stamps never go back, even
// when the latency drops.
export class SnapshotClock {
  private offset: number | undefined;
  private lastArrivalMs = 0;
  private lastStamp = -Infinity;

  // Local time for a snapshot of server time `serverMs` that arrived at `arrivalMs`.
  stamp(serverMs: number, arrivalMs: number): number {
    const sample = arrivalMs - serverMs;
    const creep = ((arrivalMs - this.lastArrivalMs) / 1000) * CREEP_MS_PER_S;
    this.offset = this.offset === undefined ? sample : Math.min(sample, this.offset + creep);
    this.offset = Math.max(this.offset, sample - MAX_LAG_MS);
    this.lastArrivalMs = arrivalMs;
    this.lastStamp = Math.max(serverMs + this.offset, this.lastStamp + 1);
    return this.lastStamp;
  }

  // A new server (a reconnect may land on a restarted one): its ticks start over.
  reset(): void {
    this.offset = undefined;
    this.lastStamp = -Infinity;
  }
}
