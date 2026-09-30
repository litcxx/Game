// Verifies the data behind the takeover gradient: A captures the neutral cell
// between its capital and B's on the smoke map (it steps right onto it), then B
// (another faction, stepping left onto it) captures that OWNED cell. Mid-takeover the
// server should keep owner = A while B's capture_progress rises (0<..<100),
// then flip owner = B. (The client cross-fades those two.)
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import {
  ClientMessageSchema,
  ProtocolVersion,
  ServerMessageSchema,
  type ClientMessage,
} from "../src/gen/game/v1/protocol_pb.js";
import { spot } from "./smokeMap.js";
import { uniqueName } from "./uniqueName.js";

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const A_HOME = spot("takeover-a"); // (30, 30)
const B_HOME = spot("takeover-b"); // (32, 30)
const CELL = A_HOME.cell + 1; // (31, 30), between them: neutral

const a = new WebSocket(URL);
const b = new WebSocket(URL);
a.binaryType = "arraybuffer";
b.binaryType = "arraybuffer";

const fA = A_HOME.factionId;
const fB = B_HOME.factionId;
let aId = 0;
let bId = 0;
let aHolding = false;
let bHolding = false;
let aOwned = false;
let bReady = false;
let bStarted = false;
let sawGradient = false; // owner == fA while B's progress is between 0 and 100
let finalOwner = 0;
let aSeq = 0;
let bSeq = 0;

function send(ws: WebSocket, msg: ClientMessage): void {
  ws.send(toBinary(ClientMessageSchema, msg));
}
const hello = (name: string) =>
  create(ClientMessageSchema, { payload: { case: "hello", value: { protocolVersion: ProtocolVersion.CURRENT, name: uniqueName(name) } } });
const spawn = (factionId: number) => create(ClientMessageSchema, { payload: { case: "spawn", value: { factionId } } });
const walk = (seq: number, moveX: number) =>
  create(ClientMessageSchema, { payload: { case: "input", value: { frames: [{ seq, moveX, moveY: 0 }] } } });
const capture = (seq: number, on: boolean) =>
  create(ClientMessageSchema, {
    payload: { case: "input", value: { frames: [{ seq, moveX: 0, moveY: 0, capturing: on, attack: false }] } },
  });

function startB(): void {
  if (!aOwned || !bReady || bStarted) return;
  bStarted = true;
  send(b, spawn(fB));
  send(b, walk(++bSeq, -1)); // left, onto CELL
}

a.onopen = () => send(a, hello("A"));
b.onopen = () => send(b, hello("B"));

a.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    aId = m.payload.value.playerId;
    send(a, spawn(fA));
    send(a, walk(++aSeq, 1)); // right, onto CELL
  } else if (m.payload.case === "snapshot") {
    const me = m.payload.value.players.find((p) => p.id === aId);
    if (me && !aHolding && me.x >= A_HOME.x + 70) {
      aHolding = true; // on CELL: stop and hold E
      send(a, capture(++aSeq, true));
    }
    for (const c of m.payload.value.cells) {
      if (c.index !== CELL) continue;
      if (!aOwned && c.owner === fA) {
        aOwned = true;
        send(a, capture(++aSeq, false)); // A stops so B isn't contested
        startB();
      }
    }
  }
};

b.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    bId = m.payload.value.playerId;
    bReady = true;
    startB();
  } else if (m.payload.case === "snapshot") {
    const me = m.payload.value.players.find((p) => p.id === bId);
    if (me && bStarted && !bHolding && me.x <= B_HOME.x - 70) {
      bHolding = true; // on CELL: stop and hold E
      send(b, capture(++bSeq, true));
    }
    for (const c of m.payload.value.cells) {
      if (c.index !== CELL) continue;
      finalOwner = c.owner;
      if (c.owner === fA && c.captureFaction === fB && c.captureProgress > 0 && c.captureProgress < 100) {
        sawGradient = true;
      }
    }
  }
};

a.onerror = () => console.error("[takeover] A socket error");
b.onerror = () => console.error("[takeover] B socket error");

setTimeout(() => {
  console.log(`[takeover] fA=${fA} fB=${fB} aOwned=${aOwned} sawGradient=${sawGradient} finalOwner=${finalOwner}`);
  const pass = aOwned && sawGradient && finalOwner === fB;
  console.log("VERDICT:", pass ? "PASS" : "FAIL");
  a.close();
  b.close();
  process.exit(pass ? 0 : 1);
}, 6000);
