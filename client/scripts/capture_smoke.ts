// M4-first-iteration e2e: spawn, hold capture ('e'), verify the server flips the
// cell under the player to our faction and reports it in Snapshot.cells.
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import { ClientMessageSchema, ServerMessageSchema } from "../src/gen/game/v1/protocol_pb.js";

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const ws = new WebSocket(URL);
ws.binaryType = "arraybuffer";

let myId = 0;
let faction = 0;
const spawnCell = 5050; // 100-wide map: col 50, row 50
let captured = false;
let progressSeen = false;

function sendHello(): void {
  ws.send(
    toBinary(
      ClientMessageSchema,
      create(ClientMessageSchema, {
        payload: { case: "hello", value: { protocolVersion: 1, name: "cap" } },
      }),
    ),
  );
}
function sendSpawnAndCapture(): void {
  ws.send(
    toBinary(
      ClientMessageSchema,
      create(ClientMessageSchema, {
        payload: { case: "spawn", value: { cell: spawnCell, factionId: faction } },
      }),
    ),
  );
  ws.send(
    toBinary(
      ClientMessageSchema,
      create(ClientMessageSchema, {
        payload: { case: "input", value: { frames: [{ seq: 1, moveX: 0, moveY: 0, capturing: true }] } },
      }),
    ),
  );
}

ws.onopen = () => sendHello();
ws.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    myId = m.payload.value.playerId;
    faction = m.payload.value.factions[0]?.id ?? 1;
    sendSpawnAndCapture();
  } else if (m.payload.case === "snapshot") {
    for (const c of m.payload.value.cells) {
      if (c.index !== spawnCell) continue;
      if (c.captureProgress > 0 && c.owner === 0) progressSeen = true;
      if (c.owner === faction) captured = true;
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
}, 1800);
