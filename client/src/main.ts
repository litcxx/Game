import { Application, Text } from "pixi.js";

import { installMovementKeys } from "./input/keyboard.js";
import { GameClient } from "./net/client.js";
import { Scene } from "./render/scene.js";

async function main(): Promise<void> {
  const app = new Application();
  await app.init({ resizeTo: window, background: "#101015", antialias: true });
  document.getElementById("app")!.appendChild(app.canvas);

  const scene = new Scene(app);
  const status = new Text({
    text: "connecting…",
    style: { fill: "#e6e6e6", fontFamily: "monospace", fontSize: 16 },
  });
  status.position.set(12, 12);
  app.stage.addChild(status);

  let myId = 0;
  let mapWidth = 0;
  let mapHeight = 0;
  let factionCount = 0;
  let selectedFaction = 1;

  const client = new GameClient("ws://localhost:27998/", (msg) => {
    switch (msg.payload.case) {
      case "welcome": {
        const w = msg.payload.value;
        myId = w.playerId;
        mapWidth = w.config?.mapWidth ?? 0;
        mapHeight = w.config?.mapHeight ?? 0;
        factionCount = w.factions.length;
        selectedFaction = w.factions[0]?.id ?? 1;
        scene.setSelf(myId);
        if (w.config) scene.setConfig(w.config.mapWidth, w.config.mapHeight);
        scene.setFactions(w.factions.map((f) => ({ id: f.id, color: f.color })));
        break;
      }
      case "roster":
        scene.upsertRoster(msg.payload.value.upsert);
        scene.removeFromRoster(msg.payload.value.removed);
        break;
      case "snapshot": {
        const s = msg.payload.value;
        scene.applySnapshot(s.players);
        const me = s.players.find((p) => p.id === myId);
        status.text = me
          ? `id=${myId} · hp=${me.hp} · pos=(${me.x},${me.y}) · WASD to move`
          : `id=${myId} · not spawned · keys 1-${factionCount} pick faction (${selectedFaction}), click a cell to spawn`;
        break;
      }
      default:
        break;
    }
  });

  client.connect("player");

  // Faction selection (keys 1..N map to faction ids 1..N in config).
  window.addEventListener("keydown", (e) => {
    const n = Number(e.key);
    if (Number.isInteger(n) && n >= 1 && n <= factionCount) selectedFaction = n;
  });

  // Click a cell to spawn there with the selected faction.
  app.canvas.addEventListener("click", (e) => {
    const rect = app.canvas.getBoundingClientRect();
    const [col, row] = scene.screenToCell(e.clientX - rect.left, e.clientY - rect.top);
    if (col < 0 || row < 0 || col >= mapWidth || row >= mapHeight) return;
    client.sendSpawn(row * mapWidth + col, selectedFaction);
  });

  // Movement.
  installMovementKeys((moveX, moveY) => client.sendInput(moveX, moveY));
}

void main();
