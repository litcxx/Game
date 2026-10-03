// M2 end-to-end: Hello -> Spawn (at the "m2" capital of the smoke map) -> Input,
// then verify the server moves us (own PlayerState.x grows across snapshots).
// Uses the generated protobuf-es code.
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import { ClientMessageSchema, ProtocolVersion, ServerMessageSchema } from "../src/gen/game/v1/protocol_pb.js";
import { spot } from "./smokeMap.js";
import { uniqueName } from "./uniqueName.js";

const HOME = spot("m2");

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const ws = new WebSocket(URL);
ws.binaryType = "arraybuffer";

let myId = 0;
let sentInput = false;
const xs: number[] = [];

function send(payload: Parameters<typeof create<typeof ClientMessageSchema>>[1]["payload"]): void {
  ws.send(toBinary(ClientMessageSchema, create(ClientMessageSchema, { payload })));
}

ws.onopen = () => send({ case: "hello", value: { protocolVersion: ProtocolVersion.CURRENT, name: uniqueName("m2") } });

ws.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    myId = m.payload.value.playerId;
    send({ case: "spawn", value: { factionId: HOME.factionId } });
  } else if (m.payload.case === "snapshot") {
    const me = m.payload.value.players.find((p) => p.id === myId);
    if (me) {
      xs.push(me.x);
      if (!sentInput) {
        send({ case: "input", value: { frames: [{ inputSeq: 1, moveX: 1, moveY: 0 }] } });
        sentInput = true;
      }
    }
  }
};

ws.onerror = () => console.error("[m2] websocket error");

setTimeout(() => {
  const moved = xs.length >= 2 && xs[xs.length - 1]! > xs[0]!;
  console.log(`[m2] player_id=${myId} snapshots_with_me=${xs.length} x: ${xs[0]} -> ${xs.at(-1)}`);
  const pass = myId >= 1 && moved;
  console.log("VERDICT:", pass ? "PASS" : "FAIL");
  ws.close();
  process.exit(pass ? 0 : 1);
}, 1500);
