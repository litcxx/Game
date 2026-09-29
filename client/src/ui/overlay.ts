// The overlay's look, added to <head> once (kept here, not in a .css file, so
// the components work the same under Vite and in the Node checks).
const STYLE = `
.overlay { position: fixed; inset: 0; z-index: 10; font-family: monospace; color: #d8d8d0; }
.overlay-modal { position: absolute; inset: 0; display: flex; align-items: center; justify-content: center;
  padding: 16px; background: rgba(6, 6, 9, 0.6); }
.overlay-modal[hidden] { display: none; }
.overlay-dialog { box-sizing: border-box; width: 100%; max-width: 420px; padding: 20px 22px; background: #16161d;
  border: 1px solid #3a3a46; border-radius: 6px; box-shadow: 0 8px 32px rgba(0, 0, 0, 0.5); }
.overlay-dialog h2 { margin: 0 0 8px; font-size: 16px; font-weight: 600; color: #f0f0e8; }
.overlay-dialog p { margin: 0; font-size: 13px; line-height: 1.5; color: #b8b8ae; white-space: pre-line; }
.overlay-dialog label { display: block; font-size: 13px; line-height: 1.5; color: #b8b8ae; }
.overlay-field { display: block; box-sizing: border-box; width: 100%; margin-top: 8px; padding: 7px 10px; font: inherit;
  font-size: 14px; color: #f0f0e8; background: #0e0e13; border: 1px solid #4a4a58; border-radius: 4px; }
.overlay-field:focus-visible { outline: 2px solid #e8c050; outline-offset: 1px; }
.overlay-error { min-height: 1.5em; margin-top: 6px; font-size: 12px; line-height: 1.5; color: #e8806a; }
.overlay-buttons { display: flex; flex-wrap: wrap; justify-content: flex-end; gap: 8px; margin-top: 16px; }
.overlay-buttons button { font: inherit; font-size: 13px; padding: 6px 14px; color: #f0f0e8; background: #26262f;
  border: 1px solid #4a4a58; border-radius: 4px; cursor: pointer; }
.overlay-buttons button:hover { background: #30303b; }
.overlay-buttons button:focus-visible { outline: 2px solid #e8c050; outline-offset: 2px; }
.overlay-toasts { position: absolute; z-index: 1; top: 16px; left: 50%; transform: translateX(-50%); max-width: calc(100vw - 32px);
  display: flex; flex-direction: column; align-items: center; gap: 6px; }
.overlay-toast { padding: 6px 12px; font-size: 13px; background: rgba(10, 10, 15, 0.85); border: 1px solid #3a3a46;
  border-radius: 4px; }
`;

// The DOM layer over the game canvas for text UI — dialogs, notices and the
// nickname screen now, chat later; PixiJS keeps drawing the world. It covers the
// page but lets clicks through to the canvas: a component takes input only
// where it must (see Modal).
export class Overlay {
  readonly root: HTMLElement;

  constructor(parent: HTMLElement) {
    const doc = parent.ownerDocument;
    if (doc.head.querySelector("style[data-overlay]") === null) {
      const style = doc.createElement("style");
      style.setAttribute("data-overlay", "");
      style.textContent = STYLE;
      doc.head.appendChild(style);
    }
    this.root = doc.createElement("div");
    this.root.className = "overlay";
    this.root.style.pointerEvents = "none";
    parent.appendChild(this.root);
  }
}
