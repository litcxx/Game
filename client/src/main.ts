import { Application, Text } from "pixi.js";

import { GameClient } from "./net/client.js";

async function main(): Promise<void> {
  const app = new Application();
  await app.init({ resizeTo: window, background: "#101015", antialias: true });
  document.getElementById("app")!.appendChild(app.canvas);

  const status = new Text({
    text: "connecting…",
    style: { fill: "#e6e6e6", fontFamily: "monospace", fontSize: 18 },
  });
  status.position.set(16, 16);
  app.stage.addChild(status);

  const client = new GameClient("ws://localhost:27998/", (msg) => {
    switch (msg.payload.case) {
      case "welcome": {
        const w = msg.payload.value;
        status.text =
          `connected · player_id=${w.playerId} · tick=${w.serverTick} · ` +
          `map=${w.config?.mapWidth}x${w.config?.mapHeight} · factions=${w.factions.length}`;
        console.log("[welcome]", { playerId: w.playerId, factions: w.factions });
        break;
      }
      case "mapState":
        console.log("[mapState] owners bytes:", msg.payload.value.owners.length);
        break;
      case "roster":
        console.log(
          "[roster] upsert:",
          msg.payload.value.upsert.map((p) => `${p.id}:${p.name}`),
          "removed:",
          msg.payload.value.removed,
        );
        break;
      default:
        console.log("[server message]", msg.payload.case);
    }
  });

  client.connect("alice");
}

void main();
