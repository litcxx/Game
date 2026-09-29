import type { Modal } from "./modal.js";

// The reconnect dialog's text: when the next attempt is, and which one it is.
export function reconnectText(attempt: number, secondsLeft: number): string {
  const next = secondsLeft > 0 ? `Новая попытка через ${secondsLeft} с` : "Подключаемся…";
  return `Связь с сервером потеряна.\n${next} (попытка ${attempt}).`;
}

export interface IntervalClock {
  setInterval(fn: () => void, ms: number): unknown;
  clearInterval(handle: unknown): void;
}

// What the player sees of the connection in a modal: waiting to reconnect (a
// countdown, nothing to press) or stopped for good (why, and a reload).
export class ConnectionDialogs {
  private countdown: unknown;

  constructor(
    private readonly modal: Modal,
    private readonly clock: IntervalClock,
  ) {}

  reconnecting(attempt: number, retryInMs: number): void {
    this.stopCountdown();
    let secondsLeft = Math.ceil(retryInMs / 1000);
    const show = () => this.modal.open({ title: "Переподключение…", text: reconnectText(attempt, secondsLeft) });
    show();
    this.countdown = this.clock.setInterval(() => {
      secondsLeft--;
      show();
      if (secondsLeft <= 0) this.stopCountdown();
    }, 1000);
  }

  failed(text: string, onReload: () => void): void {
    this.stopCountdown();
    this.modal.open({ title: "Игра остановлена", text, buttons: [{ label: "Перезагрузить", onClick: onReload }] });
  }

  close(): void {
    this.stopCountdown();
    this.modal.close();
  }

  private stopCountdown(): void {
    if (this.countdown === undefined) return;
    this.clock.clearInterval(this.countdown);
    this.countdown = undefined;
  }
}
