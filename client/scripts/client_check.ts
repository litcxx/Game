// Checks GameClient over a fake WebSocket (no server): the Ping carries the
// client's report for the server's metrics — its last round trip and the
// prediction corrections since the previous Ping. Run:
//   npx tsx scripts/client_check.ts
import { fromBinary } from "@bufbuild/protobuf";

import { ClientMessageSchema, type ClientMessage } from "../src/gen/game/v1/protocol_pb.js";
import { GameClient, type PingReport } from "../src/net/client.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};

// What GameClient uses of a browser WebSocket; the test opens it by hand.
class FakeWebSocket {
  static readonly CONNECTING = 0;
  static readonly OPEN = 1;
  static readonly CLOSING = 2;
  static readonly CLOSED = 3;
  static last: FakeWebSocket | undefined;
  readyState = FakeWebSocket.CONNECTING;
  binaryType = "blob";
  readonly sent: ClientMessage[] = [];
  onopen: (() => void) | null = null;
  onmessage: ((ev: unknown) => void) | null = null;
  onclose: ((ev: unknown) => void) | null = null;
  onerror: (() => void) | null = null;
  constructor(readonly url: string) {
    FakeWebSocket.last = this;
  }
  send(data: Uint8Array): void {
    this.sent.push(fromBinary(ClientMessageSchema, data));
  }
  close(): void {
    this.readyState = FakeWebSocket.CLOSED;
  }
  open(): void {
    this.readyState = FakeWebSocket.OPEN;
    this.onopen?.();
  }
}
(globalThis as { WebSocket: unknown }).WebSocket = FakeWebSocket;

const pingOf = (ws: FakeWebSocket) => {
  const m = ws.sent.find((x) => x.payload.case === "ping");
  return m?.payload.case === "ping" ? m.payload.value : undefined;
};

{
  let taken = 0;
  const report = (): PingReport => {
    taken++;
    return { rttMs: 42, corrections: 3, maxCorrection: 17 };
  };
  const client = new GameClient("ws://game/ws", () => {}, () => {}, report);
  client.connect("Ann");
  const ws = FakeWebSocket.last!;
  ws.open();
  const ping = pingOf(ws);
  check("opened: Hello, then a Ping", ws.sent[0]?.payload.case === "hello" && ping !== undefined);
  check("the Ping carries the report", ping?.rttMs === 42 && ping.corrections === 3 && ping.maxCorrection === 17);
  check("... taken once per Ping", taken === 1);
  check("... next to the client's clock", (ping?.clientTimeMs ?? 0) > 0);
  client.close();
}
{
  const client = new GameClient("ws://game/ws", () => {}, () => {});
  client.connect("Bob");
  const ws = FakeWebSocket.last!;
  ws.open();
  const ping = pingOf(ws);
  check("no report to give: zeros", ping?.rttMs === 0 && ping.corrections === 0 && ping.maxCorrection === 0);
  client.close();
}

console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
