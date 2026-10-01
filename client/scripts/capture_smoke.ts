// Capture e2e: spawn at the "capture" capital of the smoke map (its cell is ours
// already), step right onto the neutral cell next to it, hold capture ('e'), and
// verify the server flips that cell to our faction and reports it in
// Snapshot.cells — first as progress on a neutral cell, then as ours.
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import { ClientMessageSchema, ProtocolVersion, ServerMessageSchema } from "../src/gen/game/v1/protocol_pb.js";
import { spot } from "./smokeMap.js";
import { uniqueName } from "./uniqueName.js";

const HOME = spot("capture");
const TARGET = HOME.cell + 1; // the neutral cell to its right

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const ws = new WebSocket(URL);
ws.binaryType = "arraybuffer";

let myId = 0;
const faction = HOME.factionId;
let stepping = false;
let holding = false;
let captured = false;
let progressSeen = false;

function send(payload: Parameters<typeof create<typeof ClientMessageSchema>>[1]["payload"]): void {
  ws.send(toBinary(ClientMessageSchema, create(ClientMessageSchema, { payload })));
}

ws.onopen = () => send({ case: "hello", value: { protocolVersion: ProtocolVersion.CURRENT, name: uniqueName("cap") } });
ws.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    myId = m.payload.value.playerId;
    send({ case: "spawn", value: { factionId: faction } });
  } else if (m.payload.case === "snapshot") {
    const me = m.payload.value.players.find((p) => p.id === myId);
    if (me && !stepping) {
      stepping = true;
      send({ case: "input", value: { frames: [{ seq: 1, moveX: 1, moveY: 0 }] } }); // toward TARGET
    }
    if (me && stepping && !holding && me.x >= HOME.x + 70) {
      holding = true; // on TARGET (its centre is 100 right of home): stop and hold E
      send({ case: "input", value: { frames: [{ seq: 2, moveX: 0, moveY: 0, capturing: true }] } });
    }
    for (const c of m.payload.value.cells) {
      if (c.index !== TARGET) continue;
      if (c.captureProgress > 0 && c.ownerFactionId === 0) progressSeen = true;
      if (c.ownerFactionId === faction) captured = true;
    }
  }
};
ws.onerror = () => console.error("[cap] websocket error");

setTimeout(() => {
  console.log(`[cap] player_id=${myId} faction=${faction} progressSeen=${progressSeen} captured=${captured}`);
  const pass = myId >= 1 && progressSeen && captured;
  console.log("VERDICT:", pass ? "PASS" : "FAIL");
  ws.close();
  process.exit(pass ? 0 : 1);
}, 3000);
