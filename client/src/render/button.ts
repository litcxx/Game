import { Container, Graphics, Text } from "pixi.js";

import { inRect, type Rect } from "./cardRow.js";

const FACE = 0xe0b060; // gold, as the selected card and the active ability slot

// A labelled button on the canvas (the spawn controls): a gold-rimmed box with its
// text. main routes canvas clicks through hit() — a PixiJS view, no DOM.
export class Button {
  readonly root = new Container();
  private readonly box = new Graphics();
  private readonly label: Text;
  private rect: Rect = { x: 0, y: 0, w: 0, h: 0 };

  constructor(text: string) {
    this.label = new Text({
      text,
      style: { fill: "#f0e6c8", fontFamily: "monospace", fontSize: 16, fontWeight: "bold", letterSpacing: 1 },
    });
    this.label.anchor.set(0.5);
    this.root.addChild(this.box, this.label);
  }

  setText(text: string): void {
    if (this.label.text !== text) this.label.text = text;
  }

  place(rect: Rect): void {
    this.rect = rect;
    this.box
      .clear()
      .roundRect(rect.x, rect.y, rect.w, rect.h, 6)
      .fill({ color: 0x0a0a0f, alpha: 0.85 })
      .stroke({ width: 2, color: FACE });
    this.label.position.set(Math.round(rect.x + rect.w / 2), Math.round(rect.y + rect.h / 2));
  }

  setVisible(visible: boolean): void {
    this.root.visible = visible;
  }

  // Whether a canvas point is on the button (never while hidden, itself or a parent).
  hit(sx: number, sy: number): boolean {
    return this.root.visible && (this.root.parent?.visible ?? true) && inRect(this.rect, sx, sy);
  }
}
