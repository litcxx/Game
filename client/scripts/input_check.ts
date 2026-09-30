// Checks for keyboard and mouse input (src/input) in happy-dom, a DOM for Node:
// keys count by their place on the keyboard (KeyboardEvent.code), so the game
// plays the same in any layout; a key is never left held — not by a layout
// switch mid-press, not by the window losing focus; a right click on the canvas
// opens no browser menu. Run:
//   npx tsx scripts/input_check.ts
import { Window } from "happy-dom";

import { installInput } from "../src/input/keyboard.js";
import { installMouse } from "../src/input/mouse.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};
const same = (a: unknown, b: unknown) => JSON.stringify(a) === JSON.stringify(b);
const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

const window = new Window();
const document = window.document;
const readKeys = installInput(window as unknown as globalThis.Window);

const press = (code: string, key: string) =>
  window.dispatchEvent(new window.KeyboardEvent("keydown", { code, key, bubbles: true }));
const release = (code: string, key: string) =>
  window.dispatchEvent(new window.KeyboardEvent("keyup", { code, key, bubbles: true }));
const intent = (moveX: number, moveY: number, capturing = false) => ({ moveX, moveY, capturing });

// --- Keys by place, not by letter --------------------------------------------------
check("nothing held -> no intent", same(readKeys(), intent(0, 0)));

press("KeyW", "ц");
check("Russian layout: W («ц») moves up", same(readKeys(), intent(0, -1)));
release("KeyW", "ц");
press("KeyA", "ф");
check("Russian layout: A («ф») moves left", same(readKeys(), intent(-1, 0)));
release("KeyA", "ф");
press("KeyS", "ы");
check("Russian layout: S («ы») moves down", same(readKeys(), intent(0, 1)));
release("KeyS", "ы");
press("KeyD", "в");
check("Russian layout: D («в») moves right", same(readKeys(), intent(1, 0)));
release("KeyD", "в");
press("KeyE", "у");
check("Russian layout: E («у») captures", same(readKeys(), intent(0, 0, true)));
release("KeyE", "у");
check("released -> no intent", same(readKeys(), intent(0, 0)));

press("ArrowUp", "ArrowUp");
press("ArrowRight", "ArrowRight");
check("arrows move", same(readKeys(), intent(1, -1)));
release("ArrowUp", "ArrowUp");
release("ArrowRight", "ArrowRight");

press("KeyW", "W");
check("Caps Lock / Shift (key «W») still moves up", same(readKeys(), intent(0, -1)));
release("KeyW", "W");

press("KeyQ", "й");
check("a key the game doesn't use -> no intent", same(readKeys(), intent(0, 0)));
release("KeyQ", "й");

// --- A key is never left held ------------------------------------------------------
press("KeyW", "w");
release("KeyW", "ц"); // Alt+Shift while holding W: the release comes in the other layout
check("layout switched mid-press: the key is released", same(readKeys(), intent(0, 0)));

press("KeyD", "d");
press("KeyE", "e");
window.dispatchEvent(new window.Event("blur")); // alt-tab, a browser menu: no keyup comes
check("the window losing focus releases every key", same(readKeys(), intent(0, 0)));

// --- Typing is not game input ------------------------------------------------------
const field = document.createElement("input");
document.body.appendChild(field);
field.dispatchEvent(new window.KeyboardEvent("keydown", { code: "KeyW", key: "w", bubbles: true }));
check("typing W into a text field doesn't move up", readKeys().moveY === 0);
field.dispatchEvent(new window.KeyboardEvent("keyup", { code: "KeyW", key: "w", bubbles: true }));

// --- Mouse -------------------------------------------------------------------------
const canvas = document.createElement("canvas");
document.body.appendChild(canvas);
const mouse = installMouse(canvas as unknown as HTMLCanvasElement, () => true);
const mouseEvent = (type: string, button: number) =>
  new window.MouseEvent(type, { button, bubbles: true, cancelable: true });

const menuOnCanvas = mouseEvent("contextmenu", 2);
canvas.dispatchEvent(menuOnCanvas);
check("right click on the canvas opens no browser menu", menuOnCanvas.defaultPrevented);
const menuOnPage = mouseEvent("contextmenu", 2);
field.dispatchEvent(menuOnPage);
check("the menu stays elsewhere (paste into the nickname field)", !menuOnPage.defaultPrevented);

canvas.dispatchEvent(mouseEvent("mousedown", 2));
check("the right button doesn't attack", !mouse.attacking());

canvas.dispatchEvent(mouseEvent("mousedown", 0));
check("the left button attacks while held", mouse.attacking());
await sleep(80); // past the 60 ms min-hold
window.dispatchEvent(mouseEvent("mouseup", 0));
check("releasing the left button anywhere in the window stops the attack", !mouse.attacking());

await window.happyDOM.close();
console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
