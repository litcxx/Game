// Pure checks for the faction picker's card row: layout and click hit-testing
// (a click on a card selects the faction instead of spawning).
// Run: npx tsx scripts/picker_check.ts
import { cardAt, cardRow } from "../src/render/cardRow.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};

// 1000x800 screen, 4 cards of 150x44 with 12 px gaps, 56 px above the bottom:
// row width 636 -> x0 = 182, y = 800 - 56 - 44 = 700.
const row = cardRow(1000, 800, 4, 56);
check("row is centred", row.x0 === 182 && row.y === 700);
check("top-left corner hits card 0", cardAt(row, 182, 700) === 0);
check("right edge of card 0 still hits it", cardAt(row, 332, 722) === 0);
check("the gap after card 0 hits nothing", cardAt(row, 338, 722) === -1);
check("left edge of card 1", cardAt(row, 344, 722) === 1);
check("middle of the last card", cardAt(row, 743, 722) === 3);
check("just right of the row", cardAt(row, 819, 722) === -1);
check("far right", cardAt(row, 990, 722) === -1);
check("just left of the row", cardAt(row, 181, 722) === -1);
check("above the row", cardAt(row, 400, 699) === -1);
check("below the row", cardAt(row, 400, 745) === -1);
check("an empty row hits nothing", cardAt(cardRow(1000, 800, 0, 56), 500, 722) === -1);

console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
