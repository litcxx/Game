// Connection timeouts e2e (config: handshake 5 s, idle 20 s). Four connections
// at once, watched for 23 s:
//   one that never says Hello    -> HANDSHAKE_TIMEOUT, closed 4004 (~5 s)
//   one that goes quiet after it -> IDLE_TIMEOUT, closed 4005 (~20 s)
//   one that pings every 2 s     -> still open at the end
//   the game's own GameClient, joined but sending no input (a hidden tab) ->
//     still open: its keepalive pings
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import {
  ClientMessageSchema,
  ErrorCode,
  ProtocolVersion,
  ServerMessageSchema,
  type ClientMessage,
} from "../src/gen/game/v1/protocol_pb.js";
import { GameClient } from "../src/net/client.js";

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const WATCH_MS = Number(process.env.WATCH_MS ?? 23000); // idle timeout + a margin

const hello = (name: string): ClientMessage =>
  create(ClientMessageSchema, {
    payload: { case: "hello", value: { protocolVersion: ProtocolVersion.CURRENT, name } },
  });
const ping = (): ClientMessage =>
  create(ClientMessageSchema, { payload: { case: "ping", value: { clientTimeMs: 1 } } });

interface Outcome {
  error: ErrorCode | undefined; // the fatal error it was sent, if any
  closeCode: number | undefined; // undefined: still open at the end
  closedAfterMs: number;
}

// Opens a socket, sends Hello if `name` is given, pings every `pingMs` if set.
function watch(name: string | undefined, pingMs?: number): Promise<Outcome> {
  return new Promise((resolve) => {
    const out: Outcome = { error: undefined, closeCode: undefined, closedAfterMs: 0 };
    const started = performance.now();
    const ws = new WebSocket(URL);
    ws.binaryType = "arraybuffer";
    let timer: ReturnType<typeof setInterval> | undefined;
    const send = (m: ClientMessage) => ws.send(toBinary(ClientMessageSchema, m));
    ws.onopen = () => {
      if (name !== undefined) send(hello(name));
      if (pingMs !== undefined) timer = setInterval(() => send(ping()), pingMs);
    };
    ws.onmessage = (ev: MessageEvent) => {
      const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
      if (m.payload.case === "error" && m.payload.value.fatal) out.error = m.payload.value.code;
    };
    ws.onclose = (ev: CloseEvent) => {
      clearInterval(timer);
      out.closeCode = ev.code;
      out.closedAfterMs = Math.round(performance.now() - started);
      resolve(out);
    };
    setTimeout(() => {
      clearInterval(timer);
      ws.close();
      resolve(out);
    }, WATCH_MS);
  });
}

// The real client: connects, says Hello, and — like a hidden tab — sends no input.
function watchGameClient(): Promise<Outcome & { pongs: number }> {
  return new Promise((resolve) => {
    const out: Outcome & { pongs: number } = { error: undefined, closeCode: undefined, closedAfterMs: 0, pongs: 0 };
    const started = performance.now();
    const game = new GameClient(
      URL,
      (m) => {
        if (m.payload.case === "pong") out.pongs++;
        if (m.payload.case === "error" && m.payload.value.fatal) out.error = m.payload.value.code;
      },
      (code) => {
        out.closeCode = code;
        out.closedAfterMs = Math.round(performance.now() - started);
        resolve(out);
      },
    );
    game.connect("hidden-tab");
    setTimeout(() => resolve(out), WATCH_MS);
  });
}

const [silent, quiet, pinger, client] = await Promise.all([
  watch(undefined),
  watch("quiet"),
  watch("pinger", 2000),
  watchGameClient(),
]);
const show = (o: Outcome) =>
  `error=${o.error === undefined ? "-" : ErrorCode[o.error]} close=${o.closeCode ?? "open"} after=${o.closedAfterMs}ms`;
console.log(`[idle] no Hello: ${show(silent)}`);
console.log(`[idle] quiet:    ${show(quiet)}`);
console.log(`[idle] pinger:   ${show(pinger)}`);
console.log(`[idle] client:   ${show(client)} pongs=${client.pongs}`);

const pass =
  silent.error === ErrorCode.HANDSHAKE_TIMEOUT &&
  silent.closeCode === 4004 &&
  quiet.error === ErrorCode.IDLE_TIMEOUT &&
  quiet.closeCode === 4005 &&
  pinger.closeCode === undefined &&
  client.closeCode === undefined &&
  client.pongs >= 5;
console.log("VERDICT:", pass ? "PASS" : "FAIL");
process.exit(pass ? 0 : 1); // (exits with the GameClient's socket still open)
