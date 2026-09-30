import { Application, Container, Graphics, Text } from "pixi.js";

interface Gauge {
  root: Container;
  fill: Graphics; // liquid level (masked to the circle)
  value: Text;
  label: Text;
  radius: number;
}

const mono = (size: number, fill: string) => ({ fill, fontFamily: "monospace", fontSize: size });

// px from the screen's bottom: the bottom row — the faction cards, or the ability
// bar while alive — sits above the hint line, which the capture hint uses in play.
export const ABOVE_HINT_LINE = 56;

// Corner HUD: faction badge + territory (top-left), online · ping + FPS (top-right),
// HP gauge (bottom-left), capture gauge (bottom-right), centered hint line.
export class Hud {
  private readonly factionBadge = new Graphics();
  private readonly factionName = new Text({ text: "", style: mono(16, "#e6e6e6") });
  private readonly territory = new Text({ text: "", style: mono(12, "#9a9a88") });
  private readonly online = new Text({ text: "", style: mono(13, "#9a9a88") });
  private readonly fps = new Text({ text: "FPS —", style: mono(13, "#8fce8f") });
  private readonly hint = new Text({ text: "", style: mono(14, "#c8c8b8") });
  private readonly hp: Gauge;
  private readonly cell: Gauge;

  constructor(private readonly app: Application) {
    this.online.anchor.set(1, 0);
    this.fps.anchor.set(1, 0);
    this.hint.anchor.set(0.5, 1);

    this.hp = this.makeGauge(50, "ЗДОРОВЬЕ", "#c88868");
    this.cell = this.makeGauge(50, "", "#9a9a88");

    app.stage.addChild(
      this.factionBadge,
      this.factionName,
      this.territory,
      this.online,
      this.fps,
      this.hint,
      this.hp.root,
      this.cell.root,
    );

    this.layout();
    app.renderer.on("resize", () => this.layout());
  }

  setFaction(name: string, color: number): void {
    const s = 9;
    this.factionBadge
      .clear()
      .moveTo(0, -s)
      .lineTo(s, 0)
      .lineTo(0, s)
      .lineTo(-s, 0)
      .closePath()
      .fill(color)
      .stroke({ width: 1, color: 0xffffff, alpha: 0.4 });
    this.factionName.text = name;
  }

  setTerritory(cells: number, percent: number): void {
    this.territory.text = `${cells} клеток · ${percent}% карты`;
  }

  // "В СЕТИ n · ПИНГ m" (the round trip in ms, as in the concept); just the
  // online count until the first Pong.
  setNetwork(online: number, rttMs: number | undefined): void {
    this.online.text = rttMs === undefined ? `В СЕТИ ${online}` : `В СЕТИ ${online} · ПИНГ ${rttMs}`;
  }

  setFps(n: number): void {
    this.fps.text = `FPS ${n}`;
  }

  setHint(text: string): void {
    this.hint.text = text;
    this.hint.visible = text.length > 0;
  }

  setHp(hp: number, maxHp: number, alive: boolean): void {
    this.hp.root.visible = alive;
    if (alive) this.updateGauge(this.hp, hp / maxHp, 0xd0583a, String(hp));
  }

  // The cell under the player: shown full in its owner's colour, or the capturing
  // faction's colour rising by % while a capture is in progress.
  setCell(
    index: number,
    ownerColor: number | undefined,
    captureColor: number | undefined,
    capturePercent: number,
  ): void {
    this.cell.root.visible = true;
    this.cell.label.text = `Клетка ${index}`;
    if (capturePercent > 0 && captureColor !== undefined) {
      this.updateGauge(this.cell, capturePercent / 100, captureColor, `${capturePercent}%`);
    } else if (ownerColor !== undefined) {
      this.updateGauge(this.cell, 1, ownerColor, "");
    } else {
      this.updateGauge(this.cell, 0, 0x000000, "");
    }
  }

  hideCell(): void {
    this.cell.root.visible = false;
  }

  private makeGauge(radius: number, label: string, labelColor: string): Gauge {
    const root = new Container();
    const bg = new Graphics().circle(0, 0, radius).fill(0x14141a);
    const fill = new Graphics();
    const maskG = new Graphics().circle(0, 0, radius).fill(0xffffff);
    fill.mask = maskG;
    const border = new Graphics().circle(0, 0, radius).stroke({ width: 3, color: 0x3a3a46 });
    const value = new Text({
      text: "",
      style: {
        fill: "#f4f4ec",
        fontFamily: "monospace",
        fontWeight: "bold",
        fontSize: 20,
        stroke: { color: 0x14141a, width: 3 }, // dark outline -> readable over the fill
      },
    });
    value.anchor.set(0.5);
    const labelText = new Text({ text: label, style: mono(13, labelColor) });
    labelText.anchor.set(0.5, 1);
    labelText.position.set(0, -radius - 5);
    root.addChild(bg, fill, maskG, border, value, labelText);
    return { root, fill, value, label: labelText, radius };
  }

  private updateGauge(g: Gauge, frac: number, color: number, value: string): void {
    const f = Math.max(0, Math.min(1, frac));
    const r = g.radius;
    g.fill.clear();
    if (f > 0) g.fill.rect(-r, r - f * 2 * r, 2 * r, f * 2 * r).fill({ color, alpha: 0.85 });
    g.value.text = value;
  }

  private layout(): void {
    const w = this.app.screen.width;
    const h = this.app.screen.height;
    this.factionBadge.position.set(20, 22);
    this.factionName.position.set(38, 12);
    this.territory.position.set(38, 34);
    this.online.position.set(w - 16, 12);
    this.fps.position.set(w - 16, 32);
    this.hint.position.set(w / 2, h - 16);
    this.hp.root.position.set(16 + this.hp.radius, h - 16 - this.hp.radius);
    this.cell.root.position.set(w - 16 - this.cell.radius, h - 16 - this.cell.radius);
  }
}
