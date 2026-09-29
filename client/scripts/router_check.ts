// Pure checks for the client's data layer: how each ServerMessage lands in
// GameState (router), with the territory, roster, fog, players, projectiles,
// effects and notices it holds — no PixiJS, no socket. Run:
//   npx tsx scripts/router_check.ts
import { create } from "@bufbuild/protobuf";

import { errorText, NOTICE_MS } from "../src/errors.js";
import {
  AbilityKind,
  ErrorCode,
  LifeState,
  ServerMessageSchema,
  type ServerMessage,
} from "../src/gen/game/v1/protocol_pb.js";
import { usualHint } from "../src/hint.js";
import { routeMessage } from "../src/net/router.js";
import { GameState, INTERP_DELAY_MS } from "../src/state/gameState.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};
const same = (a: unknown, b: unknown) => JSON.stringify(a) === JSON.stringify(b);
const near = (a: number, b: number) => Math.abs(a - b) < 1e-9;

type Payload = ServerMessage["payload"];
const msg = (payload: Payload): ServerMessage => create(ServerMessageSchema, { payload } as never);

// A 4x3 map (cells 0..11), you are player 7; Red (1) and Green (2); a melee
// ability and a projectile one.
const welcome = (): ServerMessage =>
  msg({
    case: "welcome",
    value: {
      playerId: 7,
      config: { tickRate: 60, snapshotRate: 20, mapWidth: 4, mapHeight: 3, moveSpeed: 300, maxHp: 120 },
      factions: [
        { id: 1, name: "Red", color: 0xff0000 },
        { id: 2, name: "Green", color: 0x00ff00 },
      ],
      abilities: [
        { id: 1, kind: AbilityKind.MELEE, name: "Strike", cooldownTicks: 45, damage: 20, range: 120 },
        { id: 2, kind: AbilityKind.PROJECTILE, name: "Shot", cooldownTicks: 90, damage: 30, range: 500, projectileSpeed: 800, projectileRadius: 8 },
      ],
    },
  } as never);

const snapshot = (value: Record<string, unknown>): ServerMessage => msg({ case: "snapshot", value } as never);
const joined = (nowMs = 1000): GameState => {
  const state = new GameState();
  routeMessage(state, welcome(), nowMs);
  return state;
};

// --- Welcome ------------------------------------------------------------------
{
  const state = new GameState();
  check("before Welcome: not spawned, not alive", state.life === LifeState.NOT_SPAWNED && !state.alive);
  routeMessage(state, welcome(), 1000);
  check("Welcome: player id", state.myId === 7);
  check("Welcome: map, max hp, tick rate", state.mapWidth === 4 && state.mapHeight === 3 && state.maxHp === 120 && state.tickRate === 60);
  check("Welcome: factions in order", same(state.factions.map((f) => f.id), [1, 2]) && state.factionColor(2) === 0x00ff00);
  check("Welcome: the first faction is selected", state.selectedFaction === 1);
  check("Welcome: abilities, slot 1 active", state.abilities.length === 2 && state.activeSlot === 0 && state.activeAbility?.id === 1);
  check("Welcome: the projectile radius", state.projectileRadius === 8);
  check("Welcome: the predictor starts at the map centre", same(state.predictor?.position, { x: 200, y: 150 }));
  check("Welcome: territory and fog sized to the map", state.territory.cols === 4 && state.territory.rows === 3 && state.fog.sightAt(11) === "unexplored");
}

// --- Roster -------------------------------------------------------------------
{
  const state = joined();
  const roster = (upsert: { id: number; name: string; factionId: number }[], full: boolean, removed: number[] = []) =>
    routeMessage(state, msg({ case: "roster", value: { upsert, full, removed } } as never), 1000);
  roster([{ id: 1, name: "Ann", factionId: 1 }, { id: 2, name: "Bob", factionId: 2 }], true);
  check("a full roster lists everyone", state.roster.online === 2 && state.roster.nameOf(1) === "Ann" && state.roster.factionOf(2) === 2);
  roster([{ id: 3, name: "Cid", factionId: 0 }], false);
  check("an upsert adds", state.roster.online === 3);
  roster([], false, [2]);
  check("removed ones leave", state.roster.online === 2 && state.roster.factionOf(2) === 0 && state.roster.nameOf(2) === "");
  roster([{ id: 5, name: "Eve", factionId: 1 }], true);
  check("a full roster replaces the list", state.roster.online === 1 && state.roster.nameOf(1) === "");
  state.roster.setFaction(7, 2);
  check("your own faction is set locally", state.roster.factionOf(7) === 2);
}

