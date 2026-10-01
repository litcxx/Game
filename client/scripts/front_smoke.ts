// Front e2e (GAME-017): land is taken only next to your own. Spawn at the "front"
// capital of the smoke map (its one cell is ours), walk two cells right onto FAR
// and hold capture ('e'): nothing happens, FAR doesn't border our land. Walk back
// onto NEAR, between them, and take it; now FAR borders ours: step onto it again
// and take it too — the front moves out from the capital.
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import { ClientMessageSchema, ProtocolVersion, ServerMessageSchema } from "../src/gen/game/v1/protocol_pb.js";
import { spot } from "./smokeMap.js";
import { uniqueName } from "./uniqueName.js";

const HOME = spot("front"); // (70, 70)
const NEAR = HOME.cell + 1; // (71, 70): its x from HOME.x + 50 to HOME.x + 150
const FAR = HOME.cell + 2; // (72, 70): from HOME.x + 150 to HOME.x + 250
const HOLD_FAR_MS = 2500; // longer than a neutral cell's capture (90 ticks = 1.5 s)

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const ws = new WebSocket(URL);
ws.binaryType = "arraybuffer";

const faction = HOME.factionId;
let myId = 0;
let seq = 0;
type Phase = "spawning" | "toFar" | "holdFar" | "toNear" | "holdNear" | "backToFar" | "takeFar" | "done";
let phase: Phase = "spawning";
let heldOnFar = false; // stood on FAR holding E for HOLD_FAR_MS
let farMoved = false; // ...and FAR showed progress or turned ours meanwhile
let nearTaken = false;
let farTaken = false;

function send(payload: Parameters<typeof create<typeof ClientMessageSchema>>[1]["payload"]): void {
  ws.send(toBinary(ClientMessageSchema, create(ClientMessageSchema, { payload })));
}
const input = (moveX: number, capturing: boolean) =>
  send({ case: "input", value: { frames: [{ seq: ++seq, moveX, moveY: 0, capturing }] } });

function finish(): void {
  console.log(
    `[front] player_id=${myId} faction=${faction} phase=${phase} heldOnFar=${heldOnFar} ` +
      `farMoved=${farMoved} nearTaken=${nearTaken} farTaken=${farTaken}`,
  );
  const pass = heldOnFar && !farMoved && nearTaken && farTaken;
  console.log("VERDICT:", pass ? "PASS" : "FAIL");
  ws.close();
  process.exit(pass ? 0 : 1);
}

ws.onopen = () => send({ case: "hello", value: { protocolVersion: ProtocolVersion.CURRENT, name: uniqueName("front") } });
ws.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    myId = m.payload.value.playerId;
    send({ case: "spawn", value: { factionId: faction } });
    return;
  }
  if (m.payload.case !== "snapshot") return;
  for (const c of m.payload.value.cells) {
    if (c.index === FAR && phase === "holdFar" && (c.captureProgress > 0 || c.ownerFactionId === faction)) farMoved = true;
    if (c.index === NEAR && c.ownerFactionId === faction) nearTaken = true;
    if (c.index === FAR && c.ownerFactionId === faction && phase === "takeFar") farTaken = true;
  }
  const me = m.payload.value.players.find((p) => p.id === myId);
  if (!me) return;
  if (phase === "spawning") {
    phase = "toFar";
    input(1, false);
  } else if (phase === "toFar" && me.x >= HOME.x + 170) {
    phase = "holdFar"; // on FAR: stop and hold E
    input(0, true);
    setTimeout(() => {
      heldOnFar = true;
      phase = "toNear";
      input(-1, false);
    }, HOLD_FAR_MS);
  } else if (phase === "toNear" && me.x <= HOME.x + 120) {
    phase = "holdNear"; // on NEAR: stop and hold E
    input(0, true);
  } else if (phase === "holdNear" && nearTaken) {
    phase = "backToFar";
    input(1, false);
  } else if (phase === "backToFar" && me.x >= HOME.x + 170) {
    phase = "takeFar"; // on FAR again: it borders NEAR, ours now
    input(0, true);
  } else if (phase === "takeFar" && farTaken) {
    phase = "done";
    finish();
  }
};
ws.onerror = () => console.error("[front] websocket error");

setTimeout(finish, 12000);
