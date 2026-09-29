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

// A Ping carries the client's clock as uint32 ms (performance.now()); its Pong
// echoes it back. The round trip, across a wrap of that clock too.
export function roundTripMs(echoedMs: number, nowMs: number): number {
  return (Math.floor(nowMs) - echoedMs) >>> 0;
}

// Thin transport: one WebSocket binary frame == one protobuf message.
// Decodes incoming ServerMessages and hands them to `onMessage`; `onClose` gets
// the socket's close code (4000 + ErrorCode when the server closed it for one).
// Pings right after Hello (the first round trip) and then every KEEPALIVE_MS,
// which also keeps the connection alive.
export class GameClient {
  private ws?: WebSocket;
  private inputSeq = 0;
  private keepalive: ReturnType<typeof setInterval> | undefined;

  constructor(
    private readonly url: string,
    private readonly onMessage: (msg: ServerMessage) => void,
    private readonly onClose: (code: number) => void = () => {},
  ) {}

  connect(name: string): void {
    const ws = new WebSocket(this.url);
    ws.binaryType = "arraybuffer";
    this.ws = ws;

    ws.onopen = () => {
      this.sendHello(name);
      this.sendPing();
      this.keepalive = setInterval(() => this.sendPing(), KEEPALIVE_MS);
    };
    ws.onmessage = (ev: MessageEvent) => {
      const bytes = new Uint8Array(ev.data as ArrayBuffer);
      this.onMessage(fromBinary(ServerMessageSchema, bytes));
    };
    ws.onclose = (ev: CloseEvent) => {
      clearInterval(this.keepalive);
      console.log(`[net] connection closed (${ev.code})`);
      this.onClose(ev.code);
    };
    ws.onerror = () => console.error("[net] connection error");
  }

  sendHello(name: string): void {
    this.dispatch(
      create(ClientMessageSchema, {
        payload: { case: "hello", value: { protocolVersion: PROTOCOL_VERSION, name } },
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

  private dispatch(msg: ClientMessage): void {
    this.ws?.send(toBinary(ClientMessageSchema, msg));
  }
}
