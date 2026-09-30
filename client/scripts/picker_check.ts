// Pure checks for the faction picker's layout — the note and the card row in the
// middle of the screen — and click hit-testing (a click on a card selects the
// faction instead of spawning). Run: npx tsx scripts/picker_check.ts
import { BUTTON_GAP, BUTTON_H, BUTTON_W, CARD_H, cardAt, cardRow, centredButton, choiceLayout, inRect, NOTE_GAP } from "../src/render/cardRow.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};

// 1000 px wide screen, 4 cards of 150x44 with 12 px gaps, the row's top at 700:
// row width 636 -> x0 = 182.
const row = cardRow(1000, 4, 700);
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
check("an empty row hits nothing", cardAt(cardRow(1000, 0, 700), 500, 722) === -1);

// The choice in the middle: a 40 px note, the gap, the cards, then «В бой» — one
// block centred on the 1000x800 screen (the bottom of the map stays clear).
const choice = choiceLayout(1000, 800, 4, 40);
const top = choice.noteY - 20;
const bottom = choice.button.y + BUTTON_H;
check("choice: the note above the cards, the gap between", choice.row.y === choice.noteY + 20 + NOTE_GAP);
check("choice: «В бой» under the cards, centred", choice.button.y === choice.row.y + CARD_H + BUTTON_GAP && choice.button.x === (1000 - BUTTON_W) / 2 && choice.button.w === BUTTON_W);
check("choice: the block centred vertically", Math.abs(top - (800 - bottom)) <= 1);
check("choice: the row centred horizontally", choice.row.x0 === 182);
check("choice: clear of the bottom (the hint line and the map's edge)", bottom < 800 - 150);

// Respawning: one button in the middle of the screen.
const respawn = centredButton(1000, 800);
check("respawn button: in the middle", respawn.x === 410 && respawn.y === 378 && respawn.w === BUTTON_W && respawn.h === BUTTON_H);
check("... a click on it hits", inRect(respawn, 500, 400) && inRect(respawn, 410, 378) && inRect(respawn, 590, 422));
check("... a click beside it misses", !inRect(respawn, 409, 400) && !inRect(respawn, 591, 400) && !inRect(respawn, 500, 377) && !inRect(respawn, 500, 423));

console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
