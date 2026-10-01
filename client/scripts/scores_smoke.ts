// Faction scores e2e (GAME-018): the server's count is the real one. A joiner
// sees nothing of the map yet (MapState all unknown), yet the FactionScores right
// after its roster list every faction of the smoke map, each with its capital's
// cell at least. After it spawns in the "scores" faction it counts online there;
// the scores keep coming about once a second.
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import { ClientMessageSchema, ProtocolVersion, ServerMessageSchema, type FactionScore } from "../src/gen/game/v1/protocol_pb.js";
import { factionIds, spot } from "./smokeMap.js";
import { uniqueName } from "./uniqueName.js";

const HOME = spot("scores");
const FACTIONS = factionIds();

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const ws = new WebSocket(URL);
ws.binaryType = "arraybuffer";

const order: string[] = []; // the kinds of messages, as they come
let mapBlank = false;
let firstScores: FactionScore[] = [];
const arrivals: number[] = []; // ms, of every FactionScores
let spawned = false;
let countedOnline = false;

function send(payload: Parameters<typeof create<typeof ClientMessageSchema>>[1]["payload"]): void {
  ws.send(toBinary(ClientMessageSchema, create(ClientMessageSchema, { payload })));
}

ws.onopen = () => send({ case: "hello", value: { protocolVersion: ProtocolVersion.CURRENT, name: uniqueName("scores") } });
ws.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case) order.push(m.payload.case);
  if (m.payload.case === "mapState") {
    mapBlank = m.payload.value.ownerFactionIds.every((b) => b === 0);
  } else if (m.payload.case === "factionScores") {
    const scores = m.payload.value.scores;
    arrivals.push(performance.now());
    if (arrivals.length === 1) {
      firstScores = scores;
      send({ case: "spawn", value: { factionId: HOME.factionId } });
      spawned = true;
    } else if (spawned && (scores.find((s) => s.factionId === HOME.factionId)?.online ?? 0) >= 1) {
      countedOnline = true;
    }
  }
};
ws.onerror = () => console.error("[scores] websocket error");

setTimeout(() => {
  const afterRoster = order.indexOf("factionScores") === order.indexOf("roster") + 1;
  const everyFaction =
    firstScores.length === FACTIONS.length && FACTIONS.every((id, i) => firstScores[i]?.factionId === id && firstScores[i]!.cells >= 1);
  const gaps = arrivals.slice(1).map((t, i) => t - arrivals[i]!);
  const steady = arrivals.length >= 3 && gaps.slice(1).every((g) => g > 700 && g < 1300); // the first gap is to the next whole second
  console.log(
    `[scores] order=${order.slice(0, 4).join(",")} mapBlank=${mapBlank} factions=${firstScores.length}/${FACTIONS.length} ` +
      `cells=${firstScores.map((s) => s.cells).join(",")} arrivals=${arrivals.length} gaps=${gaps.map(Math.round).join(",")} countedOnline=${countedOnline}`,
  );
  const pass = afterRoster && mapBlank && everyFaction && steady && countedOnline;
  console.log("VERDICT:", pass ? "PASS" : "FAIL");
  ws.close();
  process.exit(pass ? 0 : 1);
}, 3500);
