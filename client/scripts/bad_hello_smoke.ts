// Hello validation e2e: each bad start gets a fatal ServerError, then the socket
// closes with 4000 + code; a refused spawn gets a non-fatal error and the
// connection lives on. Cases:
//   another protocol version -> PROTOCOL_VERSION   (4001), no Welcome
//   a blank name             -> INVALID_NAME       (4007), no Welcome
//   a second Hello           -> UNEXPECTED_MESSAGE (4003)
//   Input before Hello       -> UNEXPECTED_MESSAGE (4003)
//   a spawn off the map      -> SPAWN_INVALID_CELL, not fatal, request_id echoed
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import {
  ClientMessageSchema,
  ErrorCode,
  ProtocolVersion,
  ServerMessageSchema,
  type ClientMessage,
  type ServerError,
} from "../src/gen/game/v1/protocol_pb.js";

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";

const hello = (name: string, protocolVersion: number = ProtocolVersion.CURRENT): ClientMessage =>
  create(ClientMessageSchema, { payload: { case: "hello", value: { protocolVersion, name } } });
const input = (): ClientMessage =>
  create(ClientMessageSchema, { payload: { case: "input", value: { frames: [{ seq: 1 }] } } });
const spawnOffMap = (requestId: number): ClientMessage =>
  create(ClientMessageSchema, { requestId, payload: { case: "spawn", value: { cell: 1_000_000, factionId: 1 } } });

interface Outcome {
  welcome: boolean;
  errors: ServerError[];
  closeCode: number | undefined; // undefined: still open when we stopped watching
}

// Opens a socket, sends `messages` in order (each after the previous reply or
// straight away), and reports what came back within `watchMs`.
function run(messages: ClientMessage[], watchMs = 1500): Promise<Outcome> {
  return new Promise((resolve) => {
    const out: Outcome = { welcome: false, errors: [], closeCode: undefined };
    const ws = new WebSocket(URL);
    ws.binaryType = "arraybuffer";
    ws.onopen = () => {
      for (const m of messages) ws.send(toBinary(ClientMessageSchema, m));
    };
    ws.onmessage = (ev: MessageEvent) => {
      const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
      if (m.payload.case === "welcome") out.welcome = true;
      if (m.payload.case === "error") out.errors.push(m.payload.value);
    };
    ws.onclose = (ev: CloseEvent) => {
      out.closeCode = ev.code;
      resolve(out);
    };
    // A snapshot: our own close completing later must not read as the server's.
    setTimeout(() => {
      resolve({ ...out });
      ws.close();
    }, watchMs);
  });
}

let failures = 0;
const check = (name: string, cond: boolean, got: Outcome) => {
  const errs = got.errors.map((e) => `${ErrorCode[e.code]}${e.fatal ? "!" : ""}#${e.requestId}`).join(",");
  console.log(`${cond ? "ok  " : "FAIL"} ${name}  [welcome=${got.welcome} errors=${errs} close=${got.closeCode}]`);
  if (!cond) failures++;
};
const fatal = (o: Outcome, code: ErrorCode) =>
  o.errors.length === 1 && o.errors[0]!.code === code && o.errors[0]!.fatal && o.closeCode === 4000 + code;

const version = await run([hello("future", 999)]);
check("another protocol version: PROTOCOL_VERSION, closed 4001, no Welcome",
  fatal(version, ErrorCode.PROTOCOL_VERSION) && !version.welcome, version);

const blank = await run([hello("   ")]);
check("a blank name: INVALID_NAME, closed 4007, no Welcome", fatal(blank, ErrorCode.INVALID_NAME) && !blank.welcome, blank);

const twice = await run([hello("twice"), hello("twice")]);
check("a second Hello: UNEXPECTED_MESSAGE, closed 4003", fatal(twice, ErrorCode.UNEXPECTED_MESSAGE) && twice.welcome, twice);

const early = await run([input()]);
check("Input before Hello: UNEXPECTED_MESSAGE, closed 4003", fatal(early, ErrorCode.UNEXPECTED_MESSAGE), early);

const offMap = await run([hello("walker"), spawnOffMap(42)], 1000);
check(
  "a spawn off the map: SPAWN_INVALID_CELL, not fatal, request_id echoed, still open",
  offMap.errors.length === 1 &&
    offMap.errors[0]!.code === ErrorCode.SPAWN_INVALID_CELL &&
    !offMap.errors[0]!.fatal &&
    offMap.errors[0]!.requestId === 42 &&
    offMap.closeCode === undefined,
  offMap,
);

console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
