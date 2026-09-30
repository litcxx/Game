import { closeError, errorText } from "../errors.js";
import { ErrorCode, type ServerMessage } from "../gen/game/v1/protocol_pb.js";

// The connection's life around one GameClient: join with the saved name and
// session token, keep the token each Welcome gives, and after a lost connection
// reconnect with backoff — the same character comes back (GAME-008). What the
// player sees of it comes out as a SessionStatus; the game gets the messages.

// Where the name and the token are kept between visits (localStorage).
export const NAME_KEY = "territory.name";
export const TOKEN_KEY = "territory.token";

// Reconnect backoff: 1, 2, 4, 8 s, then this for good.
export const MAX_RECONNECT_DELAY_MS = 15_000;

// The wait before reconnect attempt `attempt` (1-based).
export function reconnectDelayMs(attempt: number): number {
  return Math.min(1000 * 2 ** (attempt - 1), MAX_RECONNECT_DELAY_MS);
}

// What a close means: a lost connection to reconnect, a name to change, or the
// end of the game on this page.
export type CloseOutcome = "retry" | "rename" | "fatal";

// Errors that are about the connection, not the player: worth another try.
const RETRIED_ERRORS: ReadonlySet<ErrorCode> = new Set([
  ErrorCode.IDLE_TIMEOUT,
  ErrorCode.HANDSHAKE_TIMEOUT,
  ErrorCode.SERVER_SHUTDOWN,
]);

// A close without an error code (the network, the silence watchdog) is retried;
// SESSION_REPLACED is not — two tabs would take the character back and forth.
export function closeOutcome(closeCode: number): CloseOutcome {
  const error = closeError(closeCode);
  if (error === undefined || RETRIED_ERRORS.has(error)) return "retry";
  return error === ErrorCode.INVALID_NAME ? "rename" : "fatal";
}

export type SessionStatus =
  | { kind: "needName"; error?: string } // ask for a name (why, if one was refused)
  | { kind: "connecting" } // joining; nothing to show yet
  | { kind: "playing" } // Welcome came
  | { kind: "reconnecting"; attempt: number; retryInMs: number } // lost; the next try in
  | { kind: "away" } // lost while the tab is hidden; reconnects when it is shown
  | { kind: "failed"; text: string }; // over: why

// The transport as the session uses it (GameClient).
export interface SessionClient {
  connect(name: string, sessionToken: string): void;
}

export interface KeyValueStore {
  getItem(key: string): string | null;
  setItem(key: string, value: string): void;
}

export interface Timers {
  setTimeout(fn: () => void, ms: number): unknown;
  clearTimeout(handle: unknown): void;
}

export interface SessionOptions<C extends SessionClient> {
  createClient: (onMessage: (msg: ServerMessage) => void, onClose: (code: number) => void) => C;
  store: KeyValueStore;
  timers: Timers;
  onMessage: (msg: ServerMessage) => void;
  onStatus: (status: SessionStatus) => void;
}

export function memoryStore(): KeyValueStore {
  const items = new Map<string, string>();
  return {
    getItem: (key) => items.get(key) ?? null,
    setItem: (key, value) => void items.set(key, value),
  };
}

// localStorage when the browser allows it; a blocked one (private mode, cookies
// off) keeps things for this page only.
export function browserStore(storage?: Storage): KeyValueStore {
  const page = memoryStore();
  const saved = (key: string): string | null => {
    try {
      return (storage ?? localStorage).getItem(key);
    } catch {
      return null;
    }
  };
  return {
    getItem: (key) => page.getItem(key) ?? saved(key),
    setItem: (key, value) => {
      page.setItem(key, value);
      try {
        (storage ?? localStorage).setItem(key, value);
      } catch {
        // kept for this page only
      }
    },
  };
}

type Phase = "idle" | "connecting" | "playing" | "waiting" | "away" | "stopped";

export class Session<C extends SessionClient = SessionClient> {
  readonly client: C;
  private playerName: string | undefined;
  private token: string;
  private phase: Phase = "idle";
  private attempt = 0; // reconnect attempts since the last Welcome
  private retryTimer: unknown;
  private hidden = false; // the tab: see setHidden

  constructor(private readonly options: SessionOptions<C>) {
    this.client = options.createClient(
      (msg) => this.onMessage(msg),
      (code) => this.onClose(code),
    );
    this.playerName = options.store.getItem(NAME_KEY) ?? undefined;
    this.token = options.store.getItem(TOKEN_KEY) ?? "";
  }

  // The name to join with: the saved one, or the last one tried.
  get name(): string | undefined {
    return this.playerName;
  }

  // Join with the saved name and token, or ask for a name.
  start(): void {
    if (this.playerName === undefined) this.options.onStatus({ kind: "needName" });
    else this.begin();
  }

  // Join under a name the player chose: a new character, so no token.
  join(name: string): void {
    this.playerName = name;
    this.token = "";
    this.begin();
  }

  // The tab is hidden (or shown again). A hidden tab sends no input and pings
  // rarely, so the server closes it as idle; reconnecting then would only loop,
  // the character standing unplayed. So while hidden a lost connection waits
  // (away) and comes back the moment the tab is shown — in the grace period as
  // the same character. A live connection is kept: a short look away is fine.
  setHidden(hidden: boolean): void {
    this.hidden = hidden;
    if (hidden && this.phase === "waiting") {
      this.options.timers.clearTimeout(this.retryTimer);
      this.goAway();
    } else if (!hidden && this.phase === "away") {
      this.attempt = 1;
      this.options.onStatus({ kind: "reconnecting", attempt: this.attempt, retryInMs: 0 });
      this.connect();
    }
  }

  // Reconnect now instead of waiting out the backoff (the network is back).
  retryNow(): void {
    if (this.phase !== "waiting") return;
    this.options.timers.clearTimeout(this.retryTimer);
    this.connect();
  }

  private begin(): void {
    this.attempt = 0;
    this.options.onStatus({ kind: "connecting" });
    this.connect();
  }

  private connect(): void {
    this.phase = "connecting";
    this.client.connect(this.playerName ?? "", this.token);
  }

  private onMessage(msg: ServerMessage): void {
    this.options.onMessage(msg);
    if (msg.payload.case !== "welcome") return;
    // The server took the name; the token brings this character back later.
    this.token = msg.payload.value.sessionToken;
    this.options.store.setItem(NAME_KEY, this.playerName ?? "");
    this.options.store.setItem(TOKEN_KEY, this.token);
    this.attempt = 0;
    this.phase = "playing";
    this.options.onStatus({ kind: "playing" });
  }

  private onClose(code: number): void {
    if (this.phase === "stopped") return;
    switch (closeOutcome(code)) {
      case "retry": {
        if (this.hidden) {
          this.goAway();
          break;
        }
        this.phase = "waiting";
        this.attempt++;
        const retryInMs = reconnectDelayMs(this.attempt);
        this.retryTimer = this.options.timers.setTimeout(() => this.connect(), retryInMs);
        this.options.onStatus({ kind: "reconnecting", attempt: this.attempt, retryInMs });
        break;
      }
      case "rename":
        this.phase = "idle";
        this.options.onStatus({ kind: "needName", error: errorText(ErrorCode.INVALID_NAME) });
        break;
      case "fatal":
        this.phase = "stopped";
        this.options.onStatus({ kind: "failed", text: errorText(closeError(code)!) });
        break;
    }
  }

  private goAway(): void {
    this.phase = "away";
    this.options.onStatus({ kind: "away" });
  }
}
