// The faction's map outlives a reload (GAME-020). A scout of the "explored"
// faction spawns at its capital and walks off, so part of what it has seen goes
// back into the fog. A reload — a new connection with the session token — gets
// every cell it has been shown in its first MapState (explored, a bit per cell),
// the capital with its owner, before its first snapshot. A newcomer gets nothing
// on joining, and the faction's map with its first spawn in the faction.
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import { ClientMessageSchema, LifeState, ProtocolVersion, ServerMessageSchema, type ServerMessage } from "../src/gen/game/v1/protocol_pb.js";
import { spot } from "./smokeMap.js";
import { uniqueName } from "./uniqueName.js";

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const HOME = spot("explored");
const CELLS = 100 * 100; // config.json's map

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));
let failures = 0;
const check = (name: string, cond: boolean, detail = "") => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}${detail ? `  [${detail}]` : ""}`);
  if (!cond) failures++;
};

// --- A player: one connection, its messages kept ---------------------------------

interface Player {
  messages: ServerMessage[];
  send: (payload: unknown) => void;
  close: () => void;
}
function connect(): Promise<Player> {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(URL);
    ws.binaryType = "arraybuffer";
    const player: Player = {
      messages: [],
      send: (payload) => ws.send(toBinary(ClientMessageSchema, create(ClientMessageSchema, { payload } as never))),
      close: () => ws.close(),
    };
    ws.onmessage = (ev: MessageEvent) => player.messages.push(fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer)));
    ws.onopen = () => resolve(player);
    ws.onerror = () => reject(new Error("websocket error"));
  });
}
async function until<T>(get: () => T | undefined, ms = 5000): Promise<T | undefined> {
  for (const end = Date.now() + ms; Date.now() < end; await sleep(25)) {
    const value = get();
    if (value !== undefined) return value;
  }
  return get();
}
const all = <K extends NonNullable<ServerMessage["payload"]["case"]>>(p: Player, kind: K) =>
  p.messages.filter((m) => m.payload.case === kind).map((m) => m.payload.value as Extract<ServerMessage["payload"], { case: K }>["value"]);
const last = <K extends NonNullable<ServerMessage["payload"]["case"]>>(p: Player, kind: K) => all(p, kind).at(-1);
const hello = (name: string, sessionToken = "") => ({ case: "hello", value: { protocolVersion: ProtocolVersion.CURRENT, name: uniqueName(name), sessionToken } });

// The cells a MapState marks explored: bit i % 8 of byte i / 8.
function explored(bits: Uint8Array): Set<number> {
  const out = new Set<number>();
  for (let i = 0; i < Math.min(CELLS, bits.length * 8); i++) if ((bits[i >> 3]! >> (i & 7)) & 1) out.add(i);
  return out;
}
const missing = (seen: Set<number>, map: Set<number>) => [...seen].filter((c) => !map.has(c));

// --- The run ------------------------------------------------------------------------

async function main(): Promise<void> {
  // 1. The scout looks around its capital, then walks up out of it.
  const scout = await connect();
  scout.send(hello("scout"));
  const welcome = await until(() => last(scout, "welcome"));
  check("joined", welcome !== undefined);
  scout.send({ case: "spawn", value: { factionId: HOME.factionId } });
  await until(() => last(scout, "snapshot")?.you?.life === LifeState.ALIVE || undefined);
  scout.send({ case: "input", value: { frames: [{ inputSeq: 1, moveX: 0, moveY: -1 }] } });
  const me = () => last(scout, "snapshot")?.players.find((p) => p.id === welcome?.playerId);
  await until(() => ((me()?.y ?? Infinity) <= HOME.y - 500 ? true : undefined));
  scout.send({ case: "input", value: { frames: [{ inputSeq: 2, moveX: 0, moveY: 0 }] } });
  await sleep(200);
  const seen = new Set(all(scout, "snapshot").flatMap((s) => s.revealed));
  const behind = new Set(all(scout, "snapshot").flatMap((s) => s.hidden));
  check("it saw around its capital and on the way", seen.has(HOME.cell) && seen.size > 30, `seen=${seen.size}`);
  check("... and part of it is in the fog now", behind.size > 0, `hidden=${behind.size}`);
  scout.close();

  // 2. A reload: the map comes with the greeting, before the first snapshot.
  const reload = await connect();
  reload.send(hello("scout", welcome?.sessionToken ?? ""));
  const again = await until(() => last(reload, "welcome"));
  check("the same player", again?.playerId === welcome?.playerId);
  await until(() => last(reload, "snapshot"));
  const kinds = reload.messages.map((m) => m.payload.case);
  check("its MapState comes before its first snapshot", kinds.indexOf("mapState") >= 0 && kinds.indexOf("mapState") < kinds.indexOf("snapshot"));
  const map = all(reload, "mapState")[0];
  const known = explored(map?.explored ?? new Uint8Array());
  const lost = missing(seen, known);
  check("every cell it has seen is explored", lost.length === 0, `explored=${known.size} missing=${lost.slice(0, 5).join(",")}`);
  check("... the ones in the fog too", [...behind].every((c) => known.has(c)));
  check("... not the whole map", known.size < 400, `explored=${known.size}`);
  check("the capital with its owner", map?.ownerFactionIds[HOME.cell] === HOME.factionId);
  reload.close();

  // 3. A newcomer: nothing on joining; the faction's map with its first spawn.
  const newcomer = await connect();
  newcomer.send(hello("newbie"));
  await until(() => last(newcomer, "mapState"));
  check("a joiner without a faction: nothing explored", (last(newcomer, "mapState")?.explored.length ?? -1) === 0);
  newcomer.send({ case: "spawn", value: { factionId: HOME.factionId } });
  await until(() => (all(newcomer, "mapState").length >= 2 ? true : undefined));
  const theirs = explored(all(newcomer, "mapState")[1]?.explored ?? new Uint8Array());
  check("its first spawn brings the faction's map", missing(seen, theirs).length === 0, `explored=${theirs.size}`);
  newcomer.close();
}

main()
  .catch((e: unknown) => {
    console.error(e);
    failures++;
  })
  .finally(() => {
    console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
    process.exit(failures === 0 ? 0 : 1);
  });
