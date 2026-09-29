// Checks for the DOM overlay components (src/ui) in happy-dom, a DOM for Node:
// the layer over the canvas lets clicks through, a modal shows its title, text
// and buttons and blocks the canvas while open, toasts stack and fade, the
// nickname screen checks and sends a name, the connection dialogs count down to
// a reconnect or offer a reload. Run:
//   npx tsx scripts/ui_check.ts
import { Window } from "happy-dom";

import { isTyping } from "../src/input/keyboard.js";
import { ConnectionDialogs, reconnectText } from "../src/ui/connectionDialogs.js";
import { Modal } from "../src/ui/modal.js";
import { checkName, NicknameScreen } from "../src/ui/nicknameScreen.js";
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

// --- Nickname screen -----------------------------------------------------------------
{
  check("a name is trimmed", JSON.stringify(checkName("  Ann 　")) === JSON.stringify({ name: "Ann" }));
  check("an empty or blank name is refused", "error" in checkName("") && "error" in checkName("   "));
  check("16 characters (not bytes) fit", JSON.stringify(checkName("абвгдеёжзийклмно")) === JSON.stringify({ name: "абвгдеёжзийклмно" }));
  check("17 are too many", "error" in checkName("abcdefghijklmnopq"));
  check("an emoji is one character", "name" in checkName("\u{1F600}".repeat(16)));

  const submitted: string[] = [];
  const screen = new NicknameScreen(overlay, (name) => submitted.push(name));
  const input = screen.root.querySelector("input") as unknown as HTMLInputElement;
  const form = screen.root.querySelector("form") as unknown as HTMLFormElement;
  const error = screen.root.querySelector("[role=alert]") as unknown as HTMLElement;
  check("a new screen is hidden", !screen.isOpen && screen.root.hidden);

  screen.show("Ann");
  check("show: open, with the saved name to edit", screen.isOpen && input.value === "Ann" && error.textContent === "");
  check("show: it takes the clicks the canvas would get", screen.root.style.pointerEvents === "auto");
  (screen.root.querySelector("button[type=submit]") as unknown as HTMLElement).click();
  check("Play sends the name", JSON.stringify(submitted) === JSON.stringify(["Ann"]));

  input.value = "   ";
  form.dispatchEvent(new window.Event("submit", { cancelable: true }) as unknown as Event);
  check("a blank name is not sent, and says why", submitted.length === 1 && error.textContent === "Введите ник");
  input.value = "  Bob ";
  form.dispatchEvent(new window.Event("submit", { cancelable: true }) as unknown as Event);
  check("Enter (submit) sends the trimmed name", submitted.at(-1) === "Bob");

  screen.show("Ann", "Имя занято");
  check("show with an error: says why", error.textContent === "Имя занято" && input.value === "Ann");
  screen.hide();
  check("hide: closed", !screen.isOpen && screen.root.hidden);

  check("typing in a field is not playing", isTyping(input) && !isTyping(document.body as unknown as EventTarget) && !isTyping(null));
}

// --- Connection dialogs ------------------------------------------------------------------
{
  check("reconnect text: the countdown and the attempt", reconnectText(2, 3) === "Связь с сервером потеряна.\nНовая попытка через 3 с (попытка 2).");
  check("reconnect text: the wait is over", reconnectText(2, 0) === "Связь с сервером потеряна.\nПодключаемся… (попытка 2).");

  let tick: (() => void) | undefined;
  let cleared = 0;
  const clock = {
    setInterval: (fn: () => void) => ((tick = fn), 1),
    clearInterval: () => {
      cleared++;
      tick = undefined;
    },
  };
  const modal = new Modal(overlay);
  const dialogs = new ConnectionDialogs(modal, clock);
  const text = () => modal.root.querySelector("p")?.textContent ?? "";

  dialogs.reconnecting(2, 2000);
  check("reconnecting: a dialog with no buttons to wait out", modal.isOpen && modal.root.querySelectorAll("button").length === 0);
  check("reconnecting: the countdown", text() === reconnectText(2, 2));
  tick?.();
  check("... ticking down", text() === reconnectText(2, 1));
  tick?.();
  tick?.();
  check("... to the attempt", text() === reconnectText(2, 0));

  let reloads = 0;
  dialogs.failed("Игра открыта в другой вкладке", () => reloads++);
  const buttons = modal.root.querySelectorAll("button");
  check("failed: why, and a reload button", text() === "Игра открыта в другой вкладке" && buttons.length === 1 && buttons[0]?.textContent === "Перезагрузить");
  check("failed: the countdown stopped", cleared >= 1 && tick === undefined);
  (buttons[0] as unknown as HTMLElement).click();
  check("reload reloads", reloads === 1);

  dialogs.close();
  check("close: no dialog", !modal.isOpen);
}

await window.happyDOM.close();
console.log("VERDICT:", failures === 0 ? "PASS" : `FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
