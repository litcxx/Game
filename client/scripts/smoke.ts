// Headless end-to-end check: uses the SAME generated protobuf-es code as the
// browser client to connect, send Hello, and decode the server's reply.
// Run against a live server:  npx tsx scripts/smoke.ts
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import { ClientMessageSchema, ServerMessageSchema } from "../src/gen/game/v1/protocol_pb.js";

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const seen: string[] = [];
let pass = false;

const ws = new WebSocket(URL);
ws.binaryType = "arraybuffer";

ws.onopen = () => {
  const hello = create(ClientMessageSchema, {
    payload: { case: "hello", value: { protocolVersion: 1, name: "smoke" } },
  });
  ws.send(toBinary(ClientMessageSchema, hello));
};

ws.onmessage = (ev: MessageEvent) => {
  const msg = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  seen.push(msg.payload.case ?? "none");
  if (msg.payload.case === "welcome") {
    const w = msg.payload.value;
    console.log(
      `[smoke] Welcome player_id=${w.playerId} map=${w.config?.mapWidth}x${w.config?.mapHeight} factions=${w.factions.length}`,
    );
    pass = w.playerId >= 1 && (w.config?.mapWidth ?? 0) > 0 && w.factions.length > 0;
  }
};

ws.onerror = () => console.error("[smoke] websocket error");

setTimeout(() => {
  console.log("[smoke] received:", seen.join(","));
  console.log("VERDICT:", pass ? "PASS" : "FAIL");
  ws.close();
  process.exit(pass ? 0 : 1);
}, 900);
