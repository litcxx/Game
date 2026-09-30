// Pure checks for the name pill and hp bar over a player's token
// (src/render/nameplate.ts): the pill grows with the name, the bar has one width
// for everyone — a long name must not read as more hp (playtest #0). Run:
//   npx tsx scripts/nameplate_check.ts
import { HP_BAR_W, nameplate } from "../src/render/nameplate.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};
const same = (a: unknown, b: unknown) => JSON.stringify(a) === JSON.stringify(b);

const short = nameplate(18, 1); // "Ann"
const long = nameplate(110, 1); // "Длинное-имя-тут"
check("the pill grows with the name", short.pill !== undefined && long.pill !== undefined && long.pill.w > short.pill.w);
check("the hp bar has one width, whatever the name", same(short.bar, long.bar) && short.bar.w === HP_BAR_W);
check("... centred over the token", short.bar.x === -HP_BAR_W / 2);
check("... right under the pill", short.bar.y === short.pill!.y + short.pill!.h);

const nameless = nameplate(undefined, 1);
check("no name yet: no pill, the same bar over the token", nameless.pill === undefined && nameless.bar.w === HP_BAR_W && nameless.bar.y < 0);

check("the fill is the hp share of the bar", nameplate(18, 0.5).fillW === HP_BAR_W / 2);
check("... none at 0 and below, the whole bar at 1 and above", nameplate(18, -0.2).fillW === 0 && nameplate(18, 1.5).fillW === HP_BAR_W);

console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
