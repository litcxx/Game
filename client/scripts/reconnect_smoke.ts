// Reconnect e2e (GAME-008): the session token from Welcome brings the same
// character back.
//   join, spawn, take a step and stop; close the socket
//   -> Hello with the token: Welcome{resumed, the same player_id and token}; the
//      body is alive where it stood, with the same hp
//   another tab with the token -> the first gets SESSION_REPLACED (fatal, closed
//      4009); the new tab is resumed
//   a new player with that name (any case) -> INVALID_NAME (4007)
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import {
  ClientMessageSchema,
  ErrorCode,
  LifeState,
  ProtocolVersion,
  ServerMessageSchema,
  type ClientMessage,
  type PlayerState,
  type ServerMessage,
} from "../src/gen/game/v1/protocol_pb.js";
import { uniqueName } from "../src/net/uniqueName.js";

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const CELL = 15 * 100 + 85; // its own cell (col 85, row 15) — away from other smokes' cells
const NAME = uniqueName("back");

const hello = (name: string, sessionToken = ""): ClientMessage =>
  create(ClientMessageSchema, {
    payload: { case: "hello", value: { protocolVersion: ProtocolVersion.CURRENT, name, sessionToken } },
  });
const spawn = (factionId: number): ClientMessage =>
  create(ClientMessageSchema, { payload: { case: "spawn", value: { cell: CELL, factionId } } });
const walk = (seq: number, moveX: number): ClientMessage =>
  create(ClientMessageSchema, { payload: { case: "input", value: { frames: [{ seq, moveX }] } } });

// One connection: everything it receives, and how it closed.
class Conn {
  readonly ws = new WebSocket(URL);
  readonly messages: ServerMessage[] = [];
  closeCode: number | undefined;
  private readonly waiters: (() => void)[] = [];

  constructor(first: ClientMessage) {
    this.ws.binaryType = "arraybuffer";
    this.ws.onopen = () => this.send(first);
    this.ws.onmessage = (ev: MessageEvent) => {
      this.messages.push(fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer)));
      this.wake();
    };
    this.ws.onclose = (ev: CloseEvent) => {
      this.closeCode = ev.code;
      this.wake();
    };
  }

  send(m: ClientMessage): void {
    this.ws.send(toBinary(ClientMessageSchema, m));
  }

  // The first value `pick` finds among the messages so far or to come, or
  // undefined after `timeoutMs`.
  until<T>(pick: (messages: ServerMessage[]) => T | undefined, timeoutMs = 3000): Promise<T | undefined> {
    return new Promise((resolve) => {
      const timer = setTimeout(() => resolve(undefined), timeoutMs);
      const check = () => {
        const found = pick(this.messages);
        if (found === undefined && this.closeCode === undefined) {
          this.waiters.push(check);
          return;
        }
        clearTimeout(timer);
        resolve(found);
      };
      check();
    });
  }

  private wake(): void {
    for (const waiter of this.waiters.splice(0)) waiter();
  }
}

const welcomeOf = (ms: ServerMessage[]) => {
  const m = ms.find((x) => x.payload.case === "welcome");
  return m?.payload.case === "welcome" ? m.payload.value : undefined;
};
const errorOf = (ms: ServerMessage[]) => {
  const m = ms.find((x) => x.payload.case === "error");
  return m?.payload.case === "error" ? m.payload.value : undefined;
};
// The last snapshot's view of player `id` and of oneself.
const lastSeen = (ms: ServerMessage[], id: number) => {
  for (let i = ms.length - 1; i >= 0; i--) {
    const m = ms[i]!;
    if (m.payload.case !== "snapshot") continue;
    const self = m.payload.value.players.find((p) => p.id === id);
    if (self) return { self, life: m.payload.value.you?.life };
  }
  return undefined;
};
// Resolves once player `id` stands still: the same position in two snapshots in a row.
const standing = (conn: Conn, id: number) => {
  let previous: PlayerState | undefined;
  return conn.until((ms) => {
    const seen = lastSeen(ms, id);
    if (!seen) return undefined;
    const still = previous && previous !== seen.self && previous.x === seen.self.x && previous.y === seen.self.y;
    previous = seen.self;
    return still ? seen.self : undefined;
  });
};

let failures = 0;
const check = (name: string, cond: boolean, detail = "") => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}${detail ? `  [${detail}]` : ""}`);
  if (!cond) failures++;
};

// 1) Join, spawn, take a step and stop — then drop.
const first = new Conn(hello(NAME));
const welcome = await first.until(welcomeOf);
if (!welcome) throw new Error("FAIL no Welcome"); // exits non-zero
const id = welcome.playerId;
const token = welcome.sessionToken;
check("Welcome carries a 32-hex-digit session token, not resumed", /^[0-9a-f]{32}$/.test(token) && !welcome.resumed);
first.send(spawn(welcome.factions[0]?.id ?? 1));
first.send(walk(1, 1));
await new Promise((r) => setTimeout(r, 300));
first.send(walk(2, 0));
const before = await standing(first, id);
check("the body walked and stands", before !== undefined && before.x > (CELL % 100) * 100 + 50, `x=${before?.x}`);
first.ws.close();
await first.until(() => first.closeCode);

// 2) Back with the token: the same character, where and as it was.
const second = new Conn(hello(NAME, token));
const resumed = await second.until(welcomeOf);
check(
  "Hello with the token: Welcome{resumed}, the same player_id and token",
  resumed !== undefined && resumed.resumed && resumed.playerId === id && resumed.sessionToken === token,
  `resumed=${resumed?.resumed} id=${resumed?.playerId}/${id}`,
);
const after = await second.until((ms) => lastSeen(ms, id));
check(
  "the body is alive where it stood, with the same hp",
  after !== undefined &&
    before !== undefined &&
    after.life === LifeState.ALIVE &&
    after.self.x === before.x &&
    after.self.y === before.y &&
    after.self.hp === before.hp,
  `before=${before?.x},${before?.y} hp ${before?.hp}; after=${after?.self.x},${after?.self.y} hp ${after?.self.hp}`,
);

// 3) Another tab with the same token takes the character over.
const third = new Conn(hello(NAME, token));
const replaced = await second.until(errorOf);
const secondClose = await second.until(() => second.closeCode);
check(
  "the old tab gets SESSION_REPLACED (fatal) and is closed 4009",
  replaced?.code === ErrorCode.SESSION_REPLACED && replaced.fatal && secondClose === 4000 + ErrorCode.SESSION_REPLACED,
  `error=${replaced ? ErrorCode[replaced.code] : "none"} close=${secondClose}`,
);
const takeover = await third.until(welcomeOf);
check("the new tab is resumed as the same player", takeover?.resumed === true && takeover.playerId === id);

// 4) The name is taken, whatever its case.
const impostor = new Conn(hello(NAME.toUpperCase()));
const refused = await impostor.until(errorOf);
const impostorClose = await impostor.until(() => impostor.closeCode);
check(
  "a new player with that name gets INVALID_NAME, closed 4007",
  refused?.code === ErrorCode.INVALID_NAME && impostorClose === 4000 + ErrorCode.INVALID_NAME,
  `error=${refused ? ErrorCode[refused.code] : "none"} close=${impostorClose}`,
);

third.ws.close();
console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
