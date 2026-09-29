import type { Overlay } from "./overlay.js";

export interface ModalButton {
  label: string;
  onClick: () => void;
}

export interface ModalContent {
  title: string;
  text: string;
  buttons?: readonly ModalButton[]; // none: a state to wait out (e.g. reconnecting)
}

let nextId = 0;

// A modal dialog over the game: a title, a text and its buttons. While open it
// dims the game and takes the clicks the canvas would otherwise get. It shows
// one dialog at a time: open() replaces what is shown. Text is set as text,
// never parsed as markup.
export class Modal {
  readonly root: HTMLElement; // the backdrop
  private readonly dialog: HTMLElement;
  private readonly titleId = `overlay-modal-title-${nextId++}`;

  constructor(overlay: Overlay) {
    const doc = overlay.root.ownerDocument;
    this.root = doc.createElement("div");
    this.root.className = "overlay-modal";
    this.root.hidden = true;
    this.root.style.pointerEvents = "auto";
    this.dialog = doc.createElement("div");
    this.dialog.className = "overlay-dialog";
    this.dialog.setAttribute("role", "dialog");
    this.dialog.setAttribute("aria-modal", "true");
    this.dialog.setAttribute("aria-labelledby", this.titleId);
    this.root.appendChild(this.dialog);
    overlay.root.appendChild(this.root);
  }

  get isOpen(): boolean {
    return !this.root.hidden;
  }

  open(content: ModalContent): void {
    const doc = this.root.ownerDocument;
    const title = doc.createElement("h2");
    title.id = this.titleId;
    title.textContent = content.title;
    const text = doc.createElement("p");
    text.textContent = content.text;
    const parts: HTMLElement[] = [title, text];

    const buttons = content.buttons ?? [];
    if (buttons.length > 0) {
      const row = doc.createElement("div");
      row.className = "overlay-buttons";
      for (const b of buttons) {
        const button = doc.createElement("button");
        button.type = "button";
        button.textContent = b.label;
        button.addEventListener("click", () => b.onClick());
        row.appendChild(button);
      }
      parts.push(row);
    }
    this.dialog.replaceChildren(...parts);
    this.root.hidden = false;
    this.dialog.querySelector("button")?.focus();
  }

  close(): void {
    this.root.hidden = true;
    this.dialog.replaceChildren();
  }
}
