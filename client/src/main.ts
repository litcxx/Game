import { Application, Graphics, Text } from "pixi.js";

import { LifeState } from "./gen/game/v1/protocol_pb.js";
import { installInput } from "./input/keyboard.js";
import { GameClient } from "./net/client.js";
import { InterpolationBuffer, type RemoteState } from "./net/interpolation.js";
import { Predictor, type PendingInput } from "./net/prediction.js";
import { Scene } from "./render/scene.js";

const FIXED_DT = 1 / 60;
const MAX_ACCUM = 0.25;
const INTERP_DELAY = 100; // ms: render remote players ~2 snapshots in the past

async function main(): Promise<void> {
  const app = new Application();
  await app.init({
    resizeTo: window,
    background: "#101015",
    antialias: true,
    resolution: window.devicePixelRatio || 1, // crisp on HiDPI (fixes blurry text)
    autoDensity: true,
  });
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
  const fps = new Text({
    text: "FPS —",
    style: { fill: "#8fce8f", fontFamily: "monospace", fontSize: 14 },
  });
  fps.anchor.set(1, 0);
  app.stage.addChild(fps);

  let myId = 0;
  let mapWidth = 0;
  let mapHeight = 0;
  let factionCount = 0;
  let selectedFaction = 1;
  let myFaction = 0;
  let tickRate = 60;
  const factionColors = new Map<number, number>();

  const readKeys = installInput();
  let attacking = false;

  let alive = false;
  let serverTick = 0;
  let respawnTick = 0;
  let predictor: Predictor | undefined;
  const interp = new InterpolationBuffer(INTERP_DELAY);

  let acc = 0;
  let fpsAccum = 0;
  const outbox: PendingInput[] = [];

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
            w.config.tickRate,
          );
        }
        scene.setFactions(w.factions.map((f) => ({ id: f.id, color: f.color })));
        const speed = w.config?.moveSpeed ?? 300;
        predictor = new Predictor(speed, FIXED_DT, {
          maxX: mapWidth * 100 - 1,
          maxY: mapHeight * 100 - 1,
        });
        predictor.reset({ x: (mapWidth * 100) / 2, y: (mapHeight * 100) / 2 }); // centre pre-spawn
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
        const wasAlive = alive;
        alive = life === LifeState.ALIVE;
        respawnTick = s.you?.respawnTick ?? 0;

        const self = s.players.find((p) => p.id === myId);
        if (predictor) {
          if (alive && self) {
            if (wasAlive) predictor.reconcile({ x: self.x, y: self.y }, s.you?.lastInputSeq ?? 0);
            else predictor.reset({ x: self.x, y: self.y }); // just (re)spawned -> snap
          } else if (self) {
            predictor.reset({ x: self.x, y: self.y }); // dead body -> snap, stop predicting
          }
        }
        scene.updateMeta(s.players, s.you?.attackReadyTick ?? 0, serverTick);
        scene.applyCellUpdates(s.cells);
        const remoteStates = new Map<number, RemoteState>();
        for (const p of s.players) if (p.id !== myId) remoteStates.set(p.id, { x: p.x, y: p.y });
        interp.push(performance.now(), remoteStates);

        const mapHint = `M — ${scene.mapMode ? "к игроку" : "вся карта"}`;
        if (alive && self) {
          drawSwatch(myFaction);
          status.text = `id=${myId} · hp=${self.hp} · фракция ${myFaction} · WASD ход · E захват · ЛКМ атака · ${mapHint}`;
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

  window.addEventListener("keydown", (e) => {
    const key = e.key.toLowerCase();
    if (key === "m") {
      scene.toggleMap();
      return;
    }
    const n = Number(e.key);
    if (Number.isInteger(n) && n >= 1 && n <= factionCount) selectedFaction = n;
  });

  // Attack: hold the left mouse button while alive. The fixed-step loop samples
  // `attacking`; a min-hold keeps a quick click alive long enough to be sampled.
  const MIN_ATTACK_HOLD_MS = 60;
  let attackDownAt = 0;
  let releaseTimer: ReturnType<typeof setTimeout> | undefined;
  app.canvas.addEventListener("mousedown", (e) => {
    if (e.button !== 0 || !alive) return;
    if (releaseTimer !== undefined) {
      clearTimeout(releaseTimer);
      releaseTimer = undefined;
    }
    attackDownAt = performance.now();
    attacking = true;
  });
  window.addEventListener("mouseup", (e) => {
    if (e.button !== 0 || !attacking || releaseTimer !== undefined) return;
    const held = performance.now() - attackDownAt;
    const release = () => {
      releaseTimer = undefined;
      attacking = false;
    };
    if (held >= MIN_ATTACK_HOLD_MS) release();
    else releaseTimer = setTimeout(release, MIN_ATTACK_HOLD_MS - held);
  });

  // Click a cell to spawn / respawn — only when not alive (alive clicks attack).
  app.canvas.addEventListener("click", (e) => {
    if (alive || respawnTick > serverTick) return; // still fighting, or waiting out respawn
    const rect = app.canvas.getBoundingClientRect();
    const [col, row] = scene.screenToCell(e.clientX - rect.left, e.clientY - rect.top);
    if (col < 0 || row < 0 || col >= mapWidth || row >= mapHeight) return;
    myFaction = selectedFaction;
    scene.setSelfFaction(myFaction);
    predictor?.reset({ x: col * 100 + 50, y: row * 100 + 50 }); // snap to the chosen cell centre
    client.sendSpawn(row * mapWidth + col, selectedFaction);
  });

  // Fixed-step input + local prediction, then render every frame.
  app.ticker.add((ticker) => {
    fpsAccum += ticker.deltaMS;
    if (fpsAccum >= 250) {
      fpsAccum = 0;
      fps.text = `FPS ${Math.round(ticker.FPS)}`;
    }
    fps.position.set(app.screen.width - 12, 12);

    if (predictor) {
      acc = Math.min(acc + ticker.deltaMS / 1000, MAX_ACCUM);
      while (acc >= FIXED_DT) {
        acc -= FIXED_DT;
        const k = readKeys();
        outbox.push(
          predictor.step({
            moveX: alive ? k.moveX : 0,
            moveY: alive ? k.moveY : 0,
            capturing: k.capturing,
            attack: alive && attacking,
          }),
        );
      }
      while (outbox.length > 0) client.sendInputFrames(outbox.splice(0, 8)); // protocol caps at 8
      predictor.decayError(ticker.deltaMS / 1000);
      scene.setSelfPredicted(predictor.renderPosition.x, predictor.renderPosition.y);
    }
    const now = performance.now();
    const remotes = new Map<number, { x: number; y: number }>();
    for (const id of scene.remoteIds()) {
      const r = interp.sample(id, now);
      if (r) remotes.set(id, r);
    }
    scene.setRemotePositions(remotes);
    scene.frame();
  });
}

void main();
