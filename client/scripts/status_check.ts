// Checks for what render/status.ts hands the HUD, bar, picker and respawn button from GameState
// — with fake views that record the calls, so no PixiJS. Run:
//   npx tsx scripts/status_check.ts
import { create } from "@bufbuild/protobuf";

import { ErrorCode, LifeState, ServerMessageSchema, type ServerMessage } from "../src/gen/game/v1/protocol_pb.js";
import { routeMessage } from "../src/net/router.js";
import { showFailure, showStatus, type StatusViews } from "../src/render/status.js";
import { GameState } from "../src/state/gameState.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};
const same = (a: unknown, b: unknown) => JSON.stringify(a) === JSON.stringify(b);
const msg = (payload: unknown): ServerMessage => create(ServerMessageSchema, { payload } as never);

// Views that remember the last arguments of each call.
const fakeViews = () => {
  const last: Record<string, unknown[]> = {};
  const recorder = (prefix: string) =>
    new Proxy({}, { get: (_, name) => (...args: unknown[]) => void (last[`${prefix}.${String(name)}`] = args) });
  const views = { hud: recorder("hud"), bar: recorder("bar"), picker: recorder("picker"), respawn: recorder("respawn") } as unknown as StatusViews;
  return { views, last };
};

// A joined player on a 4x3 map; Ann and Bob in the roster.
const joined = (): GameState => {
  const state = new GameState();
  routeMessage(
    state,
    msg({
      case: "welcome",
      value: {
        playerId: 7,
        config: { tickRate: 60, mapWidth: 4, mapHeight: 3, moveSpeed: 300, maxHp: 100 },
        factions: [{ id: 1, name: "Red", color: 0xff0000 }],
      },
    }),
    0,
  );
  routeMessage(state, msg({ case: "roster", value: { full: true, upsert: [{ id: 7, name: "Ann" }, { id: 8, name: "Bob" }] } }), 0);
  return state;
};

{
  const state = joined();
  const { views, last } = fakeViews();
  showStatus(state, views, false, 0);
  check("the network line: online, no ping before a Pong", same(last["hud.setNetwork"], [2, undefined]));
  routeMessage(state, msg({ case: "pong", value: { clientTimeMs: 100 } }), 142);
  showStatus(state, views, false, 0);
  check("the network line: online and ping", same(last["hud.setNetwork"], [2, 42]));
  check("not spawned: the picker, no bar", same(last["picker.setVisible"], [true]) && same(last["bar.setVisible"], [false]));
  check("not spawned: the faction picked for the spawn", same(last["hud.setFaction"], ["Red", 0xff0000]));
  check("... no lone button: «В бой» is in the picker", same(last["respawn.show"], [undefined]));
  state.myFaction = 1; // back without a body, the faction locked
  showStatus(state, views, false, 0);
  check("back with a faction: no picker, «В бой» in the middle", same(last["picker.setVisible"], [false]) && same(last["respawn.show"], ["В бой"]));
  routeMessage(state, msg({ case: "snapshot", value: { tick: 10, you: { life: LifeState.DEAD, respawnTick: 20 } } }), 0);
  showStatus(state, views, false, 0);
  check("dead, waiting: no button", same(last["respawn.show"], [undefined]));
  routeMessage(state, msg({ case: "snapshot", value: { tick: 20, you: { life: LifeState.DEAD, respawnTick: 20 } } }), 0);
  showStatus(state, views, false, 0);
  check("dead, may respawn: «Возродиться» in the middle", same(last["respawn.show"], ["Возродиться"]));
}
{
  const state = joined(); // a 4x3 map: 12 cells
  const { views, last } = fakeViews();
  showStatus(state, views, false, 0);
  check("territory before the scores: nothing yet", same(last["hud.setTerritory"], [0, 0]));
  routeMessage(state, msg({ case: "factionScores", value: { scores: [{ factionId: 1, cells: 3, online: 2 }] } }), 0);
  showStatus(state, views, false, 0);
  check("territory: the server's count and share (FactionScores)", same(last["hud.setTerritory"], [3, 25]));
  check("the picker: online and share per faction", same(last["picker.setScores"], [[{ id: 1, online: 2, percent: 25 }]]));
}
{
  const state = joined();
  routeMessage(state, msg({ case: "snapshot", value: { tick: 1, you: { life: LifeState.ALIVE }, players: [{ id: 7, x: 150, y: 50, hp: 80 }] } }), 0);
  const { views, last } = fakeViews();
  showStatus(state, views, false, 0);
  check("alive: the bar, no picker", same(last["bar.setVisible"], [true]) && same(last["picker.setVisible"], [false]));
  check("alive: hp", same(last["hud.setHp"], [80, 100, true]));
  check("alive: the cell under you", same(last["hud.setCell"], [1, undefined, undefined, 0]));
  check("alive: no respawn button", same(last["respawn.show"], [undefined]));
}
{
  const state = joined();
  const { views, last } = fakeViews();
  routeMessage(state, msg({ case: "error", value: { code: ErrorCode.KICKED, fatal: true } }), 0);
  showFailure(state, views, 0);
  check("fatal: no bar, no picker, no button, the reason on the hint line",
    same(last["bar.setVisible"], [false]) && same(last["picker.setVisible"], [false]) && same(last["respawn.show"], [undefined]) &&
      last["hud.setHint"]?.[0] === "Вы отключены от сервера");
}

console.log("VERDICT:", failures === 0 ? "PASS" : `FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
