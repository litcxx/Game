import type { Overlay } from "./overlay.js";

// The server's limit (join_system normalize_name): 1–16 characters after trimming.
export const MAX_NAME_CHARS = 16;

// A name as typed: trimmed and within the length, or why not. The server has the
// last word (no invisible characters, not taken).
export function checkName(raw: string): { name: string } | { error: string } {
  const name = raw.trim();
  if (name.length === 0) return { error: "Введите ник" };
  if ([...name].length > MAX_NAME_CHARS) return { error: `Не больше ${MAX_NAME_CHARS} символов` };
  return { name };
}

let nextId = 0;

// Asks for the player's name before joining (and again when the server refuses
// one). A form over the game: Enter or «Играть» sends the checked name.
export class NicknameScreen {
  readonly root: HTMLElement; // the backdrop
  private readonly input: HTMLInputElement;
  private readonly error: HTMLElement;

  constructor(
    overlay: Overlay,
    private readonly onSubmit: (name: string) => void,
  ) {
    const doc = overlay.root.ownerDocument;
    const id = `overlay-nickname-${nextId++}`;
    this.root = doc.createElement("div");
    this.root.className = "overlay-modal";
    this.root.hidden = true;
    this.root.style.pointerEvents = "auto";

    const form = doc.createElement("form");
    form.className = "overlay-dialog";
    form.setAttribute("role", "dialog");
    form.setAttribute("aria-modal", "true");
    form.setAttribute("aria-labelledby", `${id}-title`);
    const title = doc.createElement("h2");
    title.id = `${id}-title`;
    title.textContent = "Территория";
    const label = doc.createElement("label");
    label.htmlFor = `${id}-input`;
    label.textContent = `Ваш ник — его видят все игроки (до ${MAX_NAME_CHARS} символов)`;
    this.input = doc.createElement("input");
    this.input.id = `${id}-input`;
    this.input.className = "overlay-field";
    this.input.type = "text";
    this.input.setAttribute("autocomplete", "nickname");
    this.input.spellcheck = false;
    this.error = doc.createElement("div");
    this.error.className = "overlay-error";
    this.error.setAttribute("role", "alert");
    const buttons = doc.createElement("div");
    buttons.className = "overlay-buttons";
    const play = doc.createElement("button");
    play.type = "submit";
    play.textContent = "Играть";
    buttons.appendChild(play);
    form.append(title, label, this.input, this.error, buttons);
    form.addEventListener("submit", (e) => {
      e.preventDefault();
      this.submit();
    });

    this.root.appendChild(form);
    overlay.root.appendChild(this.root);
  }

  get isOpen(): boolean {
    return !this.root.hidden;
  }

  // Shows the form with `name` to edit and, if a name was refused, why.
  show(name: string, error = ""): void {
    this.input.value = name;
    this.error.textContent = error;
    this.root.hidden = false;
    this.input.focus();
    this.input.select();
  }

  hide(): void {
    this.root.hidden = true;
  }

  private submit(): void {
    const checked = checkName(this.input.value);
    if ("error" in checked) {
      this.error.textContent = checked.error;
      this.input.focus();
      return;
    }
    this.error.textContent = "";
    this.onSubmit(checked.name);
  }
}