// --- Territory: MapState and Snapshot.cells -------------------------------------
{
  const state = joined();
  const before = state.worldRevision;
  const owners = new Uint8Array([1, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0]);
  routeMessage(state, msg({ case: "mapState", value: { owners, captures: [{ index: 2, owner: 0, captureFaction: 2, captureProgress: 40 }] } } as never), 1000);
  check("MapState: owned cells per faction", state.territory.owned(1) === 2 && state.territory.owned(2) === 1);
  check("MapState: faction stats", same(state.territory.stats(1), { cells: 2, percent: 17 }));
  check("MapState: a capture in progress", same(state.territory.cell(2, 0), { index: 2, owner: 0, captureFaction: 2, captureProgress: 40 }));
  check("MapState: the world is redrawn", state.worldRevision > before);
  check("a cell off the map reads as nothing", same(state.territory.cell(4, 0), { index: 4, owner: 0, captureFaction: 0, captureProgress: 0 }));

  const mid = state.worldRevision;
  routeMessage(state, snapshot({ tick: 5, cells: [{ index: 2, owner: 2, captureFaction: 0, captureProgress: 0 }, { index: 0, owner: 0 }] }), 1000);
  check("Snapshot.cells: owners and counts follow", state.territory.owned(1) === 1 && state.territory.owned(2) === 2 && state.territory.cell(2, 0).owner === 2);
  check("Snapshot.cells: the capture is over", state.territory.cell(2, 0).captureProgress === 0);
  check("Snapshot.cells: the world is redrawn", state.worldRevision > mid);
  const quiet = state.worldRevision;
  routeMessage(state, snapshot({ tick: 6 }), 1000);
  check("a snapshot with no cell or sight change keeps the world", state.worldRevision === quiet);
}

// --- Snapshot: you, players, prediction ------------------------------------------
{
  const state = joined();
  const you = { life: LifeState.ALIVE, lastInputSeq: 0, respawnTick: 0, attackReadyTick: 130, attackCooldownTicks: 45, blockReadyTick: 0, blockCooldownTicks: 0 };
  routeMessage(state, snapshot({ tick: 100, you, players: [{ id: 7, x: 150, y: 150, hp: 90 }, { id: 9, x: 250, y: 150, hp: 50 }] }), 2000);
  check("Snapshot: alive, hp, server tick", state.alive && state.hp === 90 && state.serverTick === 100);
  check("Snapshot: players by id", same(state.players.get(9), { x: 250, y: 150, hp: 50 }));
  check("Snapshot: remote ids are the others", same(state.remoteIds(), [9]));
  check("Snapshot: coming alive resets the prediction to you", same(state.predictor?.position, { x: 150, y: 150 }));
  check("Snapshot: the attack cooldown", near(state.cooldownProgress(2000).attack, 1 - 30 / 45) && state.cooldownProgress(2000).block === 1);
  check("the server tick advances between snapshots", near(state.currentTick(2500), 130));

  routeMessage(state, snapshot({ tick: 106, you, players: [{ id: 7, x: 160, y: 150, hp: 90 }] }), 2100);
  check("Snapshot: still alive -> reconciled to the server", same(state.predictor?.position, { x: 160, y: 150 }));
  check("Snapshot: players gone from it are dropped", !state.players.has(9) && same(state.remoteIds(), []));

  routeMessage(state, snapshot({ tick: 200, you: { life: LifeState.DEAD, respawnTick: 260 }, players: [{ id: 7, x: 160, y: 150, hp: 0 }] }), 3000);
  check("Snapshot: dead, with the respawn tick", !state.alive && state.life === LifeState.DEAD && state.respawnTick === 260 && state.hp === 0);
}

// --- Snapshot: fog of war --------------------------------------------------------
{
  const state = joined();
  let rev = state.worldRevision;
  routeMessage(state, snapshot({ tick: 1, revealed: [0, 1] }), 1000);
  check("revealed cells are visible", state.fog.sightAt(0) === "visible" && state.fog.sightAt(1) === "visible" && state.worldRevision > rev);
  rev = state.worldRevision;
  routeMessage(state, snapshot({ tick: 2, hidden: [1] }), 1000);
  check("hidden cells turn explored", state.fog.sightAt(1) === "explored" && state.worldRevision > rev);
  routeMessage(state, snapshot({ tick: 3, resync: true, revealed: [5] }), 1000);
  check("a resync forgets the old sight", state.fog.sightAt(0) === "explored" && state.fog.sightAt(5) === "visible");
}

// --- Snapshot: projectiles and effects ---------------------------------------------
{
  const state = joined();
  routeMessage(state, snapshot({ tick: 1, projectiles: [{ id: 3, x: 100, y: 100, factionId: 2, vx: 800, vy: 0 }] }), 1000);
  check("projectiles: velocity and faction kept", same(state.shotMeta.get(3), { vx: 800, vy: 0, factionId: 2, seenMs: 1000 }));
  routeMessage(state, snapshot({ tick: 60 }), 3500);
  check("projectiles: forgotten 2 s after last seen", !state.shotMeta.has(3));

  routeMessage(state, snapshot({ tick: 61, events: [{ tick: 61, kind: { case: "ability", value: { playerId: 9, abilityId: 1 } } }] }), 4000);
  const fx = state.takeEffects();
  check("events: a remote swing, delayed like its token", fx.length === 1 && fx[0]?.kind === "swing" && fx[0]?.startMs === 4000 + INTERP_DELAY_MS);
  check("events: effects are taken once", state.takeEffects().length === 0);
}

