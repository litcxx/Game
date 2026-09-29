// Ranged e2e: a shooter and a target of another faction spawn 3 cells apart. The
// shooter uses the projectile ability (from Welcome.abilities) aimed at the
// target. Verify a projectile shows up in snapshots, then a HitEvent with the
// ability's damage, and the target's hp drops by exactly one shot (the shooter
// lets go of the attack after the first hit).
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import {
  AbilityKind,
  ClientMessageSchema,
  ServerMessageSchema,
  type ClientMessage,
} from "../src/gen/game/v1/protocol_pb.js";
import { uniqueName } from "./uniqueName.js";

const URL = process.env.SERVER_URL ?? "ws://127.0.0.1:27998/";
const shooter = new WebSocket(URL);
const target = new WebSocket(URL);
shooter.binaryType = "arraybuffer";
target.binaryType = "arraybuffer";

let shooterId = 0;
let targetId = 0;
let shooterFaction = 0;
let shotAbility = 0; // Ability.id of the projectile ability
let shotDamage = 0;
let maxHp = 0;
let shooterSpawned = false;
let targetSpawned = false;
let firing = false;

let projectileSeen = false;
let hitDamage = -1;
let targetHp = -1;

// Its own row: a player stays in the world for the reconnect grace (30 s) after
// its socket closes — fog_smoke's bodies on row 20 would stop the shot.
const shooterCell = 45 * 100 + 20; // (col 20, row 45) -> centre (2050, 4550)
const targetCell = 45 * 100 + 23; // (col 23, row 45) -> centre (2350, 4550), 300 units right

function send(ws: WebSocket, msg: ClientMessage): void {
  ws.send(toBinary(ClientMessageSchema, msg));
}
const helloMsg = (name: string) =>
  create(ClientMessageSchema, { payload: { case: "hello", value: { protocolVersion: 1, name: uniqueName(name) } } });
const spawnMsg = (cell: number, factionId: number) =>
  create(ClientMessageSchema, { payload: { case: "spawn", value: { cell, factionId } } });
const fireMsg = (seq: number, attack: boolean) =>
  create(ClientMessageSchema, {
    payload: {
      case: "input",
      value: {
        frames: [
          { seq, moveX: 0, moveY: 0, capturing: false, attack, ability: shotAbility, aimX: 1000, aimY: 0 },
        ],
      },
    },
  });

function maybeFire(): void {
  if (!firing && shooterSpawned && targetSpawned && targetId >= 1 && shotAbility >= 1) {
    firing = true;
    send(shooter, fireMsg(1, true)); // held until the first hit
  }
}

shooter.onopen = () => send(shooter, helloMsg("shooter"));
target.onopen = () => send(target, helloMsg("target"));

shooter.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    const w = m.payload.value;
    shooterId = w.playerId;
    maxHp = w.config?.maxHp ?? 0;
    const shot = w.abilities.find((a) => a.kind === AbilityKind.PROJECTILE);
    shotAbility = shot?.id ?? 0;
    shotDamage = shot?.damage ?? 0;
    shooterFaction = w.factions[0]?.id ?? 1;
    send(shooter, spawnMsg(shooterCell, shooterFaction));
    shooterSpawned = true;
    maybeFire();
  } else if (m.payload.case === "snapshot") {
    const s = m.payload.value;
    if (s.projectiles.some((p) => p.factionId === shooterFaction && p.vx > 0 && p.vy === 0)) projectileSeen = true;
    const body = s.players.find((p) => p.id === targetId);
    if (body) targetHp = body.hp;
    for (const e of s.events) {
      if (e.kind.case === "hit" && e.kind.value.attackerId === shooterId && e.kind.value.targetId === targetId) {
        if (hitDamage < 0) send(shooter, fireMsg(2, false)); // one shot is enough
        hitDamage = e.kind.value.damage;
      }
    }
  }
};

target.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    targetId = m.payload.value.playerId;
    const factions = m.payload.value.factions;
    send(target, spawnMsg(targetCell, factions[1]?.id ?? 2)); // not the shooter's faction
    targetSpawned = true;
    maybeFire();
  }
};

shooter.onerror = () => console.error("[ranged] shooter socket error");
target.onerror = () => console.error("[ranged] target socket error");

setTimeout(() => {
  console.log(
    `[ranged] shooter=${shooterId} target=${targetId} ability=${shotAbility} projectileSeen=${projectileSeen} ` +
      `hitDamage=${hitDamage} (expected ${shotDamage}) targetHp=${targetHp} (expected ${maxHp - shotDamage})`,
  );
  const pass =
    shotAbility >= 1 &&
    shotDamage > 0 &&
    projectileSeen &&
    hitDamage === shotDamage &&
    targetHp === maxHp - shotDamage;
  console.log("VERDICT:", pass ? "PASS" : "FAIL");
  shooter.close();
  target.close();
  process.exit(pass ? 0 : 1);
}, 2500);
