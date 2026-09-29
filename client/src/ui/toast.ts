import type { Overlay } from "./overlay.js";

// Short notices stacked at the top of the screen (over any dialog), each gone
// after `lifetimeMs`. They never take clicks, and screen readers announce them
// as they appear.
export class Toasts {
  readonly root: HTMLElement;

  constructor(
    overlay: Overlay,
    private readonly lifetimeMs = 3000,
  ) {
    const doc = overlay.root.ownerDocument;
    this.root = doc.createElement("div");
    this.root.className = "overlay-toasts";
    this.root.setAttribute("role", "status");
    this.root.setAttribute("aria-live", "polite");
    this.root.style.pointerEvents = "none";
    overlay.root.appendChild(this.root);
  }

  show(text: string): void {
    const toast = this.root.ownerDocument.createElement("div");
    toast.className = "overlay-toast";
    toast.textContent = text;
    this.root.appendChild(toast);
    setTimeout(() => toast.remove(), this.lifetimeMs);
  }
}
