// The world outlives a restart (GAME-019). Unlike the other smokes this one runs
// a server of its own — the shared one must not restart under them: the smoke
// map's rules on port 27999, saving to a temp dir. A player of the
// "persistence" faction takes the cell next to its capital; SIGTERM saves the
// world (exit 0). Restarted, the server knows the player by its session token —
// the same player_id, its faction still locked (another is refused), its cells
// still its own, and the map its faction had explored (GAME-020) comes with the
// greeting. Then the newest save is damaged: the server refuses to start on it,
// and starts a new world with --fresh.
//   SERVER_BIN: the server binary (default ../server/build/bin/server)
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";
import { spawn, type ChildProcess } from "node:child_process";
import { mkdtempSync, openSync, readdirSync, rmSync, statSync, truncateSync, writeFileSync, readFileSync } from "node:fs";
import { createConnection } from "node:net";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

import { ClientMessageSchema, ErrorCode, LifeState, ProtocolVersion, ServerMessageSchema, type ServerMessage } from "../src/gen/game/v1/protocol_pb.js";
import { spot } from "./smokeMap.js";
import { uniqueName } from "./uniqueName.js";

const PORT = 27999;
const BIN = process.env.SERVER_BIN ?? fileURLToPath(new URL("../../server/build/bin/server", import.meta.url));
const HOME = spot("persistence");
const NEAR = HOME.cell + 1; // the neutral cell to its right
const OTHER_FACTION = spot("capture").factionId;

const dir = mkdtempSync(join(tmpdir(), "lit-persistence-"));
const saves = join(dir, "saves");
const configPath = join(dir, "config.json");
const logPath = join(dir, "server.log");
{
  const read = (path: string) => JSON.parse(readFileSync(new URL(path, import.meta.url), "utf8"));
  const config = read("../../server/config/config.json");
  const map = read("../../server/config/smoke-map.json");
  config.server.ip = "127.0.0.1";
  config.server.port = PORT;
  config.game.factions = map.factions;
  config.game.capitals = map.capitals;
  config.save = { dir: saves, interval_s: 60, keep: 5 };
  writeFileSync(configPath, JSON.stringify(config));
}

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));
let failures = 0;
const check = (name: string, cond: boolean, detail = "") => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}${detail ? `  [${detail}]` : ""}`);
  if (!cond) failures++;
};

// --- The server: start, wait for its port, stop ------------------------------------------

function start(...args: string[]): ChildProcess {
  const log = openSync(logPath, "a");
  return spawn(BIN, [configPath, ...args], { stdio: ["ignore", log, log] });
}
const listening = () =>
  new Promise<boolean>((resolve) => {
    const socket = createConnection({ host: "127.0.0.1", port: PORT });
    socket.once("connect", () => (socket.destroy(), resolve(true)));
    socket.once("error", () => resolve(false));
  });
async function up(server: ChildProcess, ms = 15000): Promise<boolean> {
  for (const end = Date.now() + ms; Date.now() < end; await sleep(100)) {
    if (server.exitCode !== null) return false;
    if (await listening()) return true;
  }
  return false;
}
const exited = (server: ChildProcess, ms = 15000) =>
  new Promise<number | null>((resolve) => {
    if (server.exitCode !== null) return resolve(server.exitCode);
    const timer = setTimeout(() => resolve(null), ms);
    server.once("exit", (code) => (clearTimeout(timer), resolve(code)));
  });
async function stop(server: ChildProcess): Promise<number | null> {
  server.kill("SIGTERM");
  return exited(server);
}

// --- A player: one connection, its messages kept ----------------------------------------

interface Player {
  messages: ServerMessage[];
  send: (payload: unknown) => void;
  close: () => void;
}
function connect(): Promise<Player> {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(`ws://127.0.0.1:${PORT}/`);
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
const myCells = (p: Player) => last(p, "factionScores")?.scores.find((s) => s.factionId === HOME.factionId)?.cells;

// --- The run --------------------------------------------------------------------------------

