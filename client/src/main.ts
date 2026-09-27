import { Application, Graphics, Text } from "pixi.js";

import { LifeState } from "./gen/game/v1/protocol_pb.js";
import { installInput } from "./input/keyboard.js";
import { GameClient } from "./net/client.js";
import { Scene } from "./render/scene.js";

async function main(): Promise<void> {
  const app = new Application();
  await app.init({ resizeTo: window, background: "#101015", antialias: true });
  document.getElementById("app")!.appendChild(app.canvas);

  const scene = new Scene(app);

  // HUD: a faction-colour swatch + a status line.
  const swatch = new Graphics();
  swatch.position.set(12, 14);
  app.stage.addChild(swatch);
  const status = new Text({
    text: "connecting…",
    style: { fill: "#e6e6e6", fontFamily: "monospace", fontSize: 16 },
  });
  status.position.set(34, 12);
  app.stage.addChild(status);

  // FPS counter (top-right), refreshed a few times a second.
  const fps = new Text({
    text: "FPS —",
    style: { fill: "#8fce8f", fontFamily: "monospace", fontSize: 14 },
  });
  fps.anchor.set(1, 0);
  app.stage.addChild(fps);
  let fpsAccum = 0;
  app.ticker.add((ticker) => {
    fps.position.set(app.screen.width - 12, 12);
    fpsAccum += ticker.deltaMS;
    if (fpsAccum >= 250) {
      fpsAccum = 0;
      fps.text = `FPS ${Math.round(ticker.FPS)}`;
    }
  });

  let myId = 0;
  let mapWidth = 0;
  let mapHeight = 0;
  let factionCount = 0;
  let selectedFaction = 1;
  let myFaction = 0;
  let tickRate = 60;
  const factionColors = new Map<number, number>();

  // Local input intent; the server holds it until the next Input arrives.
  const move = { x: 0, y: 0, capturing: false };
  let attacking = false;

  // Latest self state, refreshed from each snapshot.
  let alive = false;
  let serverTick = 0;
  let respawnTick = 0;

  const drawSwatch = (factionId: number): void => {
    const color = factionColors.get(factionId);
    swatch.clear();
    if (color !== undefined) {
      swatch.roundRect(0, 0, 14, 14, 3).fill(color).stroke({ width: 1, color: 0xffffff, alpha: 0.5 });
    }
  };

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
        for (const f of w.factions) factionColors.set(f.id, f.color);
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

        const mapHint = `M — ${scene.mapMode ? "к игроку" : "вся карта"}`;
        const me = s.players.find((p) => p.id === myId);
        if (alive && me) {
          drawSwatch(myFaction);
          status.text = `id=${myId} · hp=${me.hp} · фракция ${myFaction} · WASD ход · E захват · ЛКМ атака · ${mapHint}`;
        } else if (life === LifeState.DEAD) {
          drawSwatch(myFaction);
          const left = Math.max(0, Math.ceil((respawnTick - serverTick) / tickRate));
          status.text =
            left > 0
              ? `id=${myId} · DEAD · respawn in ${left}s · ${mapHint}`
              : `id=${myId} · DEAD · click a cell to respawn (фракция ${selectedFaction}) · ${mapHint}`;
        } else {
          drawSwatch(selectedFaction);
          status.text = `id=${myId} · not spawned · 1-${factionCount} фракция (${selectedFaction}), клик — спавн · ${mapHint}`;
        }
        break;
      }
      default:
        break;
    }
  });

  client.connect("player");

  const pushInput = () => client.sendInput(move.x, move.y, move.capturing, attacking);

  window.addEventListener("keydown", (e) => {
    const key = e.key.toLowerCase();
    if (key === "m") {
      scene.toggleMap();
      return;
    }
    // Faction selection (keys 1..N map to faction ids 1..N in config).
    const n = Number(e.key);
    if (Number.isInteger(n) && n >= 1 && n <= factionCount) selectedFaction = n;
  });

  // Attack: hold the left mouse button while alive (area hit around you, the
  // server fires each cooldown). A quick click still lands one swing — we hold
  // attack=true for a minimum window so mousedown+mouseup can't collapse into a
  // single server tick and cancel out.
  const MIN_ATTACK_HOLD_MS = 60;
  let attackDownAt = 0;
  let releaseTimer: ReturnType<typeof setTimeout> | undefined;
  const releaseAttack = () => {
    releaseTimer = undefined;
    attacking = false;
    pushInput();
  };
  app.canvas.addEventListener("mousedown", (e) => {
    if (e.button !== 0 || !alive) return;
    if (releaseTimer !== undefined) {
      clearTimeout(releaseTimer);
      releaseTimer = undefined;
    }
    attackDownAt = performance.now();
    attacking = true;
    pushInput();
  });
  window.addEventListener("mouseup", (e) => {
    if (e.button !== 0 || !attacking || releaseTimer !== undefined) return;
    const held = performance.now() - attackDownAt;
    if (held >= MIN_ATTACK_HOLD_MS) releaseAttack();
    else releaseTimer = setTimeout(releaseAttack, MIN_ATTACK_HOLD_MS - held);
  });

  // Click a cell to spawn / respawn — only when not alive (alive clicks attack).
  app.canvas.addEventListener("click", (e) => {
    if (alive || respawnTick > serverTick) return; // still fighting, or waiting out respawn
    const rect = app.canvas.getBoundingClientRect();
    const [col, row] = scene.screenToCell(e.clientX - rect.left, e.clientY - rect.top);
    if (col < 0 || row < 0 || col >= mapWidth || row >= mapHeight) return;
    myFaction = selectedFaction;
    scene.setSelfFaction(myFaction);
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
