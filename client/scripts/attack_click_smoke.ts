// Mimic a browser attack CLICK (attack=true, then attack=false after CLICK_MS)
// against an ENEMY in range: even a short click must land a hit — the
// level-triggered attack must not lose fast clicks within one server tick.
// Fails (exit 1) on NO DAMAGE.
//   CLICK_MS=0 npx tsx scripts/attack_click_smoke.ts   # instant (same tick)
//   CLICK_MS=150 npx tsx scripts/attack_click_smoke.ts # normal click
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import {
  ClientMessageSchema,
  ServerMessageSchema,
  type ClientMessage,
} from "../src/gen/game/v1/protocol_pb.js";
import { uniqueName } from "./uniqueName.js";

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const CLICK_MS = Number(process.env.CLICK_MS ?? "0");

const attacker = new WebSocket(URL);
const victim = new WebSocket(URL);
attacker.binaryType = "arraybuffer";
victim.binaryType = "arraybuffer";

let atkId = 0;
let vicId = 0;
let atkSpawned = false;
let vicSpawned = false;
let clicked = false;
let hitSeen = false;
let victimHp = -1;
let seq = 0;

// Its own cells: a player stays in the world for the reconnect grace (30 s)
// after its socket closes, so other smokes' bodies may still stand elsewhere.
const atkCell = 65 * 100 + 65; // (col 65, row 65) -> (6550, 6550)
const vicCell = 65 * 100 + 66; // (col 66, row 65) -> (6650, 6550), 100 units away (in range 120)

function send(ws: WebSocket, msg: ClientMessage): void {
  ws.send(toBinary(ClientMessageSchema, msg));
}
const helloMsg = (name: string) =>
  create(ClientMessageSchema, { payload: { case: "hello", value: { protocolVersion: 1, name: uniqueName(name) } } });
const spawnMsg = (cell: number, factionId: number) =>
  create(ClientMessageSchema, { payload: { case: "spawn", value: { cell, factionId } } });
const inputMsg = (attack: boolean) =>
  create(ClientMessageSchema, {
    payload: {
      case: "input",
      value: { frames: [{ seq: ++seq, moveX: 0, moveY: 0, capturing: false, attack }] },
    },
  });

function maybeClick(): void {
  if (clicked || !atkSpawned || !vicSpawned) return;
  clicked = true;
  send(attacker, inputMsg(true)); // mousedown
  setTimeout(() => send(attacker, inputMsg(false)), CLICK_MS); // mouseup
}

attacker.onopen = () => send(attacker, helloMsg("atk"));
victim.onopen = () => send(victim, helloMsg("vic"));

attacker.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    atkId = m.payload.value.playerId;
    send(attacker, spawnMsg(atkCell, m.payload.value.factions[0]?.id ?? 1));
    atkSpawned = true;
    maybeClick();
  } else if (m.payload.case === "snapshot") {
    const body = m.payload.value.players.find((p) => p.id === vicId);
    if (body) victimHp = body.hp;
    for (const e of m.payload.value.events) {
      if (e.kind.case === "hit" && e.kind.value.targetId === vicId) hitSeen = true;
    }
  }
};

victim.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    vicId = m.payload.value.playerId;
    const fs = m.payload.value.factions;
    send(victim, spawnMsg(vicCell, fs[1]?.id ?? fs[0]?.id ?? 2)); // different faction
    vicSpawned = true;
    maybeClick();
  }
};

setTimeout(() => {
  console.log(`[click CLICK_MS=${CLICK_MS}] atk=${atkId} vic=${vicId} hitSeen=${hitSeen} victimHp=${victimHp}`);
  const damaged = victimHp >= 0 && victimHp < 100;
  console.log("VERDICT:", hitSeen && damaged ? "DAMAGED" : "NO DAMAGE");
  attacker.close();
  victim.close();
  process.exit(hitSeen && damaged ? 0 : 1);
}, 2000);
