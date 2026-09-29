import { Application } from "pixi.js";

import { aimVector, selectSlot } from "./abilities.js";
import { installInput } from "./input/keyboard.js";
import { installMouse } from "./input/mouse.js";
import { GameClient } from "./net/client.js";
import type { PendingInput } from "./net/prediction.js";
import { routeClose, routeMessage } from "./net/router.js";
import { serverUrl } from "./net/serverUrl.js";
import { AbilityBar } from "./render/abilityBar.js";
import { FactionPicker } from "./render/factionPicker.js";
import { Hud } from "./render/hud.js";
import type { ProjectileSprite } from "./render/projectiles.js";
import { Scene } from "./render/scene.js";
import { showFailure, showStatus, showWelcome, type StatusViews } from "./render/status.js";
import { FIXED_DT, GameState } from "./state/gameState.js";
import { Overlay } from "./ui/overlay.js";

const MAX_ACCUM = 0.25;

// Wires the game together: server messages go through the router into
// GameState; the PixiJS views (render/) draw it; input changes the local
// choices and feeds the predictor; the DOM overlay (ui/) holds text UI.
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
  new Overlay(document.body); // text UI over the canvas; empty until a dialog needs it

  const state = new GameState();
  const scene = new Scene(app, state);
  const views: StatusViews = { hud: new Hud(app), bar: new AbilityBar(app), picker: new FactionPicker(app) };
  const client = new GameClient(
    serverUrl(import.meta.env.VITE_SERVER_URL, window.location),
    (msg) => {
      routeMessage(state, msg, performance.now());
      if (msg.payload.case === "welcome") showWelcome(state, views);
      if (msg.payload.case === "snapshot") showStatus(state, views, scene.mapMode, performance.now());
      if (msg.payload.case === "error") showFailure(state, views, performance.now());
    },
    (code) => {
      routeClose(state, code);
      showFailure(state, views, performance.now());
    },
  );
  client.connect("player");

  const readKeys = installInput();
  window.addEventListener("keydown", (e) => {
    if (e.key.toLowerCase() === "m") {
      scene.toggleMap();
      return;
    }
    // 1–5: the active ability bar slot (instant switch; empty slots are ignored).
    const slot = selectSlot(state.abilities, state.activeSlot, e.key);
    if (slot !== state.activeSlot) {
      state.activeSlot = slot;
      views.bar.setActive(slot);
    }
  });

  // Attack: hold the left mouse button while alive; aim follows the cursor.
  const mouse = installMouse(app.canvas, () => state.alive);

  // Click a faction card to pick it; click a cell to spawn / respawn — only when
  // not alive (alive clicks attack).
  app.canvas.addEventListener("click", (e) => {
    const rect = app.canvas.getBoundingClientRect();
    const sx = e.clientX - rect.left;
    const sy = e.clientY - rect.top;
    const picked = views.picker.pick(sx, sy);
    if (picked !== undefined) {
      state.selectedFaction = picked;
      views.picker.setSelected(picked);
      const faction = state.factions.find((f) => f.id === picked);
      if (faction) views.hud.setFaction(faction.name, faction.color);
      return; // a card click never spawns
    }
    if (state.alive || state.respawnTick > state.serverTick) return;
    const [col, row] = scene.screenToCell(sx, sy);
    if (col < 0 || row < 0 || col >= state.mapWidth || row >= state.mapHeight) return;
    state.myFaction = state.selectedFaction;
    state.roster.setFaction(state.myId, state.myFaction); // the roster doesn't echo our own
    state.predictor?.reset({ x: col * 100 + 50, y: row * 100 + 50 });
    client.sendSpawn(row * state.mapWidth + col, state.selectedFaction);
  });

  // Fixed-step input + prediction; interpolate remotes; render every frame.
  let acc = 0;
  let fpsAccum = 0;
  const outbox: PendingInput[] = [];
  app.ticker.add((ticker) => {
    fpsAccum += ticker.deltaMS;
    if (fpsAccum >= 250) {
      fpsAccum = 0;
      views.hud.setFps(Math.round(ticker.FPS));
    }
    const predictor = state.predictor;
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
            moveX: state.alive ? k.moveX : 0,
            moveY: state.alive ? k.moveY : 0,
            capturing: k.capturing,
            attack: state.alive && mouse.attacking(),
            ability: state.activeAbility?.id ?? 0,
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
    for (const id of state.remoteIds()) {
      const r = state.interp.sample(id, now);
      if (r) remotes.set(id, r);
    }
    scene.setRemotePositions(remotes);
    const shots: ProjectileSprite[] = [];
    for (const id of state.shots.ids(now)) {
      const pos = state.shots.sample(id, now);
      const meta = state.shotMeta.get(id);
      if (pos && meta) shots.push({ ...pos, vx: meta.vx, vy: meta.vy, factionId: meta.factionId, radius: state.projectileRadius });
    }
    scene.setProjectiles(shots);
    scene.addEffects(state.takeEffects());
    const cd = state.cooldownProgress(now);
    views.bar.setCooldowns(cd.attack, cd.block);
    const cur = mouse.cursor();
    if (cur) {
      const [wx, wy] = scene.screenToWorld(cur.x, cur.y);
      scene.setAimTarget({ x: wx, y: wy });
    }
    scene.frame();
  });
}

void main();
