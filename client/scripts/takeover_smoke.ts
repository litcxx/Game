// Verifies the data behind the takeover gradient: A (faction 1) captures a
// neutral cell, then B (faction 2) captures that OWNED cell. Mid-takeover the
// server should keep owner = A while B's capture_progress rises (0<..<100),
// then flip owner = B. (The client cross-fades those two.)
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import {
  ClientMessageSchema,
  ServerMessageSchema,
  type ClientMessage,
} from "../src/gen/game/v1/protocol_pb.js";

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const CELL = 50 * 100 + 50; // central cell (col 50, row 50)

const a = new WebSocket(URL);
const b = new WebSocket(URL);
a.binaryType = "arraybuffer";
b.binaryType = "arraybuffer";

let fA = 0;
let fB = 0;
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
  create(ClientMessageSchema, { payload: { case: "hello", value: { protocolVersion: 1, name } } });
const spawn = (factionId: number) =>
  create(ClientMessageSchema, { payload: { case: "spawn", value: { cell: CELL, factionId } } });
const capture = (seq: number, on: boolean) =>
  create(ClientMessageSchema, {
    payload: { case: "input", value: { frames: [{ seq, moveX: 0, moveY: 0, capturing: on, attack: false }] } },
  });

function startB(): void {
  if (!aOwned || !bReady || bStarted) return;
  bStarted = true;
  send(b, spawn(fB));
  send(b, capture(++bSeq, true));
}

a.onopen = () => send(a, hello("A"));
b.onopen = () => send(b, hello("B"));

a.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    fA = m.payload.value.factions[0]?.id ?? 1;
    send(a, spawn(fA));
    send(a, capture(++aSeq, true));
  } else if (m.payload.case === "snapshot") {
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
    fB = m.payload.value.factions[1]?.id ?? 2;
    bReady = true;
    startB();
  } else if (m.payload.case === "snapshot") {
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
}, 5000);
