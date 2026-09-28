import { Application } from "pixi.js";

import { abilitiesFromWelcome, aimVector, selectSlot, type AbilityInfo } from "./abilities.js";
import { effectsFromEvents } from "./effects.js";
import { LifeState } from "./gen/game/v1/protocol_pb.js";
import { installInput } from "./input/keyboard.js";
import { installMouse } from "./input/mouse.js";
import { GameClient } from "./net/client.js";
import { InterpolationBuffer, type RemoteState } from "./net/interpolation.js";
import { Predictor, type PendingInput } from "./net/prediction.js";
import { AbilityBar } from "./render/abilityBar.js";
import { FactionPicker } from "./render/factionPicker.js";
import { Hud } from "./render/hud.js";
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
    resolution: window.devicePixelRatio || 1, // crisp on HiDPI
    autoDensity: true,
  });
  document.getElementById("app")!.appendChild(app.canvas);

  const scene = new Scene(app);
  const hud = new Hud(app);
  const bar = new AbilityBar(app);
  const picker = new FactionPicker(app);

  let myId = 0;
  let mapWidth = 0;
  let mapHeight = 0;
  let maxHp = 100;
  let selectedFaction = 1;
  let myFaction = 0;
  let tickRate = 60;
  const factionInfo = new Map<number, { name: string; color: number }>();

  const readKeys = installInput();
  let abilities: AbilityInfo[] = [];
  let activeSlot = 0; // bar slot in use, picked with keys 1–5

  let alive = false;
  let serverTick = 0;
  let respawnTick = 0;
  let predictor: Predictor | undefined;
  const interp = new InterpolationBuffer(INTERP_DELAY);
  // Projectiles: positions interpolated like remote players (same delay, so a
  // shot leaves where its shooter is drawn); velocity/faction from the newest
  // snapshot that had them.
  const projInterp = new InterpolationBuffer(INTERP_DELAY);
  const projMeta = new Map<number, { vx: number; vy: number; factionId: number; seenMs: number }>();
  let projectileRadius = 0;

  let acc = 0;
  let fpsAccum = 0;
  const outbox: PendingInput[] = [];

  const updateHud = (life: LifeState, hp: number): void => {
    // In play: the faction of this life; otherwise the one picked for the next spawn.
    const shownFaction = alive ? myFaction : selectedFaction;
    const fi = factionInfo.get(shownFaction);
    if (fi) {
      hud.setFaction(fi.name, fi.color);
      bar.setAccent(fi.color);
    }
    bar.setVisible(alive);
    picker.setVisible(!alive);
    hud.setOnline(scene.onlineCount());
    const st = scene.factionStats(shownFaction);
    hud.setTerritory(st.cells, st.percent);
    hud.setHp(hp, maxHp, alive);

    if (alive && predictor) {
      const col = Math.floor(predictor.position.x / 100);
      const row = Math.floor(predictor.position.y / 100);
      const ci = scene.cellInfo(col, row);
      hud.setCell(ci.index, ci.ownerColor, ci.captureColor, ci.capturePercent);
    } else {
      hud.hideCell();
    }

    const mapHint = `M — ${scene.mapMode ? "к игроку" : "вся карта"}`;
    if (alive) {
      hud.setHint("");
    } else if (life === LifeState.DEAD) {
      const left = Math.max(0, Math.ceil((respawnTick - serverTick) / tickRate));
      hud.setHint(
        left > 0
          ? `Убит · возрождение через ${left}с · ${mapHint}`
          : `Убит · выберите фракцию и кликните по клетке — возрождение · ${mapHint}`,
      );
    } else {
      hud.setHint(`Выберите фракцию и кликните по клетке — старт · ${mapHint}`);
    }
  };

  const client = new GameClient("ws://localhost:27998/", (msg) => {
    switch (msg.payload.case) {
      case "welcome": {
        const w = msg.payload.value;
        myId = w.playerId;
        mapWidth = w.config?.mapWidth ?? 0;
        mapHeight = w.config?.mapHeight ?? 0;
        maxHp = w.config?.maxHp ?? 100;
        tickRate = w.config?.tickRate || 60;
        selectedFaction = w.factions[0]?.id ?? 1;
        for (const f of w.factions) factionInfo.set(f.id, { name: f.name, color: f.color });
        picker.setFactions(w.factions.map((f) => ({ id: f.id, name: f.name, color: f.color })));
        picker.setSelected(selectedFaction);
        abilities = abilitiesFromWelcome(w.abilities);
        projectileRadius = abilities.find((a) => a.kind === "projectile")?.projectileRadius ?? 0;
        activeSlot = 0;
        bar.setAbilities(abilities);
        bar.setActive(activeSlot);
        scene.setActiveAbility(abilities[activeSlot]);
        scene.setSelf(myId);
        if (w.config) scene.setConfig(w.config.mapWidth, w.config.mapHeight, w.config.maxHp, w.config.tickRate);
        scene.setFactions(w.factions.map((f) => ({ id: f.id, color: f.color })));
        const speed = w.config?.moveSpeed ?? 300;
        predictor = new Predictor(speed, FIXED_DT, {
          maxX: mapWidth * 100 - 1,
          maxY: mapHeight * 100 - 1,
        });
        predictor.reset({ x: (mapWidth * 100) / 2, y: (mapHeight * 100) / 2 });
        break;
      }
      case "mapState":
        scene.setMapState(msg.payload.value.owners, msg.payload.value.captures);
        break;
      case "roster":
        scene.upsertRoster(msg.payload.value.upsert, msg.payload.value.full);
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
            else predictor.reset({ x: self.x, y: self.y });
          } else if (self) {
            predictor.reset({ x: self.x, y: self.y });
          }
        }
        scene.updateMeta(
          s.players,
          {
            attackReadyTick: s.you?.attackReadyTick ?? 0,
            attackCooldownTicks: s.you?.attackCooldownTicks ?? 0,
            blockReadyTick: s.you?.blockReadyTick ?? 0,
            blockCooldownTicks: s.you?.blockCooldownTicks ?? 0,
          },
          serverTick,
        );
        // What others (and you) pressed: swings, blocks, blocked hits.
        scene.addEffects(effectsFromEvents(s.events, abilities, myId, performance.now(), INTERP_DELAY, 1000 / tickRate));
        // Fog of war: cells entering / leaving sight; revealed ones come with their
        // state. A resync (a frame to us was lost) re-reveals the whole sight.
        scene.applyVisibility(s.revealed, s.hidden, s.resync);
        scene.applyCellUpdates(s.cells);
        const remoteStates = new Map<number, RemoteState>();
        for (const p of s.players) if (p.id !== myId) remoteStates.set(p.id, { x: p.x, y: p.y });
        interp.push(performance.now(), remoteStates);

        const nowMs = performance.now();
        const projStates = new Map<number, RemoteState>();
        for (const p of s.projectiles) {
          projStates.set(p.id, { x: p.x, y: p.y });
          projMeta.set(p.id, { vx: p.vx, vy: p.vy, factionId: p.factionId, seenMs: nowMs });
        }
        projInterp.push(nowMs, projStates);
        for (const [id, m] of projMeta) if (nowMs - m.seenMs > 2000) projMeta.delete(id);

        updateHud(life, self?.hp ?? 0);
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
    // 1–5: the active ability bar slot (instant switch; empty slots are ignored).
    const slot = selectSlot(abilities, activeSlot, e.key);
    if (slot !== activeSlot) {
      activeSlot = slot;
      bar.setActive(slot);
      scene.setActiveAbility(abilities[slot]);
    }
  });

  // Attack: hold the left mouse button while alive; aim follows the cursor.
  const mouse = installMouse(app.canvas, () => alive);

  // Click a faction card to pick it; click a cell to spawn / respawn — only when
  // not alive (alive clicks attack).
  app.canvas.addEventListener("click", (e) => {
    const rect = app.canvas.getBoundingClientRect();
    const sx = e.clientX - rect.left;
    const sy = e.clientY - rect.top;
    const picked = picker.pick(sx, sy);
    if (picked !== undefined) {
      selectedFaction = picked;
      picker.setSelected(picked);
      const fi = factionInfo.get(picked);
      if (fi) hud.setFaction(fi.name, fi.color);
      return; // a card click never spawns
    }
    if (alive || respawnTick > serverTick) return;
    const [col, row] = scene.screenToCell(sx, sy);
    if (col < 0 || row < 0 || col >= mapWidth || row >= mapHeight) return;
    myFaction = selectedFaction;
    scene.setSelfFaction(myFaction);
    predictor?.reset({ x: col * 100 + 50, y: row * 100 + 50 });
    client.sendSpawn(row * mapWidth + col, selectedFaction);
  });

  // Fixed-step input + prediction; interpolate remotes; render every frame.
  app.ticker.add((ticker) => {
    fpsAccum += ticker.deltaMS;
    if (fpsAccum >= 250) {
      fpsAccum = 0;
      hud.setFps(Math.round(ticker.FPS));
    }
    if (predictor) {
      acc = Math.min(acc + ticker.deltaMS / 1000, MAX_ACCUM);
      while (acc >= FIXED_DT) {
        acc -= FIXED_DT;
        const k = readKeys();
        const cur = mouse.cursor();
        const [wx, wy] = cur ? scene.screenToWorld(cur.x, cur.y) : [predictor.position.x, predictor.position.y];
        const aim = aimVector(predictor.position, { x: wx, y: wy });
        outbox.push(
          predictor.step({
            moveX: alive ? k.moveX : 0,
            moveY: alive ? k.moveY : 0,
            capturing: k.capturing,
            attack: alive && mouse.attacking(),
            ability: abilities[activeSlot]?.id ?? 0,
            aimX: aim.x,
            aimY: aim.y,
          }),
        );
      }
      while (outbox.length > 0) client.sendInputFrames(outbox.splice(0, 8));
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
    const shots = [];
    for (const id of projInterp.ids(now)) {
      const pos = projInterp.sample(id, now);
      const meta = projMeta.get(id);
      if (pos && meta) shots.push({ ...pos, vx: meta.vx, vy: meta.vy, factionId: meta.factionId, radius: projectileRadius });
    }
    scene.setProjectiles(shots);
    const cd = scene.cooldowns();
    bar.setCooldowns(cd.attack, cd.block);
    const cur = mouse.cursor();
    if (cur) {
      const [wx, wy] = scene.screenToWorld(cur.x, cur.y);
      scene.setAimTarget({ x: wx, y: wy });
    }
    scene.frame();
  });
}

void main();
