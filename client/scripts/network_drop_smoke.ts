// The network goes away for DROP_MS (10 s) and comes back (GAME-009): the page's
// Session over GameClient returns to the game as the same character. The game
// talks to the server through a TCP proxy here that, while "down", passes
// nothing and closes nothing — as when Wi-Fi drops: no close on either side,
// and a connection opened meanwhile hangs for good.
//   join, spawn, take a step and stop
//   the network drops -> the silence is noticed (no close comes): reconnecting
//   the network is back -> Welcome{resumed, the same player_id}; the body is
//      alive where it stood, with the same hp
import { connect, createServer, type Socket } from "node:net";

import { LifeState } from "../src/gen/game/v1/protocol_pb.js";
import { STALE_MS } from "../src/net/client.js";
import { SessionPlayer } from "./sessionPlayer.js";
import { spot } from "./smokeMap.js";
import { uniqueName } from "./uniqueName.js";

const SERVER = new URL(process.env.SERVER_URL ?? "ws://127.0.0.1:27998/");
const DROP_MS = Number(process.env.DROP_MS ?? 10_000);
const HOME = spot("network-drop"); // its own capital on the smoke map, away from other smokes' spots

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

let failures = 0;
const check = (name: string, cond: boolean, detail = "") => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}${detail ? `  [${detail}]` : ""}`);
  if (!cond) failures++;
};

// The network between the game and the server.
let down = false;
const links: { dead: boolean }[] = [];
const network = createServer((game: Socket) => {
  const link = { dead: down };
  links.push(link);
  game.on("error", () => {});
  if (link.dead) return; // never answered, never closed
  const server = connect(Number(SERVER.port), SERVER.hostname);
  server.on("error", () => {});
  game.on("data", (d) => {
    if (!link.dead) server.write(d);
  });
  server.on("data", (d) => {
    if (!link.dead) game.write(d);
  });
  game.on("close", () => {
    if (!link.dead) server.destroy();
  });
  server.on("close", () => {
    if (!link.dead) game.destroy();
  });
});
await new Promise<void>((r) => network.listen(0, "127.0.0.1", r));
const port = (network.address() as { port: number }).port;
const setDown = (value: boolean) => {
  down = value;
  if (down) for (const link of links) link.dead = true;
};

// 1) Join, spawn, take a step and stop.
const tab = new SessionPlayer(`ws://127.0.0.1:${port}/`);
tab.session.join(uniqueName("drop"));
const welcome = await tab.until(() => tab.welcomes()[0]);
if (!welcome) throw new Error("FAIL no Welcome"); // exits non-zero
const id = welcome.playerId;
tab.client.sendSpawn(HOME.factionId);
tab.walk(1);
await sleep(300);
tab.walk(0);
const before = await tab.standing(id);
check("the body walked and stands", before !== undefined && before.x > HOME.x, `x=${before?.x}`);

// 2) The network drops: nothing comes, nothing closes.
setDown(true);
const cutAt = performance.now();
const noticed = await tab.until(() => tab.statuses.find((s) => s.kind === "reconnecting"), STALE_MS + 4000);
const noticedAfterMs = performance.now() - cutAt;
check(
  `the silence is noticed within ${STALE_MS / 1000} s + a keepalive: reconnecting`,
  noticed !== undefined && noticedAfterMs >= STALE_MS - 500,
  `after ${Math.round(noticedAfterMs)} ms`,
);

// 3) The network is back: the same character, where and as it was.
await sleep(Math.max(0, DROP_MS - (performance.now() - cutAt)));
setDown(false);
const resumed = await tab.until(() => tab.welcomes()[1], 25_000);
check(
  "back: Welcome{resumed}, the same player_id",
  resumed !== undefined && resumed.resumed && resumed.playerId === id,
  `resumed=${resumed?.resumed} id=${resumed?.playerId}/${id} after ${Math.round(performance.now() - cutAt)} ms`,
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

console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
