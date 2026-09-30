import { Application, Container, Graphics, Text } from "pixi.js";

import { FACTION_ONCE } from "../hint.js";
import { Button } from "./button.js";
import { CARD_H, CARD_W, cardAt, cardRow, choiceLayout, type CardRow } from "./cardRow.js";

const SELECTED = 0xe0b060; // gold rim, as the active ability slot
const BORDER = 0x3a3a46;

interface Card {
  id: number;
  root: Container;
  frame: Graphics;
  name: Text;
}

// Faction choice before the first spawn, one block in the middle of the screen:
// one sentence — the choice is once a season — above a row of cards (the
// faction's diamond badge and name), the selected card gold-rimmed, and «В бой»
// under them: into the world, at the chosen faction's capital.
// Shown only while the faction is still to be chosen (GameState.choosingFaction),
// never after a death. main routes canvas clicks through hit() first, so a click
// on a card selects instead of spawning.
export class FactionPicker {
  private readonly root = new Container();
  private readonly title = new Text({
    text: "ФРАКЦИЯ",
    style: { fill: "#9a9a88", fontFamily: "monospace", fontSize: 12, letterSpacing: 2 },
  });
  private readonly start = new Button("В бой");
  private readonly noteBack = new Graphics();
  private readonly note = new Text({
    text: FACTION_ONCE,
    style: { fill: "#f0e6c8", fontFamily: "monospace", fontSize: 16, fontWeight: "bold", align: "center", wordWrap: true },
  });
  private cards: Card[] = [];
  private selected = 0;
  private row: CardRow = cardRow(0, 0, 0);

  constructor(private readonly app: Application) {
    this.title.anchor.set(0.5, 1);
    this.note.anchor.set(0.5);
    this.root.addChild(this.title, this.noteBack, this.note, this.start.root);
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

  // What a canvas point is on: a faction's card or «В бой»; undefined off them or
  // while hidden.
  hit(sx: number, sy: number): { kind: "faction"; id: number } | { kind: "start" } | undefined {
    if (!this.root.visible) return undefined;
    if (this.start.hit(sx, sy)) return { kind: "start" };
    const id = this.cards[cardAt(this.row, sx, sy)]?.id;
    return id !== undefined ? { kind: "faction", id } : undefined;
  }

  private layout(): void {
    const { width, height } = this.app.screen;
    this.note.style.wordWrapWidth = Math.min(width - 64, 760); // wraps on a narrow screen
    const w = this.note.width + 32;
    const h = this.note.height + 20; // the note's box
    const { noteY, row, button } = choiceLayout(width, height, this.cards.length, h);
    this.row = row;
    this.start.place(button);
    this.note.position.set(Math.round(width / 2), Math.round(noteY));
    this.noteBack
      .clear()
      .roundRect(Math.round((width - w) / 2), Math.round(noteY - h / 2), w, h, 6)
      .fill({ color: 0x0a0a0f, alpha: 0.75 })
      .stroke({ width: 1, color: BORDER });
    this.cards.forEach((c, i) => c.root.position.set(row.x0 + i * (row.w + row.gap), row.y));
    this.title.position.set(Math.round(width / 2), row.y - 8);
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
