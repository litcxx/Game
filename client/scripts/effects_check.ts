// Pure checks for effect timing: which snapshot events become effects (melee
// swing, block shield, blocked hit, your own hit landing), on whose token, when
// they start and how long they last. Run: npx tsx scripts/effects_check.ts
import { create } from "@bufbuild/protobuf";

import type { AbilityInfo } from "../src/abilities.js";
import { effectProgress, effectsFromEvents, type Effect } from "../src/effects.js";
import { GameEventSchema, type GameEvent } from "../src/gen/game/v1/protocol_pb.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};
const same = (a: unknown, b: unknown) => JSON.stringify(a) === JSON.stringify(b);

const bar: AbilityInfo[] = [
  { id: 1, kind: "melee", name: "Удар", range: 120, cooldownTicks: 45, projectileRadius: 0, durationTicks: 0 },
  { id: 2, kind: "projectile", name: "Выстрел", range: 500, cooldownTicks: 90, projectileRadius: 8, durationTicks: 0 },
  { id: 3, kind: "block", name: "Блок", range: 0, cooldownTicks: 45, projectileRadius: 0, durationTicks: 9 },
];
const SELF = 7;
const ARRIVAL = 1000; // ms the snapshot arrived
const DELAY = 100; // remote players are drawn 100 ms in the past
const TICK_MS = 1000 / 60;
const used = (playerId: number, abilityId: number): GameEvent =>
  create(GameEventSchema, { tick: 1, kind: { case: "ability", value: { playerId, abilityId } } });
const hit = (attackerId: number, targetId: number, damage: number, blocked: boolean): GameEvent =>
  create(GameEventSchema, { tick: 1, kind: { case: "hit", value: { attackerId, targetId, damage, blocked } } });
const fx = (events: GameEvent[]): Effect[] => effectsFromEvents(events, bar, SELF, ARRIVAL, DELAY, TICK_MS);

check(
  "remote melee swing: a swing to the melee range, after the interpolation delay",
  same(fx([used(5, 1)]), [{ kind: "swing", playerId: 5, startMs: 1100, durationMs: 220, radius: 120, damage: 0 }]),
);
check("own swing starts on arrival", fx([used(SELF, 1)])[0]?.startMs === 1000);
check(
  "block: a shield for exactly the block's duration",
  same(fx([used(5, 3)]), [{ kind: "shield", playerId: 5, startMs: 1100, durationMs: 150, radius: 0, damage: 0 }]),
);
check("projectile launch: no effect (the shot itself is drawn)", fx([used(5, 2)]).length === 0);
check("unknown ability: no effect", fx([used(5, 99)]).length === 0);
check(
  "blocked hit: a burst on the defender",
  same(fx([hit(5, SELF, 0, true)]), [{ kind: "blocked", playerId: SELF, startMs: 1000, durationMs: 700, radius: 0, damage: 0 }]),
);
check("blocked hit on a remote defender starts after the delay", fx([hit(SELF, 5, 0, true)])[0]?.startMs === 1100);
check("your hit that was blocked: only the block burst", same(fx([hit(SELF, 5, 0, true)]).map((x) => x.kind), ["blocked"]));

// A hit that landed shows only to whoever dealt it: a flash and the damage on the
// target. Everyone else sees just the hp bar drop; a miss shows nothing.
check(
  "your hit: a flash with the damage on the target, when the target is drawn",
  same(fx([hit(SELF, 5, 20, false)]), [{ kind: "hit", playerId: 5, startMs: 1100, durationMs: 600, radius: 0, damage: 20 }]),
);
check("a hit on you: no effect (your hp bar drops)", fx([hit(5, SELF, 20, false)]).length === 0);
check("a hit between others: no effect", fx([hit(5, 9, 20, false)]).length === 0);
check(
  "a death: no effect",
  fx([create(GameEventSchema, { tick: 1, kind: { case: "death", value: { victimId: 5, killerId: 7 } } })]).length === 0,
);

// effectProgress: undefined before the start and from the end on; linear between.
const e: Effect = { kind: "swing", playerId: 5, startMs: 1000, durationMs: 200, radius: 120, damage: 0 };
check("not started yet", effectProgress(e, 999) === undefined);
check("starts at 0", effectProgress(e, 1000) === 0);
check("halfway", effectProgress(e, 1100) === 0.5);
check("over at its end", effectProgress(e, 1200) === undefined);

console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
