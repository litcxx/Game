// Ranged e2e: a shooter and a target of another faction spawn 3 cells apart. The
// shooter uses the projectile ability (from Welcome.abilities) aimed at the
// target, in one input frame (FIRE) holding attack, and sends no more. Verify a
// projectile shows up in snapshots, then a HitEvent with the ability's damage,
// and the target's hp drops by exactly one shot, though the attack is held past
// the cooldown: an attack comes from a frame, not from a repeat (LTC-89). The
// shooter's own shot names that frame (ProjectileState.input_seq) and so does
// its last attack (SelfState.last_attack_input_seq); the target sees the shot
// naming none.
import { create, fromBinary, toBinary } from "@bufbuild/protobuf";

import {
  AbilityKind,
  ClientMessageSchema,
  ProtocolVersion,
  ServerMessageSchema,
  type ClientMessage,
} from "../src/gen/game/v1/protocol_pb.js";
import { spot } from "./smokeMap.js";
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
const FIRE = 7; // the input_seq of the shooter's one frame
const ownShots = new Set<number>(); // projectile ids the shooter saw as its own
let ownShotsName = true; // ...each with input_seq FIRE
let lastAttackInputSeq = -1;
let targetSawShot = false;
let targetSawNoName = true; // the shot as the target sees it: input_seq 0, not mine
let hitDamage = -1;
let targetHp = -1;

// Its own row: a player stays in the world for the reconnect grace (30 s) after
// its socket closes — fog_smoke's bodies on row 20 would stop the shot.
// Their capitals on the smoke map: (20, 45) and (23, 45), 300 units apart on one row.
const SHOOTER = spot("ranged-shooter");
const TARGET = spot("ranged-target");

function send(ws: WebSocket, msg: ClientMessage): void {
  ws.send(toBinary(ClientMessageSchema, msg));
}
const helloMsg = (name: string) =>
  create(ClientMessageSchema, { payload: { case: "hello", value: { protocolVersion: ProtocolVersion.CURRENT, name: uniqueName(name) } } });
const spawnMsg = (factionId: number) => create(ClientMessageSchema, { payload: { case: "spawn", value: { factionId } } });
const fireMsg = (inputSeq: number, attack: boolean) =>
  create(ClientMessageSchema, {
    payload: {
      case: "input",
      value: {
        frames: [
          { inputSeq, moveX: 0, moveY: 0, capturing: false, attack, abilityId: shotAbility, aimX: 1000, aimY: 0 },
        ],
      },
    },
  });

function maybeFire(): void {
  if (!firing && shooterSpawned && targetSpawned && targetId >= 1 && shotAbility >= 1) {
    firing = true;
    send(shooter, fireMsg(FIRE, true)); // held: no frame lets go
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
    shooterFaction = SHOOTER.factionId;
    send(shooter, spawnMsg(shooterFaction));
    shooterSpawned = true;
    maybeFire();
  } else if (m.payload.case === "snapshot") {
    const s = m.payload.value;
    if (s.projectiles.some((p) => p.factionId === shooterFaction && p.vx > 0 && p.vy === 0)) projectileSeen = true;
    for (const p of s.projectiles.filter((p) => p.mine)) {
      ownShots.add(p.id);
      if (p.inputSeq !== FIRE) ownShotsName = false;
    }
    lastAttackInputSeq = s.you?.lastAttackInputSeq ?? lastAttackInputSeq;
    const body = s.players.find((p) => p.id === targetId);
    if (body) targetHp = body.hp;
    for (const e of s.events) {
      if (e.kind.case === "hit" && e.kind.value.attackerId === shooterId && e.kind.value.targetId === targetId) {
        hitDamage = e.kind.value.damage;
      }
    }
  }
};

target.onmessage = (ev: MessageEvent) => {
  const m = fromBinary(ServerMessageSchema, new Uint8Array(ev.data as ArrayBuffer));
  if (m.payload.case === "welcome") {
    targetId = m.payload.value.playerId;
    send(target, spawnMsg(TARGET.factionId)); // not the shooter's faction
    targetSpawned = true;
    maybeFire();
  } else if (m.payload.case === "snapshot") {
    for (const p of m.payload.value.projectiles.filter((p) => p.factionId === SHOOTER.factionId)) {
      targetSawShot = true;
      if (p.mine || p.inputSeq !== 0) targetSawNoName = false;
    }
  }
};

shooter.onerror = () => console.error("[ranged] shooter socket error");
target.onerror = () => console.error("[ranged] target socket error");

setTimeout(() => {
  console.log(
    `[ranged] shooter=${shooterId} target=${targetId} ability=${shotAbility} projectileSeen=${projectileSeen} ` +
      `hitDamage=${hitDamage} (expected ${shotDamage}) targetHp=${targetHp} (expected ${maxHp - shotDamage}) ` +
      `ownShots=${ownShots.size} named=${ownShotsName} lastAttackInputSeq=${lastAttackInputSeq} (expected ${FIRE}) ` +
      `targetSawShot=${targetSawShot} unnamed=${targetSawNoName}`,
  );
  const pass =
    shotAbility >= 1 &&
    shotDamage > 0 &&
    projectileSeen &&
    hitDamage === shotDamage &&
    targetHp === maxHp - shotDamage &&
    ownShots.size === 1 &&
    ownShotsName &&
    lastAttackInputSeq === FIRE &&
    targetSawShot &&
    targetSawNoName;
  console.log("VERDICT:", pass ? "PASS" : "FAIL");
  shooter.close();
  target.close();
  process.exit(pass ? 0 : 1);
}, 3500); // past the shot's cooldown (1.5 s): a repeat would have fired again
