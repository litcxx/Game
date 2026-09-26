import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import {
  ClientMessageSchema,
  ServerMessageSchema,
  type ClientMessage,
  type ServerMessage,
} from "../gen/game/v1/protocol_pb.js";

export const PROTOCOL_VERSION = 1;

// Thin transport: one WebSocket binary frame == one protobuf message.
// Decodes incoming ServerMessages and hands them to `onMessage`.
export class GameClient {
  private ws?: WebSocket;
  private inputSeq = 0;

  constructor(
    private readonly url: string,
    private readonly onMessage: (msg: ServerMessage) => void,
  ) {}

  connect(name: string): void {
    const ws = new WebSocket(this.url);
    ws.binaryType = "arraybuffer";
    this.ws = ws;

    ws.onopen = () => this.sendHello(name);
    ws.onmessage = (ev: MessageEvent) => {
      const bytes = new Uint8Array(ev.data as ArrayBuffer);
      this.onMessage(fromBinary(ServerMessageSchema, bytes));
    };
    ws.onclose = () => console.log("[net] connection closed");
    ws.onerror = () => console.error("[net] connection error");
  }

  sendHello(name: string): void {
    this.dispatch(
      create(ClientMessageSchema, {
        payload: { case: "hello", value: { protocolVersion: PROTOCOL_VERSION, name } },
      }),
    );
  }

  sendSpawn(cell: number, factionId: number): void {
    this.dispatch(
      create(ClientMessageSchema, { payload: { case: "spawn", value: { cell, factionId } } }),
    );
  }

  sendInput(moveX: number, moveY: number): void {
    this.dispatch(
      create(ClientMessageSchema, {
        payload: { case: "input", value: { frames: [{ seq: ++this.inputSeq, moveX, moveY }] } },
      }),
    );
  }

  private dispatch(msg: ClientMessage): void {
    this.ws?.send(toBinary(ClientMessageSchema, msg));
  }
}
