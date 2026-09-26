import { Application, Text } from "pixi.js";

import { LifeState } from "./gen/game/v1/protocol_pb.js";
import { installInput } from "./input/keyboard.js";
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
  let tickRate = 60;

  // Local input intent; the server holds it until the next Input arrives.
  const move = { x: 0, y: 0, capturing: false };
  let attacking = false;

  // Latest self state, refreshed from each snapshot.
  let alive = false;
  let serverTick = 0;
  let respawnTick = 0;

  const client = new GameClient("ws://localhost:27998/", (msg) => {
    switch (msg.payload.case) {
      case "welcome": {
        const w = msg.payload.value;
        myId = w.playerId;
        mapWidth = w.config?.mapWidth ?? 0;
        mapHeight = w.config?.mapHeight ?? 0;
        tickRate = w.config?.tickRate || 60;
        factionCount = w.factions.length;
        selectedFaction = w.factions[0]?.id ?? 1;
        scene.setSelf(myId);
        if (w.config) {
          scene.setConfig(
            w.config.mapWidth,
            w.config.mapHeight,
            w.config.maxHp,
            w.config.attackRange,
            w.config.attackCooldownTicks,
          );
        }
        scene.setFactions(w.factions.map((f) => ({ id: f.id, color: f.color })));
        break;
      }
      case "mapState":
        scene.setMapState(msg.payload.value.owners, msg.payload.value.captures);
        break;
      case "roster":
        scene.upsertRoster(msg.payload.value.upsert);
        scene.removeFromRoster(msg.payload.value.removed);
        break;
      case "snapshot": {
        const s = msg.payload.value;
        serverTick = s.tick;
        const life = s.you?.life ?? LifeState.NOT_SPAWNED;
        alive = life === LifeState.ALIVE;
        respawnTick = s.you?.respawnTick ?? 0;
        scene.applySnapshot(s.players, s.you?.attackReadyTick ?? 0, serverTick);
        scene.applyCellUpdates(s.cells);

        const me = s.players.find((p) => p.id === myId);
        if (alive && me) {
          status.text = `id=${myId} · hp=${me.hp} · pos=(${me.x},${me.y}) · WASD move · hold E capture · hold LMB to attack`;
        } else if (life === LifeState.DEAD) {
          const left = Math.max(0, Math.ceil((respawnTick - serverTick) / tickRate));
          status.text =
            left > 0
              ? `id=${myId} · DEAD · respawn in ${left}s`
              : `id=${myId} · DEAD · click a cell to respawn (faction ${selectedFaction})`;
        } else {
          status.text = `id=${myId} · not spawned · keys 1-${factionCount} pick faction (${selectedFaction}), click a cell to spawn`;
        }
        break;
      }
      default:
        break;
    }
  });

  client.connect("player");

  const pushInput = () => client.sendInput(move.x, move.y, move.capturing, attacking);

  // Faction selection (keys 1..N map to faction ids 1..N in config).
  window.addEventListener("keydown", (e) => {
    const n = Number(e.key);
    if (Number.isInteger(n) && n >= 1 && n <= factionCount) selectedFaction = n;
  });

  // Attack: hold the left mouse button while alive (area hit around you, the
  // server fires each cooldown). Held state is released on mouseup anywhere.
  app.canvas.addEventListener("mousedown", (e) => {
    if (e.button !== 0 || !alive) return;
    attacking = true;
    pushInput();
  });
  window.addEventListener("mouseup", (e) => {
    if (e.button !== 0 || !attacking) return;
    attacking = false;
    pushInput();
  });

  // Click a cell to spawn / respawn — only when not alive (alive clicks attack).
  app.canvas.addEventListener("click", (e) => {
    if (alive || respawnTick > serverTick) return; // still fighting, or waiting out respawn
    const rect = app.canvas.getBoundingClientRect();
    const [col, row] = scene.screenToCell(e.clientX - rect.left, e.clientY - rect.top);
    if (col < 0 || row < 0 || col >= mapWidth || row >= mapHeight) return;
    client.sendSpawn(row * mapWidth + col, selectedFaction);
  });

  // Movement + capture ('e'); merged with the current attack intent.
  installInput((moveX, moveY, capturing) => {
    move.x = moveX;
    move.y = moveY;
    move.capturing = capturing;
    pushInput();
  });
}

void main();
