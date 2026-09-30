// Pure checks for the connection's life (src/net/session.ts) with a fake client,
// fake timers and an in-memory store — no socket: joining with the saved name and
// token, keeping the token from Welcome, reconnecting with backoff after a lost
// connection, and when not to (a fatal error, a name to change, a hidden tab —
// until the player is back). Run:
//   npx tsx scripts/session_check.ts
import { create } from "@bufbuild/protobuf";

import { errorText } from "../src/errors.js";
import { ErrorCode, ServerMessageSchema, type ServerMessage } from "../src/gen/game/v1/protocol_pb.js";
import { STALE_CLOSE_CODE } from "../src/net/client.js";
import {
  browserStore,
  closeOutcome,
  memoryStore,
  NAME_KEY,
  reconnectDelayMs,
  Session,
  TOKEN_KEY,
  type KeyValueStore,
  type SessionClient,
  type SessionStatus,
} from "../src/net/session.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};
const same = (a: unknown, b: unknown) => JSON.stringify(a) === JSON.stringify(b);

// Records connects; the test plays the server through welcome() and close().
class FakeClient implements SessionClient {
  readonly connects: { name: string; token: string }[] = [];
  constructor(
    private readonly onMessage: (msg: ServerMessage) => void,
    private readonly onClose: (code: number) => void,
  ) {}
  connect(name: string, sessionToken: string): void {
    this.connects.push({ name, token: sessionToken });
  }
  welcome(playerId: number, sessionToken: string, resumed = false): void {
    this.onMessage(create(ServerMessageSchema, { payload: { case: "welcome", value: { playerId, sessionToken, resumed } } }));
  }
  close(code: number): void {
    this.onClose(code);
  }
}

// setTimeout / clearTimeout on a clock the test moves.
class FakeTimers {
  private now = 0;
  private nextId = 1;
  private readonly pending = new Map<number, { at: number; fn: () => void }>();
  setTimeout = (fn: () => void, ms: number): unknown => {
    const id = this.nextId++;
    this.pending.set(id, { at: this.now + ms, fn });
    return id;
  };
  clearTimeout = (handle: unknown): void => {
    this.pending.delete(handle as number);
  };
  advance(ms: number): void {
    this.now += ms;
    for (const [id, t] of [...this.pending]) {
      if (t.at <= this.now) {
        this.pending.delete(id);
        t.fn();
      }
    }
  }
}

const setUp = (store: KeyValueStore = memoryStore()) => {
  const timers = new FakeTimers();
  const statuses: SessionStatus[] = [];
  const messages: ServerMessage[] = [];
  const session = new Session({
    createClient: (onMessage, onClose) => new FakeClient(onMessage, onClose),
    store,
    timers,
    onMessage: (m) => messages.push(m),
    onStatus: (s) => statuses.push(s),
  });
  return { session, client: session.client, timers, statuses, messages, store, last: () => statuses.at(-1) };
};

// --- The policy -------------------------------------------------------------------
check(
  "backoff: 1, 2, 4, 8 s, then 15 s for good",
  same([1, 2, 3, 4, 5, 6, 20].map(reconnectDelayMs), [1000, 2000, 4000, 8000, 15000, 15000, 15000]),
);
check(
  "a lost connection is retried: a plain close, an abnormal one, a silent one",
  [1000, 1001, 1005, 1006, STALE_CLOSE_CODE].every((code) => closeOutcome(code) === "retry"),
);
check(
  "connection trouble the server reports is retried too",
  [ErrorCode.IDLE_TIMEOUT, ErrorCode.HANDSHAKE_TIMEOUT, ErrorCode.SERVER_SHUTDOWN].every(
    (code) => closeOutcome(4000 + code) === "retry",
  ),
);
check("a refused name asks for another", closeOutcome(4000 + ErrorCode.INVALID_NAME) === "rename");
check(
  "other fatal errors stop: another tab took over, an old build, a kick, a flood",
  [ErrorCode.SESSION_REPLACED, ErrorCode.PROTOCOL_VERSION, ErrorCode.KICKED, ErrorCode.RATE_LIMITED].every(
    (code) => closeOutcome(4000 + code) === "fatal",
  ),
);

