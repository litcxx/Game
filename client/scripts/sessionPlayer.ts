// A player as the page runs one — a Session over a real GameClient, with memory
// in place of localStorage — recording the messages and statuses it gets. For
// the smokes on joining and reconnecting.
import type { PlayerState, ServerMessage, Welcome } from "../src/gen/game/v1/protocol_pb.js";
import { GameClient } from "../src/net/client.js";
import { memoryStore, Session, type KeyValueStore, type SessionStatus } from "../src/net/session.js";

export class SessionPlayer {
  readonly messages: ServerMessage[] = [];
  readonly statuses: SessionStatus[] = [];
  readonly session: Session<GameClient>;
  private readonly waiters: (() => void)[] = [];
  private inputSeq = 0; // the last input frame's number

  // Players sharing a store are tabs of one browser.
  constructor(
    url: string,
    readonly store: KeyValueStore = memoryStore(),
  ) {
    this.session = new Session({
      createClient: (onMessage, onClose) => new GameClient(url, onMessage, onClose),
      store,
      timers: {
        setTimeout: (fn, ms) => setTimeout(fn, ms),
        clearTimeout: (handle) => clearTimeout(handle as ReturnType<typeof setTimeout>),
      },
      onMessage: (msg) => {
        this.messages.push(msg);
        this.wake();
      },
      onStatus: (status) => {
        this.statuses.push(status);
        this.wake();
      },
    });
  }

  get client(): GameClient {
    return this.session.client;
  }

  // One input frame walking along x (moveX -1, 0 or 1; nothing else held). The
  // player numbers its frames itself, as the game's Predictor does.
  walk(moveX: number): void {
    this.client.sendInputFrames([
      { inputSeq: ++this.inputSeq, moveX, moveY: 0, capturing: false, attack: false, abilityId: 0, aimX: 0, aimY: 0 },
    ]);
  }

  // The first value `pick` finds, now or as messages and statuses come, or
  // undefined after `timeoutMs`.
  until<T>(pick: () => T | undefined, timeoutMs = 3000): Promise<T | undefined> {
    return new Promise((resolve) => {
      const timer = setTimeout(() => resolve(undefined), timeoutMs);
      const check = () => {
        const found = pick();
        if (found === undefined) {
          this.waiters.push(check);
          return;
        }
        clearTimeout(timer);
        resolve(found);
      };
      check();
    });
  }

  // Every Welcome so far: one per session (the first join, each reconnect).
  welcomes(): Welcome[] {
    return this.messages.flatMap((m) => (m.payload.case === "welcome" ? [m.payload.value] : []));
  }

  // The last snapshot's view of player `id` and of oneself, among the messages
  // after the `session`-th Welcome (0-based).
  lastSeen(id: number, session = 0): { self: PlayerState; life: number | undefined } | undefined {
    const welcomeAt = this.messages.filter((m) => m.payload.case === "welcome")[session];
    const from = welcomeAt === undefined ? this.messages.length : this.messages.indexOf(welcomeAt);
    for (let i = this.messages.length - 1; i > from; i--) {
      const m = this.messages[i]!;
      if (m.payload.case !== "snapshot") continue;
      const self = m.payload.value.players.find((p) => p.id === id);
      if (self) return { self, life: m.payload.value.you?.life };
    }
    return undefined;
  }

  // Resolves once player `id` stands still: the same position in two snapshots in a row.
  standing(id: number, session = 0): Promise<PlayerState | undefined> {
    let previous: PlayerState | undefined;
    return this.until(() => {
      const seen = this.lastSeen(id, session);
      if (!seen) return undefined;
      const still = previous && previous !== seen.self && previous.x === seen.self.x && previous.y === seen.self.y;
      previous = seen.self;
      return still ? seen.self : undefined;
    });
  }

  private wake(): void {
    for (const waiter of this.waiters.splice(0)) waiter();
  }
}
