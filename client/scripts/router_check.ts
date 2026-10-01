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
import { FADE_MS, GameState, INTERP_DELAY_MS } from "../src/state/gameState.js";

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
// ability and a projectile one. `extra`: more Welcome fields (e.g. the map).
const welcome = (extra: Record<string, unknown> = {}): ServerMessage =>
  msg({
    case: "welcome",
    value: {
      ...extra,
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
  const roster = (upsert: { id: number; name: string; factionId: number }[], full: boolean, removedPlayerIds: number[] = []) =>
    routeMessage(state, msg({ case: "roster", value: { upsert, full, removedPlayerIds } } as never), 1000);
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
  routeMessage(state, msg({ case: "mapState", value: { ownerFactionIds: owners, captures: [{ index: 2, ownerFactionId: 0, captureFactionId: 2, captureProgress: 40 }] } } as never), 1000);
  check("MapState: owned cells per faction", state.territory.owned(1) === 2 && state.territory.owned(2) === 1);
  check("MapState: faction stats", same(state.territory.stats(1), { cells: 2, percent: 17 }));
  check("MapState: a capture in progress", same(state.territory.cell(2, 0), { index: 2, owner: 0, captureFaction: 2, captureProgress: 40 }));
  check("MapState: the world is redrawn", state.worldRevision > before);
  check("a cell off the map reads as nothing", same(state.territory.cell(4, 0), { index: 4, owner: 0, captureFaction: 0, captureProgress: 0 }));

  const mid = state.worldRevision;
  routeMessage(state, snapshot({ tick: 5, cells: [{ index: 2, ownerFactionId: 2, captureFactionId: 0, captureProgress: 0 }, { index: 0, ownerFactionId: 0 }] }), 1000);
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

// --- Snapshot: a hit on you -------------------------------------------------------
// Your hp dropping between snapshots flashes your token with the damage — the hit
// its dealer sees — with or without a HitEvent (a shot from the fog brings none),
// once the blow is drawn reaching you (the attacker and its shot are drawn
// INTERP_DELAY_MS behind).
{
  const state = joined();
  const you = { life: LifeState.ALIVE };
  const me = (hp: number) => [{ id: 7, x: 150, y: 150, hp }];
  const onMe = { tick: 4, kind: { case: "hit", value: { attackerId: 9, targetId: 7, damage: 30 } } };
  const taken = () => state.takeEffects().map((e) => ({ kind: e.kind, playerId: e.playerId, startMs: e.startMs, damage: e.damage }));
  routeMessage(state, snapshot({ tick: 1, you, players: me(120) }), 1000);
  check("a hit on you: nothing at your first snapshot", taken().length === 0);
  routeMessage(state, snapshot({ tick: 4, you, players: me(90) }), 1050);
  check("a hit on you: your hp drop flashes your token with the damage, after the delay", same(taken(), [{ kind: "hit", playerId: 7, startMs: 1050 + INTERP_DELAY_MS, damage: 30 }]));
  routeMessage(state, snapshot({ tick: 7, you, players: me(60), events: [onMe] }), 1100);
  check("... one flash with its HitEvent too, not two", taken().length === 1);
  routeMessage(state, snapshot({ tick: 10, you, players: me(60) }), 1150);
  check("... nothing while your hp holds", taken().length === 0);
  routeMessage(state, snapshot({ tick: 13, you: { life: LifeState.DEAD, respawnTick: 400 }, players: me(0) }), 1200);
  check("... the killing blow too", same(taken(), [{ kind: "hit", playerId: 7, startMs: 1200 + INTERP_DELAY_MS, damage: 60 }]));
  routeMessage(state, snapshot({ tick: 400, you, players: me(120) }), 1250);
  check("... nothing at a respawn (hp up)", taken().length === 0);
  routeMessage(state, snapshot({ tick: 401, you: { life: LifeState.NOT_SPAWNED } }), 1300);
  check("... nothing when your body is gone from the snapshot", taken().length === 0);
  routeMessage(state, snapshot({ tick: 402, you, players: me(120) }), 1350);
  routeMessage(state, welcome(), 2000);
  routeMessage(state, snapshot({ tick: 1, you, players: me(70) }), 2050); // a resumed, hurt body
  check("... nothing at a new session's first snapshot", taken().length === 0);
}

// --- Snapshot: your own projectiles -----------------------------------------------
// Your shot is drawn from the latest snapshot on by its velocity, not the
// interpolation delay behind: it leaves your token along the aim line. Others'
// shots stay interpolated, in step with the players who fired them.
{
  const state = joined();
  const yours = { id: 3, x: 100, y: 100, factionId: 1, vx: 600, vy: 0, mine: true };
  const theirs = { id: 4, x: 300, y: 100, factionId: 2, vx: -600, vy: 0 };
  routeMessage(state, snapshot({ tick: 1, revealed: [0, 1, 2, 3, 4, 5, 6, 7], projectiles: [yours, theirs] }), 1000);
  const drawn = (nowMs: number) => state.shotsAt(nowMs);
  check("your shot is not interpolated", !state.shots.ids(1100).has(3) && state.shots.ids(1100).has(4));
  check(
    "your shot: from the latest snapshot on by its velocity, no delay",
    same(drawn(1030).find((d) => d.factionId === 1), { x: 118, y: 100, vx: 600, vy: 0, factionId: 1, alpha: 1 }),
  );
  check("... at most 100 ms ahead (snapshots late)", drawn(1500).find((d) => d.factionId === 1)?.x === 160);
  check(
    "others' shots: interpolated, the delay behind",
    same(drawn(1000 + INTERP_DELAY_MS).find((d) => d.factionId === 2), { x: 300, y: 100, vx: -600, vy: 0, factionId: 2, alpha: 1 }),
  );
  routeMessage(state, snapshot({ tick: 4, projectiles: [{ ...theirs, x: 270 }] }), 1050);
  check("your shot gone from the latest snapshot in sight (a hit, its range): gone at once", drawn(1050).every((d) => d.factionId !== 1));
}

// --- Projectiles leaving sight -------------------------------------------------------
// The server sends only the projectiles in sight: one flying into the fog drops
// out of the snapshots and used to vanish there, as if it hit a wall. Now one
// gone where it would have flown out of sight flies on and fades over FADE_MS;
// one gone in sight (a hit, the end of its range) is gone as before.
{
  const state = joined(); // 4x3 cells of 100; cells 0,1 (row 0) and 4,5 (row 1) in sight
  routeMessage(state, snapshot({ tick: 3, revealed: [0, 1, 4, 5] }), 1000); // tick 3 = 50 ms: offset 950
  const intoFog = { id: 20, x: 190, y: 50, factionId: 2, vx: 600, vy: 0 }; // in 50 ms at 220: cell 2, fog
  const inSight = { id: 21, x: 110, y: 150, factionId: 3, vx: 600, vy: 0 }; // in 50 ms at 140: cell 5, sight
  const yoursIntoFog = { id: 22, x: 190, y: 150, factionId: 1, vx: 600, vy: 0, mine: true }; // -> cell 6, fog
  routeMessage(state, snapshot({ tick: 6, projectiles: [intoFog, inSight, yoursIntoFog] }), 1050);
  routeMessage(state, snapshot({ tick: 9 }), 1100); // all three gone
  const of = (nowMs: number, faction: number) => state.shotsAt(nowMs).find((d) => d.factionId === faction);
  // Others' shots are drawn INTERP_DELAY_MS behind: the last snapshot with them is
  // stamped 1050, so they leave it at now 1150.
  check("others' shot into the fog flies on from where it was", same(of(1200, 2), { x: 220, y: 50, vx: 600, vy: 0, factionId: 2, alpha: 0.75 }));
  check("... fading out over FADE_MS", (of(1100 + INTERP_DELAY_MS + FADE_MS - 60, 2)?.alpha ?? 1) < 0.35 && of(1050 + INTERP_DELAY_MS + FADE_MS, 2) === undefined);
  check("others' shot gone in sight (a hit): gone as before", of(1200, 3) === undefined);
  check("your shot into the fog flies on from its last snapshot, fading", same(of(1100, 1), { x: 220, y: 150, vx: 600, vy: 0, factionId: 1, alpha: 0.75 }));
  check("... and is gone after FADE_MS", of(1050 + FADE_MS, 1) === undefined);
}

// --- Remote motion: snapshots on the server's clock -------------------------------------
// Snapshots arrive unevenly (Wi-Fi: ±15 ms). Placed by their arrival time, a
// running player's interpolated motion went uneven and wobbled against the
// camera — its name unreadable (playtest #0). Placed on the server's clock
// (SnapshotClock), it stays even.
{
  const state = joined(); // 60 ticks/s: a snapshot every 3 ticks = 50 ms
  const jitter = [0, -15, 12, -9, 15, -14, 6, -3, 11, 8, 2, -12, 14, -6, 9, -11, 4, 13, -8, 1, -13, 10, -2, 15, -10];
  const arrivals = jitter.map((j, k) => ({ tick: 3 * k, x: 1000 + 15 * k, at: 1000 + 50 * k + 40 + j })); // 300 units/s
  const xs: number[] = [];
  let next = 0;
  for (let now = 1300; now < 2200; now += 1000 / 60) {
    while (next < arrivals.length && arrivals[next]!.at <= now) {
      const a = arrivals[next++]!;
      routeMessage(state, snapshot({ tick: a.tick, players: [{ id: 9, x: a.x, y: 500, hp: 100 }] }), a.at);
    }
    xs.push(state.interp.sample(9, now)!.x);
  }
  const steps = xs.slice(1).map((x, i) => x - xs[i]!);
  const spread = Math.max(...steps) - Math.min(...steps);
  check(`a running player moves evenly despite uneven arrivals (frame steps vary by ${spread.toFixed(3)})`, spread < 0.01);
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
  routeMessage(state, snapshot({ tick: 101, you, players: [{ id: 7, x: 150, y: 150, hp: 90 }], projectiles: [{ id: 5, x: 2, y: 2, factionId: 1, mine: true }], events: [{ tick: 101, kind: { case: "ability", value: { playerId: 9, abilityId: 1 } } }] }), 2050);
  check("(alive before it)", state.alive && state.hp === 90);
  routeMessage(state, msg({ case: "pong", value: { clientTimeMs: 1000 } } as never), 1040);
  routeMessage(state, msg({ case: "error", value: { code: ErrorCode.IDLE_TIMEOUT, fatal: true } } as never), 2100);
  state.myFaction = 2;

  routeMessage(state, welcome(), 5000);
  check("a new Welcome: nothing of the old session is alive", state.life === LifeState.NOT_SPAWNED && state.hp === 0 && !state.alive);
  check("... no players, no remote positions, no projectiles", state.players.size === 0 && state.interp.ids(5000).size === 0 && state.shotMeta.size === 0 && state.shots.ids(5000).size === 0 && state.shotsAt(5000).length === 0);
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

// --- Capitals: static knowledge from Welcome, seen through the fog -----------------------
{
  const state = new GameState();
  routeMessage(state, welcome({ map: { capitals: [{ factionId: 1, cell: 5, protectedRadius: 1 }, { factionId: 2, cell: 10, protectedRadius: 1 }] } }), 1000);
  check("capitals: from Welcome", same(state.capitals, [{ factionId: 1, cell: 5, protectedRadius: 1 }, { factionId: 2, cell: 10, protectedRadius: 1 }]));
  routeMessage(state, welcome(), 2000); // a new session, on a server with none
  check("... a new Welcome replaces them", state.capitals.length === 0);
}

// --- Your faction: chosen once, at the first spawn, for the season ----------------------
// The choice shows until the first spawn and never again, after death neither: the
// server refuses another faction for the season.
{
  const state = joined();
  check("faction: to choose before the first spawn", state.choosingFaction);
  state.selectedFaction = 2; // a card clicked
  check("faction: the first spawn locks the one picked", state.spawnFaction() === 2 && state.myFaction === 2);
  check("... told to the roster at once (it doesn't echo our own)", state.roster.factionOf(7) === 2);
  check("... no choice from then on", !state.choosingFaction);
  routeMessage(state, snapshot({ tick: 10, you: { life: LifeState.DEAD, respawnTick: 10 } }), 1000);
  check("... none after death either", !state.choosingFaction);
  state.selectedFaction = 1;
  check("... a respawn goes in the locked faction", state.spawnFaction() === 2);
}
{
  const state = joined();
  routeMessage(state, msg({ case: "roster", value: { upsert: [{ id: 7, name: "Me", factionId: 2 }], full: true } } as never), 1000);
  check("faction: a character back with one: no choice, it spawns in it", !state.choosingFaction && state.spawnFaction() === 2);
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
  check("hint: before spawning", usualHint(state, false) === "Выберите фракцию и нажмите «В бой» · M — вся карта");
  check("hint: in the map view", usualHint(state, true).endsWith("M — к игроку"));
  check("may spawn before the first spawn", state.maySpawn);
  state.myFaction = 2; // locked (a character back after leaving the world)
  check("hint: back with a faction: no choice", usualHint(state, false) === "«В бой» — в столицу фракции · M — вся карта");
  routeMessage(state, snapshot({ tick: 200, you: { life: LifeState.DEAD, respawnTick: 260 } }), 1000);
  check("hint: dead, waiting", usualHint(state, false) === "Убит · возрождение через 1с · M — вся карта");
  check("... may not respawn yet", !state.maySpawn);
  routeMessage(state, snapshot({ tick: 260, you: { life: LifeState.DEAD, respawnTick: 260 } }), 1000);
  check("hint: dead, may respawn — at the capital, no choice", usualHint(state, false) === "Убит · «Возродиться» — снова в столице · M — вся карта");
  check("... may respawn", state.maySpawn);
  routeMessage(state, snapshot({ tick: 300, you: { life: LifeState.ALIVE } }), 1000);
  check("may not spawn while alive", !state.maySpawn);
  check("hint: alive, no cell captured yet -> how to capture (next to your land)", usualHint(state, false) === "Встаньте на чужую или ничью клетку рядом со своей и держите E — захват");
  state.captureLearned = true; // see onboarding_check.ts
  check("hint: none while alive once you have captured", usualHint(state, false) === "");
}

// --- The cell under you --------------------------------------------------------------
{
  const state = joined();
  check("not spawned: no cell under you", state.cellUnderMe() === undefined);
  const owners = new Uint8Array([0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0]);
  routeMessage(state, msg({ case: "mapState", value: { ownerFactionIds: owners, captures: [] } } as never), 1000);
  routeMessage(state, snapshot({ tick: 10, you: { life: LifeState.ALIVE }, players: [{ id: 7, x: 150, y: 150, hp: 90 }] }), 1000);
  check("alive: the cell under the predicted position", state.cellUnderMe()?.index === 5 && state.cellUnderMe()?.owner === 2);
  state.predictor?.reset({ x: 399, y: 250 });
  check("... it follows the prediction", state.cellUnderMe()?.index === 11);
  routeMessage(state, snapshot({ tick: 20, you: { life: LifeState.DEAD, respawnTick: 400 } }), 1000);
  check("dead: no cell under you", state.cellUnderMe() === undefined);
}

console.log("VERDICT:", failures === 0 ? "PASS" : `FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
