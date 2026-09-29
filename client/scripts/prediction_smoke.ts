// e2e: send per-tick input frames (6x move-right + 1x stop) and confirm the
// server applies exactly one per tick (position advances ~6 ticks) and acks
// via SelfState.last_input_seq.
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import {
  ClientMessageSchema,
  ServerMessageSchema,
  type ClientMessage,
} from "../src/gen/game/v1/protocol_pb.js";
import { uniqueName } from "./uniqueName.js";

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const ws = new WebSocket(URL);
ws.binaryType = "arraybuffer";

const spawnCell = 50 * 100 + 50; // center (5050, 5050)
let myId = 0;
let moveSpeed = 300;
let tickRate = 60;
const startX = 5050;
let lastX = -1;
let lastSeq = 0;

function send(msg: ClientMessage): void {
  ws.send(toBinary(ClientMessageSchema, msg));
}

ws.onopen = () =>
  send(create(ClientMessageSchema, { payload: { case: "hello", value: { protocolVersion: 1, name: uniqueName("pred") } } }));

ws.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    myId = m.payload.value.playerId;
    moveSpeed = m.payload.value.config?.moveSpeed ?? 300;
    tickRate = m.payload.value.config?.tickRate ?? 60;
    const faction = m.payload.value.factions[0]?.id ?? 1;
    send(create(ClientMessageSchema, { payload: { case: "spawn", value: { cell: spawnCell, factionId: faction } } }));
    // one Input message, 7 frames (<=8): 6 move-right then stop
    const frames = [];
    for (let i = 1; i <= 6; i++) frames.push({ seq: i, moveX: 1, moveY: 0, capturing: false, attack: false });
    frames.push({ seq: 7, moveX: 0, moveY: 0, capturing: false, attack: false });
    send(create(ClientMessageSchema, { payload: { case: "input", value: { frames } } }));
  } else if (m.payload.case === "snapshot") {
    lastSeq = m.payload.value.you?.lastInputSeq ?? lastSeq;
    const me = m.payload.value.players.find((p) => p.id === myId);
    if (me) lastX = me.x;
  }
};
ws.onerror = () => console.error("[pred] socket error");

setTimeout(() => {
  const expectedDx = 6 * moveSpeed * (1 / tickRate); // 6 ticks moved before the stop
  const dx = lastX - startX;
  console.log(`[pred] lastSeq=${lastSeq} startX=${startX} lastX=${lastX} dx=${dx} expected≈${expectedDx}`);
  const pass = lastSeq === 7 && Math.abs(dx - expectedDx) <= moveSpeed * (1 / tickRate) + 1;
  console.log("VERDICT:", pass ? "PASS" : "FAIL");
  ws.close();
  process.exit(pass ? 0 : 1);
}, 1500);