// --- Joining ------------------------------------------------------------------------
{
  const { session, client, statuses } = setUp();
  session.start();
  check("no saved name: ask for one, no socket yet", same(statuses, [{ kind: "needName" }]) && client.connects.length === 0);
  session.join("Ann");
  check("a name joins: connecting, Hello without a token", same(client.connects, [{ name: "Ann", token: "" }]) && statuses.at(-1)?.kind === "connecting");
}
{
  const { session, client, store, last, messages } = setUp();
  session.join("Ann");
  client.welcome(7, "t1");
  check("Welcome: playing", last()?.kind === "playing");
  check("Welcome: the name and the token are saved", store.getItem(NAME_KEY) === "Ann" && store.getItem(TOKEN_KEY) === "t1");
  check("messages go on to the game", messages.length === 1 && messages[0]?.payload.case === "welcome");
}
{
  const store = memoryStore();
  store.setItem(NAME_KEY, "Ann");
  store.setItem(TOKEN_KEY, "t1");
  const { session, client, statuses } = setUp(store);
  check("the saved name is offered", session.name === "Ann");
  session.start();
  check("a saved name joins at once, with the saved token", same(client.connects, [{ name: "Ann", token: "t1" }]) && statuses[0]?.kind === "connecting");
}

// --- Reconnecting -------------------------------------------------------------------
{
  const { session, client, timers, last } = setUp();
  session.join("Ann");
  client.welcome(7, "t1");
  client.close(1006);
  check("a lost connection: reconnecting in 1 s (attempt 1)", same(last(), { kind: "reconnecting", attempt: 1, retryInMs: 1000 }));
  timers.advance(999);
  check("... not before", client.connects.length === 1);
  timers.advance(1);
  check("... then again, with the name and the token", same(client.connects.at(-1), { name: "Ann", token: "t1" }) && client.connects.length === 2);
  client.close(1006);
  check("failing again waits longer (2 s)", same(last(), { kind: "reconnecting", attempt: 2, retryInMs: 2000 }));
  timers.advance(2000);
  client.close(1006);
  check("... and longer (4 s)", same(last(), { kind: "reconnecting", attempt: 3, retryInMs: 4000 }));
  timers.advance(4000);
  client.welcome(7, "t1", true);
  check("back: playing", last()?.kind === "playing");
  client.close(STALE_CLOSE_CODE);
  check("a Welcome starts the backoff over", same(last(), { kind: "reconnecting", attempt: 1, retryInMs: 1000 }));
}
{
  const { session, client, timers } = setUp();
  session.join("Ann");
  client.welcome(7, "t1");
  client.close(1006);
  session.retryNow();
  check("the network is back: retry now", client.connects.length === 2);
  timers.advance(5000);
  check("... and not once more when the wait ends", client.connects.length === 2);
  session.retryNow();
  check("retry now does nothing while connected or connecting", client.connects.length === 2);
}
{
  const { session, client, store } = setUp();
  session.join("Ann");
  client.welcome(7, "t1");
  client.welcome(7, "t2");
  check("each Welcome's token is kept", store.getItem(TOKEN_KEY) === "t2");
}

