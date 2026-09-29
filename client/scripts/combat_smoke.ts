// M3 e2e: two clients spawn one cell apart (within attack_range). The attacker
// holds the attack key; the server swings an area hit each cooldown. Verify
// HitEvents flow back, the victim's hp drops, and a DeathEvent lands.
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import {
  ClientMessageSchema,
  ServerMessageSchema,
  type ClientMessage,
} from "../src/gen/game/v1/protocol_pb.js";
import { uniqueName } from "./uniqueName.js";

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const attacker = new WebSocket(URL);
const victim = new WebSocket(URL);
attacker.binaryType = "arraybuffer";
victim.binaryType = "arraybuffer";

let atkId = 0;
let vicId = 0;
let atkSpawned = false;
let vicSpawned = false;
let attacking = false;

let hitSeen = false;
let deathSeen = false;
let victimHp = -1;

// Its own cells: a player stays in the world for the reconnect grace (30 s)
// after its socket closes, so other smokes' bodies may still stand elsewhere.
const atkCell = 60 * 100 + 60; // (col 60, row 60) -> center (6050, 6050)
const vicCell = 60 * 100 + 61; // (col 61, row 60) -> center (6150, 6050), 100 units away

function send(ws: WebSocket, msg: ClientMessage): void {
  ws.send(toBinary(ClientMessageSchema, msg));
}
const helloMsg = (name: string) =>
  create(ClientMessageSchema, { payload: { case: "hello", value: { protocolVersion: 1, name: uniqueName(name) } } });
const spawnMsg = (cell: number, factionId: number) =>
  create(ClientMessageSchema, { payload: { case: "spawn", value: { cell, factionId } } });
const attackMsg = () =>
  create(ClientMessageSchema, {
    payload: {
      case: "input",
      value: { frames: [{ seq: 1, moveX: 0, moveY: 0, capturing: false, attack: true }] },
    },
  });

function maybeAttack(): void {
  if (!attacking && atkSpawned && vicSpawned && vicId >= 1) {
    attacking = true;
    send(attacker, attackMsg()); // held: the server swings each cooldown
  }
}

attacker.onopen = () => send(attacker, helloMsg("atk"));
victim.onopen = () => send(victim, helloMsg("vic"));

attacker.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    atkId = m.payload.value.playerId;
    const faction = m.payload.value.factions[0]?.id ?? 1;
    send(attacker, spawnMsg(atkCell, faction));
    atkSpawned = true;
    maybeAttack();
  } else if (m.payload.case === "snapshot") {
    const s = m.payload.value;
    const body = s.players.find((p) => p.id === vicId);
    if (body) victimHp = body.hp;
    for (const e of s.events) {
      if (e.kind.case === "hit" && e.kind.value.attackerId === atkId && e.kind.value.targetId === vicId) {
        hitSeen = true;
      }
      if (e.kind.case === "death" && e.kind.value.victimId === vicId) deathSeen = true;
    }
  }
};

victim.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    vicId = m.payload.value.playerId;
    const factions = m.payload.value.factions;
    const faction = factions[1]?.id ?? factions[0]?.id ?? 2; // different faction from attacker
    send(victim, spawnMsg(vicCell, faction));
    vicSpawned = true;
    maybeAttack();
  }
};

attacker.onerror = () => console.error("[combat] attacker socket error");
victim.onerror = () => console.error("[combat] victim socket error");

setTimeout(() => {
  console.log(
    `[combat] atk=${atkId} vic=${vicId} hitSeen=${hitSeen} deathSeen=${deathSeen} victimHp=${victimHp}`,
  );
  const pass = atkId >= 1 && vicId >= 1 && hitSeen && deathSeen && victimHp === 0;
  console.log("VERDICT:", pass ? "PASS" : "FAIL");
  attacker.close();
  victim.close();
  process.exit(pass ? 0 : 1);
}, 6500);