async function main(): Promise<void> {
  // 1. A season under way: the player locks its faction and takes the cell next to its capital.
  let server = start("--fresh");
  check("the server is up", await up(server));
  const bystander = await connect(); // joins first: the player below is not id 1 — a new world's first
  bystander.send({ case: "hello", value: { protocolVersion: ProtocolVersion.CURRENT, name: uniqueName("bystand") } });
  await until(() => last(bystander, "welcome"));
  const first = await connect();
  first.send({ case: "hello", value: { protocolVersion: ProtocolVersion.CURRENT, name: uniqueName("persist") } });
  const welcome = await until(() => last(first, "welcome"));
  check("joined", welcome !== undefined);
  first.send({ case: "spawn", value: { factionId: HOME.factionId } });
  let inputSeq = 0;
  await until(() => last(first, "snapshot")?.you?.life === LifeState.ALIVE || undefined);
  first.send({ case: "input", value: { frames: [{ inputSeq: ++inputSeq, moveX: 1, moveY: 0 }] } });
  await until(() => ((last(first, "snapshot")?.players.find((p) => p.id === welcome?.playerId)?.x ?? 0) >= HOME.x + 70 || undefined));
  first.send({ case: "input", value: { frames: [{ inputSeq: ++inputSeq, moveX: 0, moveY: 0, capturing: true }] } });
  const taken = await until(() => last(first, "snapshot")?.cells.some((c) => c.index === NEAR && c.ownerFactionId === HOME.factionId) || undefined);
  check("the cell next to the capital is taken", taken === true);
  const seen = new Set(all(first, "snapshot").flatMap((s) => s.revealed)); // the faction's map
  first.close();
  bystander.close();

  // 2. SIGTERM: the world is saved as the server stops.
  check("SIGTERM: a clean exit", (await stop(server)) === 0);
  const files = () => (statSync(saves, { throwIfNoEntry: false }) ? readdirSync(saves).filter((f) => f.endsWith(".save")).sort() : []);
  check("a save is written", files().length >= 1, files().join(","));

  // 3. Restarted: the same player by its token, its faction locked, its cells its own.
  server = start();
  check("restarted on the save", await up(server));
  const back = await connect();
  back.send({ case: "hello", value: { protocolVersion: ProtocolVersion.CURRENT, name: uniqueName("again"), sessionToken: welcome?.sessionToken ?? "" } });
  const again = await until(() => last(back, "welcome"));
  check("the same player_id", again?.playerId === welcome?.playerId, `${welcome?.playerId} -> ${again?.playerId}`);
  const newcomer = await connect();
  newcomer.send({ case: "hello", value: { protocolVersion: ProtocolVersion.CURRENT, name: uniqueName("newbie") } });
  const newId = (await until(() => last(newcomer, "welcome")))?.playerId ?? 0;
  check("a newcomer gets a new id: none is reused", newId > (welcome?.playerId ?? Infinity), `id ${newId}`);
  newcomer.close();
  const mine = await until(() => all(back, "roster").find((r) => r.full)?.upsert.find((p) => p.id === welcome?.playerId));
  check("its faction is still locked", mine?.factionId === HOME.factionId);
  const cells = await until(() => myCells(back));
  check("its faction's cells: the capital and the one taken", cells === 2, `cells=${cells}`);
  const map = all(back, "mapState")[0];
  const bits = map?.explored ?? new Uint8Array();
  const known = (i: number) => ((bits[i >> 3] ?? 0) >> (i & 7)) & 1;
  const lost = [...seen].filter((i) => !known(i));
  check("the map its faction had explored", seen.size > 0 && lost.length === 0, `seen=${seen.size} missing=${lost.length}`);
  check("... the taken cell as last seen: its own", map?.ownerFactionIds[NEAR] === HOME.factionId);
  back.send({ case: "spawn", value: { factionId: OTHER_FACTION } });
  const refused = await until(() => last(back, "error"));
  check("another faction is refused", refused?.code === ErrorCode.INVALID_FACTION);
  back.send({ case: "spawn", value: { factionId: HOME.factionId } });
  check("its own faction spawns", (await until(() => last(back, "snapshot")?.you?.life === LifeState.ALIVE || undefined)) === true);
  back.close();
  check("SIGTERM again: a clean exit", (await stop(server)) === 0);

  // 4. A damaged newest save: no start on it; --fresh starts a new world.
  const newest = join(saves, files().at(-1) ?? "none");
  truncateSync(newest, statSync(newest).size - 4);
  server = start();
  const code = await exited(server);
  check("a damaged save: the server refuses to start", code !== null && code !== 0, `exit ${code}`);
  server = start("--fresh");
  check("--fresh: a new world", await up(server));
  check("... and a clean exit", (await stop(server)) === 0);
}

main()
  .catch((e: unknown) => {
    console.error(e);
    failures++;
  })
  .finally(() => {
    if (failures > 0) console.log(`--- server log (${logPath}) ---\n${readFileSync(logPath, "utf8")}`);
    else rmSync(dir, { recursive: true, force: true });
    console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
    process.exit(failures === 0 ? 0 : 1);
  });
