import { Application, Container, Graphics, Text } from "pixi.js";

import { BAR_SLOTS, type AbilityInfo } from "../abilities.js";

const SLOT = 56; // px, square
const GAP = 12;
const ACTIVE = 0xe0b060; // gold rim of the slot in use (concept)
const BORDER = 0x3a3a46;

interface Slot {
  frame: Graphics;
  icon: Graphics;
  key: Text;
}

// Bottom-centre ability bar (keys 1–5): one icon per ability kind — ◆ melee,
// ● projectile — in the faction's colour; the active slot glows gold. Shown
// while the player is alive.
export class AbilityBar {
  private readonly root = new Container();
  private readonly slots: Slot[] = [];
  private abilities: readonly AbilityInfo[] = [];
  private active = 0;
  private accent = 0xd8d8d0;

  constructor(private readonly app: Application) {
    for (let i = 0; i < BAR_SLOTS; i++) {
      const frame = new Graphics();
      const icon = new Graphics();
      icon.position.set(SLOT / 2, SLOT / 2);
      const key = new Text({
        text: String(i + 1),
        style: { fill: "#c8c8b8", fontFamily: "monospace", fontSize: 11 },
      });
      key.position.set(5, 3);
      const slot = new Container();
      slot.position.set(i * (SLOT + GAP), 0);
      slot.addChild(frame, icon, key);
      this.root.addChild(slot);
      this.slots.push({ frame, icon, key });
    }
    app.stage.addChild(this.root);
    this.layout();
    app.renderer.on("resize", () => this.layout());
    this.redraw();
  }

  setAbilities(abilities: readonly AbilityInfo[]): void {
    this.abilities = abilities;
    this.redraw();
  }

  setActive(slot: number): void {
    if (slot === this.active) return;
    this.active = slot;
    this.redraw();
  }

  // Icon colour: the player's faction.
  setAccent(color: number): void {
    if (color === this.accent) return;
    this.accent = color;
    this.redraw();
  }

  setVisible(visible: boolean): void {
    this.root.visible = visible;
  }

  private redraw(): void {
    this.slots.forEach((s, i) => {
      const ability = this.abilities[i];
      const isActive = ability !== undefined && i === this.active;
      s.frame.clear();
      if (isActive) s.frame.roundRect(-4, -4, SLOT + 8, SLOT + 8, 7).fill({ color: ACTIVE, alpha: 0.12 });
      s.frame
        .roundRect(0, 0, SLOT, SLOT, 4)
        .fill({ color: 0x0a0a0f, alpha: 0.92 }) // opaque enough to hide cell labels
        .stroke({
          width: isActive ? 2 : 1,
          color: isActive ? ACTIVE : BORDER,
          alpha: ability !== undefined ? 1 : 0.5,
        });

      s.icon.clear();
      if (ability?.kind === "melee") {
        const r = 11;
        s.icon.moveTo(0, -r).lineTo(r, 0).lineTo(0, r).lineTo(-r, 0).closePath().fill(this.accent);
      } else if (ability?.kind === "projectile") {
        s.icon.circle(0, 0, 8).fill(this.accent);
      }
      s.icon.alpha = isActive ? 1 : 0.65;
      s.key.alpha = ability !== undefined ? 0.8 : 0.35;
    });
  }

  private layout(): void {
    const width = BAR_SLOTS * SLOT + (BAR_SLOTS - 1) * GAP;
    this.root.position.set(
      Math.round((this.app.screen.width - width) / 2),
      Math.round(this.app.screen.height - 16 - SLOT),
    );
  }
}
