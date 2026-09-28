// Fog of war e2e: a scout (faction 1) spawns at (20,20); an enemy (faction 2)
// spawns far off at (80,80); another (faction 3) spawns up to 2 cells from the
// scout 1.5 s later. Verify the joiner's MapState reveals nothing, spawning
// reveals the disc of cells within the vision radius, the far enemy is never sent
// (both ways), and the near one is.
//   VISION_CELLS — the server's vision_radius in cells (config: 300 units = 3).
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import {
  ClientMessageSchema,
  ServerMessageSchema,
  type ClientMessage,
} from "../src/gen/game/v1/protocol_pb.js";

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const R = Number(process.env.VISION_CELLS ?? 3); // sight, in cells
if (!Number.isInteger(R) || R < 1) {
  console.error(`[fog] VISION_CELLS must be a whole number of cells >= 1, got '${process.env.VISION_CELLS}'`);
  process.exit(1);
}
const MAP = 100; // cells per row
const cell = (col: number, row: number) => row * MAP + col;
const SCOUT_CELL = cell(20, 20);
const FAR_CELL = cell(80, 80);
const NEAR_CELL = cell(20 + Math.min(2, R), 20); // in sight

function send(ws: WebSocket, msg: ClientMessage): void {
  ws.send(toBinary(ClientMessageSchema, msg));
}
const helloMsg = (name: string) =>
  create(ClientMessageSchema, { payload: { case: "hello", value: { protocolVersion: 1, name } } });
const spawnMsg = (cellIndex: number, factionId: number) =>
  create(ClientMessageSchema, { payload: { case: "spawn", value: { cell: cellIndex, factionId } } });

interface Client {
  ws: WebSocket;
  id: number;
}

// Opens a client that says Hello, spawns at `cellIndex` in the bar's faction
// `factionSlot` on Welcome, and hands every message to `onMessage`.
function open(
  name: string,
  cellIndex: number,
  factionSlot: number,
  onMessage: (m: ReturnType<typeof decode>) => void,
): Client {
  const client: Client = { ws: new WebSocket(URL), id: 0 };
  client.ws.binaryType = "arraybuffer";
  client.ws.onopen = () => send(client.ws, helloMsg(name));
  client.ws.onerror = () => console.error(`[fog] ${name} socket error`);
  client.ws.onmessage = (ev: MessageEvent) => {
    const m = decode(ev);
    if (m.payload.case === "welcome") {
      client.id = m.payload.value.playerId;
      const factions = m.payload.value.factions;
      send(client.ws, spawnMsg(cellIndex, factions[factionSlot]?.id ?? factionSlot + 1));
    }
    onMessage(m);
  };
  return client;
}
const decode = (ev: MessageEvent) => fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));

let mapStateBlank: boolean | undefined;
let revealedOnSpawn: number[] = [];
let scoutSawFar = false;
let scoutSawNear = false;
let farSawScout = false;

const scout = open("scout", SCOUT_CELL, 0, (m) => {
  if (m.payload.case === "mapState") {
    mapStateBlank = m.payload.value.owners.every((b) => b === 0);
  } else if (m.payload.case === "snapshot") {
    const s = m.payload.value;
    if (revealedOnSpawn.length === 0) revealedOnSpawn = [...s.revealed];
    if (s.players.some((p) => p.id === far.id && far.id !== 0)) scoutSawFar = true;
    if (s.players.some((p) => p.id === near?.id && near.id !== 0)) scoutSawNear = true;
  }
});
const far = open("far", FAR_CELL, 1, (m) => {
  if (m.payload.case === "snapshot" && m.payload.value.players.some((p) => p.id === scout.id)) {
    farSawScout = true;
  }
});
let near: Client | undefined;
setTimeout(() => {
  near = open("near", NEAR_CELL, 2, () => {}); // a third faction: far must not share its sight
}, 1500);

setTimeout(() => {
  // Every cell within R cells (inclusive) is revealed, the next ones out — R + 1
  // and sqrt(R^2 + 1) cells away — are not. (Not an exact count: territory the
  // faction owns from earlier runs sees too.)
  const revealed = new Set(revealedOnSpawn);
  let discOk = !revealed.has(cell(20 + R + 1, 20)) && !revealed.has(cell(20 + R, 21));
  for (let dy = -R; dy <= R; dy++) {
    for (let dx = -R; dx <= R; dx++) {
      if (dx * dx + dy * dy <= R * R && !revealed.has(cell(20 + dx, 20 + dy))) discOk = false;
    }
  }
  console.log(
    `[fog] R=${R} scout=${scout.id} far=${far.id} near=${near?.id ?? 0} mapStateBlank=${mapStateBlank} ` +
      `revealed=${revealed.size} discOk=${discOk} scoutSawFar=${scoutSawFar} ` +
      `farSawScout=${farSawScout} scoutSawNear=${scoutSawNear}`,
  );
  const pass = mapStateBlank === true && discOk && !scoutSawFar && !farSawScout && scoutSawNear;
  console.log("VERDICT:", pass ? "PASS" : "FAIL");
  for (const c of [scout, far, near]) c?.ws.close();
  process.exit(pass ? 0 : 1);
}, 3500);
