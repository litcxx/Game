import type { Application } from "pixi.js";

import { Button } from "./button.js";
import { centredButton } from "./cardRow.js";

// The lone button in the middle of the screen once the faction is chosen: into
// the world again, at the faction's capital — «Возродиться» after a death, «В бой»
// when back without a body. (Before the first spawn «В бой» is the picker's.)
export class SpawnButton {
  private readonly button = new Button("");

  constructor(private readonly app: Application) {
    this.button.setVisible(false);
    app.stage.addChild(this.button.root);
    app.renderer.on("resize", () => this.layout());
    this.layout();
  }

  // Shows the button with `text`; hides it when undefined.
  show(text: string | undefined): void {
    if (text !== undefined) this.button.setText(text);
    this.button.setVisible(text !== undefined);
  }

  hit(sx: number, sy: number): boolean {
    return this.button.hit(sx, sy);
  }

  private layout(): void {
    this.button.place(centredButton(this.app.screen.width, this.app.screen.height));
  }
}