// --- A hidden tab: the player is away -------------------------------------------------
// A hidden tab sends no input and pings rarely, so the server closes it as idle.
// Reconnecting then would only loop (connect, idle, close) with the character
// standing unplayed; so it waits until the tab is shown again.
{
  const { session, client, timers, last } = setUp();
  session.join("Ann");
  client.welcome(7, "t1");
  session.setHidden(true);
  check("hiding the tab while playing keeps the connection", client.connects.length === 1 && last()?.kind === "playing");
  client.close(4000 + ErrorCode.IDLE_TIMEOUT);
  check("closed while hidden: away, no reconnect scheduled", same(last(), { kind: "away" }));
  timers.advance(10 * 60_000);
  check("... not even minutes later", client.connects.length === 1);
  session.setHidden(false);
  check("shown again: reconnects at once, with the token", client.connects.length === 2 && same(client.connects.at(-1), { name: "Ann", token: "t1" }));
  check("... saying so (attempt 1, now)", same(last(), { kind: "reconnecting", attempt: 1, retryInMs: 0 }));
  client.welcome(7, "t1", true);
  check("... and plays again", last()?.kind === "playing");
}
{
  const { session, client, timers, last } = setUp();
  session.join("Ann");
  client.welcome(7, "t1");
  client.close(1006);
  session.setHidden(true);
  check("hidden while waiting to reconnect: away", same(last(), { kind: "away" }));
  timers.advance(60_000);
  check("... the pending retry is off", client.connects.length === 1);
  session.setHidden(false);
  check("... shown again: reconnects at once", client.connects.length === 2);
  timers.advance(60_000);
  check("... and only once", client.connects.length === 2);
}
{
  const { session, client, last } = setUp();
  session.join("Ann");
  client.welcome(7, "t1");
  session.setHidden(true);
  session.setHidden(false);
  check("hidden and shown while playing: nothing happens", client.connects.length === 1 && last()?.kind === "playing");
}
{
  const { session, client, last } = setUp();
  session.join("Ann");
  client.welcome(7, "t1");
  session.setHidden(true);
  client.close(4000 + ErrorCode.SESSION_REPLACED);
  check("a fatal error while hidden: failed, as ever", last()?.kind === "failed");
  session.setHidden(false);
  check("... shown again: still no reconnect", client.connects.length === 1);
}
{
  const { session, client, last } = setUp();
  session.setHidden(true);
  session.join("Ann");
  client.close(4000 + ErrorCode.INVALID_NAME);
  check("a refused name while hidden: ask for another, as ever", last()?.kind === "needName");
}

// --- When not to reconnect ---------------------------------------------------------------
{
  const { session, client, timers, last } = setUp();
  session.join("Ann");
  client.welcome(7, "t1");
  client.close(4000 + ErrorCode.SESSION_REPLACED);
  check("another tab took over: failed, with why", same(last(), { kind: "failed", text: errorText(ErrorCode.SESSION_REPLACED) }));
  timers.advance(60_000);
  check("... and no reconnect (two tabs would take it back and forth)", client.connects.length === 1);
}
{
  const { session, client, timers, last, store } = setUp();
  session.join("Ann");
  client.close(4000 + ErrorCode.INVALID_NAME);
  check("a refused name: ask for another, saying why", same(last(), { kind: "needName", error: errorText(ErrorCode.INVALID_NAME) }));
  timers.advance(60_000);
  check("... no reconnect with it", client.connects.length === 1);
  check("the refused name is offered to edit, not saved", session.name === "Ann" && store.getItem(NAME_KEY) === null);
  session.join("Bob");
  check("another name joins", same(client.connects.at(-1), { name: "Bob", token: "" }));
  client.welcome(8, "t2");
  check("... and is saved once the server takes it", store.getItem(NAME_KEY) === "Bob");
}

// --- Storage ------------------------------------------------------------------------------
{
  const broken = {
    getItem: () => {
      throw new Error("blocked");
    },
    setItem: () => {
      throw new Error("blocked");
    },
  } as unknown as Storage;
  const store = browserStore(broken);
  store.setItem(NAME_KEY, "Ann");
  check("storage blocked (private mode): kept in memory for this page", store.getItem(NAME_KEY) === "Ann");
  check("nothing saved reads as null", memoryStore().getItem(NAME_KEY) === null);
}

console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
