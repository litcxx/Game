// Checks for the DOM overlay components (src/ui) in happy-dom, a DOM for Node:
// the layer over the canvas lets clicks through, a modal shows its title, text
// and buttons and blocks the canvas while open, toasts stack and fade. Run:
//   npx tsx scripts/ui_check.ts
import { Window } from "happy-dom";

import { Modal } from "../src/ui/modal.js";
import { Overlay } from "../src/ui/overlay.js";
import { Toasts } from "../src/ui/toast.js";

let failures = 0;
const check = (name: string, cond: boolean) => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}`);
  if (!cond) failures++;
};
const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

const window = new Window();
const document = window.document;
const body = document.body as unknown as HTMLElement;

// --- The layer -------------------------------------------------------------------
const overlay = new Overlay(body);
check("the overlay is mounted over the page", overlay.root.parentElement === body && overlay.root.classList.contains("overlay"));
check("the overlay lets clicks through to the canvas", overlay.root.style.pointerEvents === "none");
new Overlay(body);
check("its stylesheet is added once", document.head.querySelectorAll("style[data-overlay]").length === 1);

// --- Modal -----------------------------------------------------------------------
{
  const modal = new Modal(overlay);
  check("a new modal is closed", !modal.isOpen && modal.root.hidden);

  let clicks = 0;
  modal.open({ title: "Связь потеряна", text: "Переподключение…", buttons: [{ label: "Перезагрузить", onClick: () => clicks++ }] });
  const dialog = modal.root.querySelector("[role=dialog]");
  const buttons = modal.root.querySelectorAll("button");
  check("open: shown", modal.isOpen && !modal.root.hidden);
  check("open: a modal dialog", dialog?.getAttribute("aria-modal") === "true");
  check("open: title and text", dialog?.textContent?.includes("Связь потеряна") === true && dialog?.textContent?.includes("Переподключение…") === true);
  check("open: its buttons", buttons.length === 1 && buttons[0]?.textContent === "Перезагрузить");
  check("open: it takes the clicks the canvas would get", modal.root.style.pointerEvents === "auto");
  (buttons[0] as unknown as HTMLElement).click();
  check("a button calls its handler", clicks === 1);

  modal.open({ title: "<b>Ошибка</b>", text: "Без кнопок" });
  check("reopening replaces the content", modal.root.querySelectorAll("button").length === 0);
  check("text is text, not markup", modal.root.querySelector("b") === null && modal.root.textContent?.includes("<b>Ошибка</b>") === true);

  modal.close();
  check("close: hidden again", !modal.isOpen && modal.root.hidden);
}

// --- Toasts ----------------------------------------------------------------------
{
  const toasts = new Toasts(overlay, 50);
  toasts.show("Первый");
  toasts.show("Второй");
  const shown = [...toasts.root.children].map((t) => t.textContent);
  check("toasts stack in order", JSON.stringify(shown) === JSON.stringify(["Первый", "Второй"]));
  check("toasts are announced politely", toasts.root.getAttribute("aria-live") === "polite");
  check("toasts don't catch clicks", toasts.root.style.pointerEvents === "none");
  await sleep(120);
  check("toasts fade after their time", toasts.root.children.length === 0);
}

await window.happyDOM.close();
console.log("VERDICT:", failures === 0 ? "PASS" : `FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
