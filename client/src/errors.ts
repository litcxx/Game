import { ErrorCode } from "./gen/game/v1/protocol_pb.js";

// Server errors as the player sees them. ServerError.detail is for the logs; the
// text shown comes from the code. A fatal error (the server then closes the
// socket with 4000 + code) stays on screen; a refused request shows for a while.

export const NOTICE_MS = 3000; // how long a refusal (e.g. a spawn) stays on the hint line

const TEXTS: Partial<Record<ErrorCode, string>> = {
  [ErrorCode.PROTOCOL_VERSION]: "Версия игры устарела — перезагрузите страницу",
  [ErrorCode.BAD_MESSAGE]: "Сервер не смог прочитать сообщение — перезагрузите страницу",
  [ErrorCode.UNEXPECTED_MESSAGE]: "Нарушен обмен с сервером — перезагрузите страницу",
  [ErrorCode.HANDSHAKE_TIMEOUT]: "Сервер не дождался входа — перезагрузите страницу",
  [ErrorCode.IDLE_TIMEOUT]: "Соединение закрыто: долго не было связи",
  [ErrorCode.RATE_LIMITED]: "Слишком много сообщений серверу",
  [ErrorCode.INVALID_NAME]: "Недопустимое имя: 1–16 видимых символов",
  [ErrorCode.SERVER_FULL]: "Сервер заполнен, попробуйте позже",
  [ErrorCode.SESSION_REPLACED]: "Игра открыта в другой вкладке",
  [ErrorCode.SERVER_SHUTDOWN]: "Сервер остановлен",
  [ErrorCode.KICKED]: "Вы отключены от сервера",
  [ErrorCode.INTERNAL]: "Внутренняя ошибка сервера",
  [ErrorCode.UNSUPPORTED_MESSAGE]: "Сервер не поддерживает этот запрос",
  [ErrorCode.SPAWN_INVALID_CELL]: "Эта клетка вне карты",
  [ErrorCode.SPAWN_FORBIDDEN]: "Здесь появиться нельзя",
  [ErrorCode.SPAWN_TOO_EARLY]: "Возрождение ещё не готово",
  [ErrorCode.ALREADY_SPAWNED]: "Вы уже в игре",
  [ErrorCode.INVALID_FACTION]: "Эта фракция недоступна",
};

// What to tell the player about an error code.
export function errorText(code: ErrorCode): string {
  return TEXTS[code] ?? `Ошибка сервера (код ${code})`;
}

// The error a WebSocket close code carries (4000 + ErrorCode), if any.
export function closeError(closeCode: number): ErrorCode | undefined {
  const code = closeCode - 4000;
  return closeCode > 4000 && code in ErrorCode ? (code as ErrorCode) : undefined;
}

// What the hint line shows: a fatal error for good (the first one — the close
// code follows the error frame), else a refusal for NOTICE_MS, else the usual.
export class Notices {
  private fatal: string | undefined;
  private notice: { text: string; untilMs: number } | undefined;

  get failed(): boolean {
    return this.fatal !== undefined;
  }

  fail(text: string): void {
    this.fatal ??= text;
  }

  refuse(text: string, nowMs: number): void {
    this.notice = { text, untilMs: nowMs + NOTICE_MS };
  }

  hint(usual: string, nowMs: number): string {
    if (this.fatal !== undefined) return this.fatal;
    if (this.notice !== undefined && nowMs < this.notice.untilMs) return this.notice.text;
    return usual;
  }
}
