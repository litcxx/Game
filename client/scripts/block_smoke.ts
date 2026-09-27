// Block e2e: a defender and an attacker of another faction stand 1 cell apart.
// The defender blocks once (its block ability from Welcome) just before the
// attacker starts holding melee: the first swing must be a blocked hit (no
// damage), the next one (after the 0.75 s cooldown) lands. Both uses are
// announced with AbilityEvents.
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import {
  AbilityKind,
  ClientMessageSchema,
  ServerMessageSchema,
  type ClientMessage,
} from "../src/gen/game/v1/protocol_pb.js";

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const attacker = new WebSocket(URL);
const defender = new WebSocket(URL);
attacker.binaryType = "arraybuffer";
defender.binaryType = "arraybuffer";

let atkId = 0;
let defId = 0;
let meleeId = 0;
let meleeDamage = 0;
let blockId = 0;
let maxHp = 0;
let atkAlive = false;
let defAlive = false;
let started = false;

let blockAnnounced = false;
let swingAnnounced = false;
let blockedHits = 0;
let landedDamage = -1;
let defHpAfterLanded = -1;

const atkCell = 35 * 100 + 35; // (col 35, row 35) -> (3550, 3550)
const defCell = 35 * 100 + 36; // (col 36, row 35) -> (3650, 3550): 100 units away

function send(ws: WebSocket, msg: ClientMessage): void {
  ws.send(toBinary(ClientMessageSchema, msg));
}
const helloMsg = (name: string) =>
  create(ClientMessageSchema, { payload: { case: "hello", value: { protocolVersion: 1, name } } });
const spawnMsg = (cell: number, factionId: number) =>
  create(ClientMessageSchema, { payload: { case: "spawn", value: { cell, factionId } } });
const framesMsg = (frames: { seq: number; attack: boolean; ability: number }[]) =>
  create(ClientMessageSchema, {
    payload: {
      case: "input",
      value: { frames: frames.map((f) => ({ ...f, moveX: 0, moveY: 0, capturing: false, aimX: 0, aimY: 0 })) },
    },
  });

function maybeStart(): void {
  if (started || !atkAlive || !defAlive || meleeId === 0 || blockId === 0) return;
  started = true;
  // Block for one tick's worth of input (it then lasts its duration)...
  send(defender, framesMsg([{ seq: 1, attack: true, ability: blockId }, { seq: 2, attack: false, ability: blockId }]));
  // ...and 10 ms later the attacker starts holding melee (swings each cooldown).
  setTimeout(() => send(attacker, framesMsg([{ seq: 1, attack: true, ability: meleeId }])), 10);
}

attacker.onopen = () => send(attacker, helloMsg("attacker"));
defender.onopen = () => send(defender, helloMsg("defender"));

attacker.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    const w = m.payload.value;
    atkId = w.playerId;
    maxHp = w.config?.maxHp ?? 0;
    const melee = w.abilities.find((a) => a.kind === AbilityKind.MELEE);
    meleeId = melee?.id ?? 0;
    meleeDamage = melee?.damage ?? 0;
    blockId = w.abilities.find((a) => a.kind === AbilityKind.BLOCK)?.id ?? 0;
    send(attacker, spawnMsg(atkCell, w.factions[0]?.id ?? 1));
  } else if (m.payload.case === "snapshot") {
    const s = m.payload.value;
    atkAlive = s.players.some((p) => p.id === atkId && p.hp > 0);
    defAlive = s.players.some((p) => p.id === defId && p.hp > 0);
    maybeStart();
    for (const e of s.events) {
      if (e.kind.case === "ability") {
        if (e.kind.value.playerId === defId && e.kind.value.abilityId === blockId) blockAnnounced = true;
        if (e.kind.value.playerId === atkId && e.kind.value.abilityId === meleeId) swingAnnounced = true;
      } else if (e.kind.case === "hit" && e.kind.value.attackerId === atkId && e.kind.value.targetId === defId) {
        if (e.kind.value.blocked) blockedHits++;
        else if (landedDamage < 0) {
          landedDamage = e.kind.value.damage;
          defHpAfterLanded = s.players.find((p) => p.id === defId)?.hp ?? -1;
          send(attacker, framesMsg([{ seq: 2, attack: false, ability: meleeId }])); // done
          setTimeout(finish, 200);
        }
      }
    }
  }
};

defender.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    defId = m.payload.value.playerId;
    const factions = m.payload.value.factions;
    send(defender, spawnMsg(defCell, factions[1]?.id ?? 2));
  }
};

attacker.onerror = () => console.error("[block] attacker socket error");
defender.onerror = () => console.error("[block] defender socket error");

let finished = false;
function finish(): void {
  if (finished) return;
  finished = true;
  console.log(
    `[block] atk=${atkId} def=${defId} blockAnnounced=${blockAnnounced} swingAnnounced=${swingAnnounced} ` +
      `blockedHits=${blockedHits} landedDamage=${landedDamage} (expected ${meleeDamage}) ` +
      `defHp=${defHpAfterLanded} (expected ${maxHp - meleeDamage})`,
  );
  const pass =
    blockAnnounced &&
    swingAnnounced &&
    blockedHits === 1 &&
    landedDamage === meleeDamage &&
    defHpAfterLanded === maxHp - meleeDamage;
  console.log("VERDICT:", pass ? "PASS" : "FAIL");
  attacker.close();
  defender.close();
  process.exit(pass ? 0 : 1);
}
setTimeout(finish, 3000);
