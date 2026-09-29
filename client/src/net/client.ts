import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import {
  ClientMessageSchema,
  ProtocolVersion,
  ServerMessageSchema,
  type ClientMessage,
  type ServerMessage,
} from "../gen/game/v1/protocol_pb.js";

// From the protocol itself, so a stale build is told PROTOCOL_VERSION by the server.
export const PROTOCOL_VERSION: number = ProtocolVersion.CURRENT;

// The server closes a connection it has not heard from for its idle timeout
// (20 s). A hidden tab stops the game loop and with it the input frames, so a
// Ping goes out this often regardless while the socket is open.
export const KEEPALIVE_MS = 2000;

// The server talks many times a second (snapshots at 20 Hz, Pongs). Silence
// this long means the connection is gone even if the socket has not noticed —
// a dropped Wi-Fi often closes nothing for minutes. It counts from connect(),
// so a connection that never opens (made while the network was down) ends too.
export const STALE_MS = 6000;
// The close code reported for such a connection (never sent on the wire).
export const STALE_CLOSE_CODE = 4900;

// A Ping carries the client's clock as uint32 ms (performance.now()); its Pong
// echoes it back. The round trip, across a wrap of that clock too.
export function roundTripMs(echoedMs: number, nowMs: number): number {
  return (Math.floor(nowMs) - echoedMs) >>> 0;
}

// Thin transport: one WebSocket binary frame == one protobuf message.
// Decodes incoming ServerMessages and hands them to `onMessage`; `onClose` gets
// the socket's close code (4000 + ErrorCode when the server closed it for one,
// STALE_CLOSE_CODE when it went silent). Pings right after Hello (the first round
// trip) and then every KEEPALIVE_MS, which also keeps the connection alive.
// connect() again opens a new socket (a reconnect); what is sent while none is
// open is dropped.
export class GameClient {
  private ws?: WebSocket;
  private inputSeq = 0;
  private keepalive: ReturnType<typeof setInterval> | undefined;
  private lastHeardMs = 0;

  constructor(
    private readonly url: string,
    private readonly onMessage: (msg: ServerMessage) => void,
    private readonly onClose: (code: number) => void = () => {},
  ) {}

  // Opens a socket and says Hello: a new player, or with a session token from
  // an earlier Welcome, the same character back.
  connect(name: string, sessionToken = ""): void {
    this.drop();
    const ws = new WebSocket(this.url);
    ws.binaryType = "arraybuffer";
    this.ws = ws;
    this.lastHeardMs = performance.now();
    this.keepalive = setInterval(() => this.keepAlive(), KEEPALIVE_MS);

    ws.onopen = () => {
      this.sendHello(name, sessionToken);
      this.sendPing();
    };
    ws.onmessage = (ev: MessageEvent) => {
      this.lastHeardMs = performance.now();
      const bytes = new Uint8Array(ev.data as ArrayBuffer);
      this.onMessage(fromBinary(ServerMessageSchema, bytes));
    };
    ws.onclose = (ev: CloseEvent) => {
      this.drop();
      console.log(`[net] connection closed (${ev.code})`);
      this.onClose(ev.code);
    };
    ws.onerror = () => console.error("[net] connection error");
  }

  // Closes the socket; onClose follows, as for any close.
  close(): void {
    this.ws?.close();
  }

  sendHello(name: string, sessionToken = ""): void {
    this.dispatch(
      create(ClientMessageSchema, {
        payload: { case: "hello", value: { protocolVersion: PROTOCOL_VERSION, name, sessionToken } },
      }),
    );
  }

  // client_time_ms comes back in the Pong (RTT); uint32 ms since the page loaded.
  sendPing(): void {
    const clientTimeMs = Math.floor(performance.now()) >>> 0;
    this.dispatch(create(ClientMessageSchema, { payload: { case: "ping", value: { clientTimeMs } } }));
  }

  sendSpawn(cell: number, factionId: number): void {
    this.dispatch(
      create(ClientMessageSchema, { payload: { case: "spawn", value: { cell, factionId } } }),
    );
  }

  sendInput(moveX: number, moveY: number, capturing: boolean, attack = false): void {
    this.dispatch(
      create(ClientMessageSchema, {
        payload: {
          case: "input",
          value: { frames: [{ seq: ++this.inputSeq, moveX, moveY, capturing, attack }] },
        },
      }),
    );
  }

  // Send a batch of already-sequenced input frames (the predictor owns the seqs).
  sendInputFrames(
    frames: {
      seq: number;
      moveX: number;
      moveY: number;
      capturing: boolean;
      attack: boolean;
      ability: number;
      aimX: number;
      aimY: number;
    }[],
  ): void {
    this.dispatch(create(ClientMessageSchema, { payload: { case: "input", value: { frames } } }));
  }

  // Every KEEPALIVE_MS: a Ping (once open), or — after STALE_MS of silence —
  // the end of a connection that is gone without saying so.
  private keepAlive(): void {
    if (performance.now() - this.lastHeardMs < STALE_MS) {
      this.sendPing();
      return;
    }
    console.log(`[net] no word from the server for ${STALE_MS} ms: connection lost`);
    this.drop();
    this.onClose(STALE_CLOSE_CODE);
  }

  // Lets go of the current socket, if any: no more events from it.
  private drop(): void {
    clearInterval(this.keepalive);
    this.keepalive = undefined;
    const ws = this.ws;
    if (!ws) return;
    this.ws = undefined;
    ws.onopen = ws.onmessage = ws.onclose = ws.onerror = null;
    if (ws.readyState === WebSocket.CONNECTING || ws.readyState === WebSocket.OPEN) ws.close();
  }

  private dispatch(msg: ClientMessage): void {
    // A socket still connecting would throw; a closed one drops it anyway.
    if (this.ws?.readyState === WebSocket.OPEN) this.ws.send(toBinary(ClientMessageSchema, msg));
  }
}
