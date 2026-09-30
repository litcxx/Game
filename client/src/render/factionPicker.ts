import { Application, Container, Graphics, Text } from "pixi.js";

import { FACTION_ONCE } from "../hint.js";
import { CARD_H, CARD_W, cardAt, cardRow, type CardRow } from "./cardRow.js";
import { ABOVE_HINT_LINE } from "./hud.js";

const SELECTED = 0xe0b060; // gold rim, as the active ability slot
const BORDER = 0x3a3a46;

interface Card {
  id: number;
  root: Container;
  frame: Graphics;
  name: Text;
}

// Faction choice before the first spawn: a centred row of cards — the faction's
// diamond badge and name — above the hint line, the selected card gold-rimmed,
// and in the middle of the screen one sentence: the choice is once a season.
// Shown only while the faction is still to be chosen (GameState.choosingFaction),
// never after a death. main routes canvas clicks through pick() first, so a click
// on a card selects instead of spawning.
export class FactionPicker {
  private readonly root = new Container();
  private readonly title = new Text({
    text: "ФРАКЦИЯ",
    style: { fill: "#9a9a88", fontFamily: "monospace", fontSize: 12, letterSpacing: 2 },
  });
  private readonly noteBack = new Graphics();
  private readonly note = new Text({
    text: FACTION_ONCE,
    style: { fill: "#f0e6c8", fontFamily: "monospace", fontSize: 16, fontWeight: "bold", align: "center", wordWrap: true },
  });
  private cards: Card[] = [];
  private selected = 0;
  private row: CardRow = cardRow(0, 0, 0, ABOVE_HINT_LINE);

  constructor(private readonly app: Application) {
    this.title.anchor.set(0.5, 1);
    this.note.anchor.set(0.5);
    this.root.addChild(this.title, this.noteBack, this.note);
    app.stage.addChild(this.root);
    app.renderer.on("resize", () => this.layout());
  }

  setFactions(factions: readonly { id: number; name: string; color: number }[]): void {
    for (const c of this.cards) c.root.destroy({ children: true });
    this.cards = factions.map((f) => {
      const root = new Container();
      const frame = new Graphics();
      const s = 9;
      const badge = new Graphics()
        .moveTo(0, -s)
        .lineTo(s, 0)
        .lineTo(0, s)
        .lineTo(-s, 0)
        .closePath()
        .fill(f.color)
        .stroke({ width: 1, color: 0xffffff, alpha: 0.4 });
      badge.position.set(22, CARD_H / 2);
      const name = new Text({ text: f.name, style: { fill: "#e6e6e6", fontFamily: "monospace", fontSize: 14 } });
      name.anchor.set(0, 0.5);
      name.position.set(40, Math.round(CARD_H / 2));
      root.addChild(frame, badge, name);
      this.root.addChild(root);
      return { id: f.id, root, frame, name };
    });
    this.layout();
    this.redraw();
  }

  setSelected(id: number): void {
    this.selected = id;
    this.redraw();
  }

  setVisible(visible: boolean): void {
    this.root.visible = visible;
  }

  // Faction id of the card under a canvas point; undefined off the cards or while hidden.
  pick(sx: number, sy: number): number | undefined {
    if (!this.root.visible) return undefined;
    const i = cardAt(this.row, sx, sy);
    return i >= 0 ? this.cards[i]?.id : undefined;
  }

  private layout(): void {
    this.row = cardRow(this.app.screen.width, this.app.screen.height, this.cards.length, ABOVE_HINT_LINE);
    this.cards.forEach((c, i) => c.root.position.set(this.row.x0 + i * (this.row.w + this.row.gap), this.row.y));
    this.title.position.set(Math.round(this.app.screen.width / 2), this.row.y - 8);

    const { width, height } = this.app.screen;
    this.note.style.wordWrapWidth = Math.min(width - 64, 760); // wraps on a narrow screen
    this.note.position.set(Math.round(width / 2), Math.round(height / 2));
    const w = this.note.width + 32;
    const h = this.note.height + 20;
    this.noteBack
      .clear()
      .roundRect(Math.round((width - w) / 2), Math.round((height - h) / 2), w, h, 6)
      .fill({ color: 0x0a0a0f, alpha: 0.75 })
      .stroke({ width: 1, color: BORDER });
  }

  private redraw(): void {
    for (const c of this.cards) {
      const on = c.id === this.selected;
      c.frame.clear();
      if (on) c.frame.roundRect(-4, -4, CARD_W + 8, CARD_H + 8, 7).fill({ color: SELECTED, alpha: 0.12 });
      c.frame
        .roundRect(0, 0, CARD_W, CARD_H, 4)
        .fill({ color: 0x0a0a0f, alpha: 0.8 })
        .stroke({ width: on ? 2 : 1, color: on ? SELECTED : BORDER });
      c.name.alpha = on ? 1 : 0.7;
    }
  }
}