// --- Errors and closes ---------------------------------------------------------------
{
  const state = joined();
  routeMessage(state, msg({ case: "error", value: { code: ErrorCode.SPAWN_TOO_EARLY, fatal: false } } as never), 1000);
  check("a refusal shows for a while", state.notices.hint("usual", 1000) === errorText(ErrorCode.SPAWN_TOO_EARLY) && !state.notices.failed);
  check("a refusal fades", state.notices.hint("usual", 1000 + NOTICE_MS) === "usual");
  routeMessage(state, msg({ case: "error", value: { code: ErrorCode.KICKED, fatal: true } } as never), 1000);
  check("a fatal error stays", state.notices.failed && state.notices.hint("usual", 99999) === errorText(ErrorCode.KICKED));
}

// --- A new session on the same page (a reconnect) -----------------------------------------
{
  const state = joined();
  const you = { life: LifeState.ALIVE, attackReadyTick: 130, attackCooldownTicks: 45 };
  routeMessage(state, snapshot({ tick: 100, you, players: [{ id: 7, x: 150, y: 150, hp: 90 }, { id: 9, x: 250, y: 150, hp: 50 }], projectiles: [{ id: 3, x: 1, y: 1, factionId: 2 }] }), 2000);
  routeMessage(state, snapshot({ tick: 101, you, players: [{ id: 7, x: 150, y: 150, hp: 90 }], events: [{ tick: 101, kind: { case: "ability", value: { playerId: 9, abilityId: 1 } } }] }), 2050);
  check("(alive before it)", state.alive && state.hp === 90);
  routeMessage(state, msg({ case: "pong", value: { clientTimeMs: 1000 } } as never), 1040);
  routeMessage(state, msg({ case: "error", value: { code: ErrorCode.IDLE_TIMEOUT, fatal: true } } as never), 2100);
  state.myFaction = 2;

  routeMessage(state, welcome(), 5000);
  check("a new Welcome: nothing of the old session is alive", state.life === LifeState.NOT_SPAWNED && state.hp === 0 && !state.alive);
  check("... no players, no remote positions, no projectiles", state.players.size === 0 && state.interp.ids(5000).size === 0 && state.shotMeta.size === 0 && state.shots.ids(5000).size === 0);
  check("... no pending effects, no cooldowns", state.takeEffects().length === 0 && state.cooldownProgress(5000).attack === 1);
  check("... the error that ended it is gone", !state.notices.failed && state.notices.hint("usual", 5000) === "usual");
  check("... the round trip is measured anew", state.rttMs === undefined);
  check("... the faction comes back with the roster", state.myFaction === 0);
}
{
  const state = joined();
  routeMessage(state, msg({ case: "roster", value: { upsert: [{ id: 7, name: "Me", factionId: 2 }, { id: 9, name: "Bob", factionId: 1 }], full: true } } as never), 1000);
  check("a full roster tells your own faction (a resumed body)", state.myFaction === 2 && state.selectedFaction === 2);
  const fresh = joined();
  routeMessage(fresh, msg({ case: "roster", value: { upsert: [{ id: 7, name: "Me", factionId: 0 }], full: true } } as never), 1000);
  check("... none yet: the first faction stays selected", fresh.myFaction === 0 && fresh.selectedFaction === 1);
}

// --- Pong: round-trip time ---------------------------------------------------------------
{
  const state = joined();
  check("no round trip before the first Pong", state.rttMs === undefined);
  routeMessage(state, msg({ case: "pong", value: { clientTimeMs: 1000, serverTick: 5 } } as never), 1038.7);
  check("Pong: RTT = now - the echoed send time", state.rttMs === 38);
  // Sent as uint32 ms: the clock wrapped between the Ping and its Pong.
  routeMessage(state, msg({ case: "pong", value: { clientTimeMs: 2 ** 32 - 10, serverTick: 6 } } as never), 2 ** 32 + 28);
  check("Pong: RTT across the uint32 wrap", state.rttMs === 38);
}

// --- The usual hint line ---------------------------------------------------------------
{
  const state = joined();
  check("hint: before spawning", usualHint(state, false) === "Выберите фракцию и кликните по клетке — старт · M — вся карта");
  check("hint: in the map view", usualHint(state, true).endsWith("M — к игроку"));
  routeMessage(state, snapshot({ tick: 200, you: { life: LifeState.DEAD, respawnTick: 260 } }), 1000);
  check("hint: dead, waiting", usualHint(state, false) === "Убит · возрождение через 1с · M — вся карта");
  routeMessage(state, snapshot({ tick: 260, you: { life: LifeState.DEAD, respawnTick: 260 } }), 1000);
  check("hint: dead, may respawn", usualHint(state, false).startsWith("Убит · выберите фракцию"));
  routeMessage(state, snapshot({ tick: 300, you: { life: LifeState.ALIVE } }), 1000);
  check("hint: none while alive", usualHint(state, false) === "");
}

console.log("VERDICT:", failures === 0 ? "PASS" : `FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
