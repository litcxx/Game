// Pure checks for the ability model: Welcome mapping, 1–5 slot selection and the
// aim vector sent in InputFrame. Run: npx tsx scripts/abilities_check.ts
import { create } from "@bufbuild/protobuf";

import { abilitiesFromWelcome, aimVector, cooldownProgress, selectSlot } from "../src/abilities.js";
import { AbilityKind, AbilitySchema } from "../src/gen/game/v1/protocol_pb.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};
const same = (a: unknown, b: unknown) => JSON.stringify(a) === JSON.stringify(b);

const strike = create(AbilitySchema, {
  id: 1, kind: AbilityKind.MELEE, name: "Удар", cooldownTicks: 45, damage: 20, range: 120,
});
const shot = create(AbilitySchema, {
  id: 2, kind: AbilityKind.PROJECTILE, name: "Выстрел", cooldownTicks: 90, damage: 30, range: 500,
  projectileSpeed: 800, projectileRadius: 8,
});
const mystery = create(AbilitySchema, { id: 7, kind: AbilityKind.UNSPECIFIED, name: "?" });

// abilitiesFromWelcome: bar order kept, kinds as strings, unknown kinds skipped.
const bar = abilitiesFromWelcome([strike, mystery, shot]);
check(
  "maps abilities in bar order, skipping unknown kinds",
  same(bar, [
    { id: 1, kind: "melee", name: "Удар", range: 120, cooldownTicks: 45, projectileRadius: 0 },
    { id: 2, kind: "projectile", name: "Выстрел", range: 500, cooldownTicks: 90, projectileRadius: 8 },
  ]),
);

// selectSlot: digits 1–5 pick a filled slot; empty slots and other keys keep the current one.
check("key 2 selects slot 2", selectSlot(bar, 0, "2") === 1);
check("key 1 selects slot 1", selectSlot(bar, 1, "1") === 0);
check("empty slot 3 keeps the current slot", selectSlot(bar, 1, "3") === 1);
check("key 0 is not a slot", selectSlot(bar, 1, "0") === 1);
check("key 6 is past the bar", selectSlot([...bar, ...bar, ...bar], 1, "6") === 1);
check("non-digit keys are ignored", selectSlot(bar, 0, "w") === 0);

// aimVector: unit direction x 1000, rounded; no aim when the cursor sits on the player.
check("aim right", same(aimVector({ x: 100, y: 100 }, { x: 200, y: 100 }), { x: 1000, y: 0 }));
check("aim up-left", same(aimVector({ x: 0, y: 0 }, { x: -30, y: 40 }), { x: -600, y: 800 }));
check("aim diagonal rounds", same(aimVector({ x: 0, y: 0 }, { x: 1, y: 1 }), { x: 707, y: 707 }));
check("cursor on the player -> no aim", same(aimVector({ x: 50, y: 50 }, { x: 50.5, y: 50 }), { x: 0, y: 0 }));

// cooldownProgress: 0 right after use -> 1 when ready; the length comes from the ability used.
check("just used -> 0", cooldownProgress(130, 40, 90) === 0);
check("halfway through a 90-tick cooldown", cooldownProgress(130, 85, 90) === 0.5);
check("ready -> 1", cooldownProgress(130, 130, 90) === 1);
check("past ready stays 1", cooldownProgress(130, 500, 90) === 1);
check("never attacked (length 0) -> ready", cooldownProgress(0, 10, 0) === 1);

console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
