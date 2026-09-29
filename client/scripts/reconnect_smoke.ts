// Reconnect e2e (GAME-008, GAME-009): the page's Session over GameClient brings
// the same character back.
//   join by name, spawn, take a step and stop -> the name and the Welcome's
//      session token are saved
//   the connection drops -> the session reconnects by itself after 1 s with the
//      token: Welcome{resumed, the same player_id}; the body is alive where it
//      stood, with the same hp
//   another tab of the browser (the same saved token) starts -> it gets the
//      character; the first tab gets SESSION_REPLACED and stops for good (no
//      reconnect: two tabs would take it back and forth)
//   a new player with that name (any case) -> asked for another name
//      (INVALID_NAME), no reconnect
import { errorText } from "../src/errors.js";
import { ErrorCode, LifeState } from "../src/gen/game/v1/protocol_pb.js";
import { NAME_KEY, TOKEN_KEY } from "../src/net/session.js";
import { SessionPlayer } from "./sessionPlayer.js";
import { uniqueName } from "./uniqueName.js";

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const CELL = 15 * 100 + 85; // its own cell (col 85, row 15) — away from other smokes' cells
const NAME = uniqueName("back");

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));
const same = (a: unknown, b: unknown) => JSON.stringify(a) === JSON.stringify(b);

let failures = 0;
const check = (name: string, cond: boolean, detail = "") => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}${detail ? `  [${detail}]` : ""}`);
  if (!cond) failures++;
};

// 1) Join, spawn, take a step and stop.
const tab = new SessionPlayer(URL);
tab.session.join(NAME);
const welcome = await tab.until(() => tab.welcomes()[0]);
if (!welcome) throw new Error("FAIL no Welcome"); // exits non-zero
const id = welcome.playerId;
const token = welcome.sessionToken;
check("Welcome carries a 32-hex-digit session token, not resumed", /^[0-9a-f]{32}$/.test(token) && !welcome.resumed);
check("the name and the token are saved", tab.store.getItem(NAME_KEY) === NAME && tab.store.getItem(TOKEN_KEY) === token);
tab.client.sendSpawn(CELL, welcome.factions[0]?.id ?? 1);
tab.client.sendInput(1, 0, false);
await sleep(300);
tab.client.sendInput(0, 0, false);
const before = await tab.standing(id);
check("the body walked and stands", before !== undefined && before.x > (CELL % 100) * 100 + 50, `x=${before?.x}`);

// 2) The connection drops: the session comes back by itself.
tab.client.close();
const waiting = await tab.until(() => tab.statuses.find((s) => s.kind === "reconnecting"));
check("a dropped connection: reconnecting in 1 s (attempt 1)", same(waiting, { kind: "reconnecting", attempt: 1, retryInMs: 1000 }), JSON.stringify(waiting));
const resumed = await tab.until(() => tab.welcomes()[1]);
check(
  "... then Hello with the token: Welcome{resumed}, the same player_id and token",
  resumed !== undefined && resumed.resumed && resumed.playerId === id && resumed.sessionToken === token,
  `resumed=${resumed?.resumed} id=${resumed?.playerId}/${id}`,
);
check("... playing again", tab.statuses.at(-1)?.kind === "playing");
const after = await tab.until(() => tab.lastSeen(id, 1));
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

// 3) Another tab of the browser starts with the saved name and token.
const other = new SessionPlayer(URL, tab.store);
other.session.start();
const takeover = await other.until(() => other.welcomes()[0]);
check("another tab joins with the saved token: the same player, resumed", takeover?.resumed === true && takeover.playerId === id);
const stopped = await tab.until(() => tab.statuses.find((s) => s.kind === "failed"));
check(
  "the first tab stops: SESSION_REPLACED, with why",
  same(stopped, { kind: "failed", text: errorText(ErrorCode.SESSION_REPLACED) }),
  JSON.stringify(stopped),
);

// 4) The name is taken, whatever its case.
const impostor = new SessionPlayer(URL);
impostor.session.join(NAME.toUpperCase());
const renamed = await impostor.until(() => impostor.statuses.find((s) => s.kind === "needName"));
check(
  "a new player with that name is asked for another (INVALID_NAME)",
  same(renamed, { kind: "needName", error: errorText(ErrorCode.INVALID_NAME) }),
  JSON.stringify(renamed),
);

// Past the first backoff: neither reconnected.
const statusesNow = [tab.statuses.length, impostor.statuses.length];
await sleep(1500);
check("no reconnect after either", same([tab.statuses.length, impostor.statuses.length], statusesNow));
check("the other tab still plays", other.statuses.at(-1)?.kind === "playing");

console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
