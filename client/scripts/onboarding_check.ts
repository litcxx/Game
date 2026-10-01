// Checks for the first-capture lesson (src/onboarding.ts): the capture hint
// stays until the player captures a cell themselves — the cell under them turns
// their faction's while they hold E — and is remembered in the store for later
// visits. The state comes through the router, as in the game. Run:
//   npx tsx scripts/onboarding_check.ts
import { create } from "@bufbuild/protobuf";

import { LifeState, ServerMessageSchema, type ServerMessage } from "../src/gen/game/v1/protocol_pb.js";
import { memoryStore, type KeyValueStore } from "../src/net/session.js";
import { routeMessage } from "../src/net/router.js";
import { CAPTURED_KEY, CaptureLesson } from "../src/onboarding.js";
import { GameState } from "../src/state/gameState.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};
const msg = (payload: unknown): ServerMessage => create(ServerMessageSchema, { payload } as never);

// A 4x3 map of 100-unit cells; you are player 7 of Red (1); Green (2) is the enemy.
// Cell 5 is at (150, 150), cell 4 at (50, 150).
const RED = 1;
const GREEN = 2;
let tick = 0;
const joined = (owners: number[]): GameState => {
  const state = new GameState();
  routeMessage(
    state,
    msg({
      case: "welcome",
      value: {
        playerId: 7,
        config: { tickRate: 60, mapWidth: 4, mapHeight: 3, moveSpeed: 300, maxHp: 100 },
        factions: [
          { id: RED, name: "Red", color: 0xff0000 },
          { id: GREEN, name: "Green", color: 0x00ff00 },
        ],
      },
    }),
    1000,
  );
  routeMessage(state, msg({ case: "mapState", value: { ownerFactionIds: new Uint8Array(owners), captures: [] } }), 1000);
  state.myFaction = RED; // as a spawn click sets it
  return state;
};
const neutral = () => new Array<number>(12).fill(0);
const at = (state: GameState, x: number, cells: { index: number; ownerFactionId: number }[] = []) =>
  routeMessage(
    state,
    msg({
      case: "snapshot",
      value: { tick: ++tick, you: { life: LifeState.ALIVE }, players: [{ id: 7, x, y: 150, hp: 100 }], cells },
    }),
    1000,
  );
const dead = (state: GameState) =>
  routeMessage(state, msg({ case: "snapshot", value: { tick: ++tick, you: { life: LifeState.DEAD, respawnTick: tick + 300 } } }), 1000);

// The game's order for one snapshot interval: E held (or not) over the fixed
// steps, then the snapshot lands, then the lesson looks at it.
const interval = (state: GameState, lesson: CaptureLesson, holdingE: boolean, x: number, cells?: { index: number; ownerFactionId: number }[]) => {
  lesson.noteCapturing(holdingE);
  at(state, x, cells);
  lesson.observe();
};

// --- Not learned until you capture ------------------------------------------------
{
  const store = memoryStore();
  const state = joined(neutral());
  new CaptureLesson(state, store);
  check("a first visit: the lesson is ahead", !state.captureLearned);
}

{
  const store = memoryStore();
  const state = joined(neutral());
  const lesson = new CaptureLesson(state, store);
  interval(state, lesson, true, 150);
  check("standing on a neutral cell with E: not yet", !state.captureLearned);
  interval(state, lesson, true, 150, [{ index: 5, ownerFactionId: RED }]);
  check("the neutral cell under you turns yours while you hold E: learned", state.captureLearned);
  check("... and remembered in the store", store.getItem(CAPTURED_KEY) !== null);
}

{
  const owners = neutral();
  owners[5] = GREEN;
  const state = joined(owners);
  const lesson = new CaptureLesson(state, memoryStore());
  interval(state, lesson, true, 150);
  interval(state, lesson, true, 150, [{ index: 5, ownerFactionId: RED }]);
  check("an enemy cell under you turns yours while you hold E: learned", state.captureLearned);
}

{
  // At a low frame rate (or when two snapshots arrive together) several snapshots
  // land between two input steps: E is still held for the ones without a step.
  const state = joined(neutral());
  const lesson = new CaptureLesson(state, memoryStore());
  interval(state, lesson, true, 150);
  at(state, 150); // no input step since the last snapshot
  lesson.observe();
  at(state, 150, [{ index: 5, ownerFactionId: RED }]);
  lesson.observe();
  check("snapshots between two input steps: E still held, learned", state.captureLearned);
}

// --- What doesn't count ------------------------------------------------------------
{
  const store = memoryStore();
  const state = joined(neutral());
  const lesson = new CaptureLesson(state, store);
  interval(state, lesson, false, 150);
  interval(state, lesson, false, 150, [{ index: 5, ownerFactionId: RED }]);
  check("an ally captured the cell under you, you never held E: not learned", !state.captureLearned);
  check("... and nothing stored", store.getItem(CAPTURED_KEY) === null);
}

{
  const state = joined(neutral());
  const lesson = new CaptureLesson(state, memoryStore());
  interval(state, lesson, true, 150);
  interval(state, lesson, false, 150);
  interval(state, lesson, false, 150, [{ index: 5, ownerFactionId: RED }]);
  check("E let go a snapshot before the cell turned: not learned", !state.captureLearned);
}

{
  const owners = neutral();
  owners[5] = RED;
  const state = joined(owners);
  const lesson = new CaptureLesson(state, memoryStore());
  interval(state, lesson, true, 50); // cell 4, neutral
  interval(state, lesson, true, 150); // onto cell 5, already yours
  interval(state, lesson, true, 150);
  check("stepping onto a cell that is already yours: not learned", !state.captureLearned);
}

{
  const state = joined(neutral());
  const lesson = new CaptureLesson(state, memoryStore());
  interval(state, lesson, true, 50); // cell 4
  interval(state, lesson, true, 150, [{ index: 5, ownerFactionId: RED }]); // moved onto cell 5 as it turned
  check("the cell turned as you stepped onto it: not learned", !state.captureLearned);
}

{
  const state = joined(neutral());
  const lesson = new CaptureLesson(state, memoryStore());
  lesson.noteCapturing(true);
  lesson.observe(); // not spawned yet
  check("not spawned: nothing to learn", !state.captureLearned && state.cellUnderMe() === undefined);
  interval(state, lesson, true, 150); // alive on cell 5, neutral
  lesson.noteCapturing(true);
  dead(state);
  lesson.observe();
  interval(state, lesson, true, 150, [{ index: 5, ownerFactionId: RED }]); // back on cell 5, an ally took it meanwhile
  check("a death in between: the change seen on respawn doesn't count", !state.captureLearned);
}

// --- Remembered ---------------------------------------------------------------------
{
  const store: KeyValueStore = memoryStore();
  const first = joined(neutral());
  const lesson = new CaptureLesson(first, store);
  interval(first, lesson, true, 150);
  interval(first, lesson, true, 150, [{ index: 5, ownerFactionId: RED }]);
  const later = joined(neutral());
  new CaptureLesson(later, store);
  check("a later visit with the same store: learned from the start", later.captureLearned);
}

console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
