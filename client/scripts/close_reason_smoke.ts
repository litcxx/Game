// Close reason e2e: a client that closes its own connection says why — a code and
// a reason — and the server's log keeps them, so a playtest's log tells a client
// that heard nothing from the server (4900) from a closed tab. The player joins,
// closes with 4900 and a reason made for this run, and the line must show up in
// the server's log: SERVER_LOG, by default ../server/server.log (where CI writes
// it; run from client/).
import { existsSync, readFileSync } from "node:fs";

import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import { ClientMessageSchema, ProtocolVersion, ServerMessageSchema } from "../src/gen/game/v1/protocol_pb.js";
import { STALE_CLOSE_CODE } from "../src/net/client.js";
import { uniqueName } from "./uniqueName.js";

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const LOG = process.env.SERVER_LOG ?? "../server/server.log";
const TIMEOUT_MS = 5000;

const finish = (pass: boolean, detail: string) => {
  console.log(`[close] ${detail}`);
  console.log("VERDICT:", pass ? "PASS" : "FAIL");
  process.exit(pass ? 0 : 1);
};

if (!existsSync(LOG)) finish(false, `no server log at ${LOG}: set SERVER_LOG to the running server's log`);

const reason = `smoke ${uniqueName("close")}`;
const expected = `closed: ${STALE_CLOSE_CODE} ${reason}`;
const ws = new WebSocket(URL);
ws.binaryType = "arraybuffer";
ws.onopen = () =>
  ws.send(
    toBinary(
      ClientMessageSchema,
      create(ClientMessageSchema, {
        payload: { case: "hello", value: { protocolVersion: ProtocolVersion.CURRENT, name: uniqueName("closer") } },
      }),
    ),
  );
ws.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") ws.close(STALE_CLOSE_CODE, reason);
};
ws.onerror = () => finish(false, "socket error");

const started = Date.now();
const poll = setInterval(() => {
  if (readFileSync(LOG, "utf8").includes(expected)) {
    clearInterval(poll);
    finish(true, `the server logged "${expected}"`);
  } else if (Date.now() - started > TIMEOUT_MS) {
    clearInterval(poll);
    finish(false, `no "${expected}" in ${LOG} after ${TIMEOUT_MS} ms`);
  }
}, 100);
