// A playtest's numbers from the server's log of its window (journalctl -o cat,
// see deploy/playtest-report.sh): the minutely `metrics` lines summed up, and
// each player's time in the world from the join / drop / leave lines. For the
// report in docs/playtests/.

export interface PlayerSession {
  id: number;
  name: string;
  enteredAt: string; // HH:MM:SS of the first join
  minutesInWorld: number; // to 0.01; a dropped character stays in the world for the grace
  firstSpawnAfterS: number | undefined; // from the first join
  disconnects: number;
  resumes: number; // back within the grace
  returns: number; // back after it
}

export interface MinuteRow {
  time: string; // HH:MM of the metrics line
  ccu: number;
  rttP50: number;
  rttP95: number;
  corrections: number;
  correctionMax: number;
  deaths: number;
  captures: number;
  tickP99Us: number;
}

export interface PlaytestSummary {
  from: string;
  to: string;
  minutes: number; // of metrics
  players: number; // who joined
  peakCcu: number;
  avgCcu: number;
  rtt: { p50: number; p95: number; max: number }; // the median minute's p50, the worst minute's p95, the max
  corrections: { total: number; perPlayerMinute: number; max: number };
  tick: { p99Us: number; maxUs: number }; // the worst minute
  snapshotBytesAvg: number;
  delivery: { drops: number; resyncs: number; closedBehind: number };
  presence: { joins: number; leaves: number; resumes: number };
  game: { spawns: number; deaths: number; captures: number };
  errors: Record<string, number>; // by code, sorted
  perMinute: MinuteRow[];
  sessions: PlayerSession[]; // by id
}

// The fields of a metrics line this summary reads (server/README.md).
interface MetricsLine {
  period_s: number;
  ccu: number;
  tick_us: { p99: number; max: number };
  snapshots: number;
  snapshot_bytes_avg: number;
  drops: number;
  resyncs: number;
  closed_behind: number;
  joins: number;
  leaves: number;
  resumes: number;
  rtt_ms?: { p50: number; p95: number; max: number };
  corrections?: { count: number; max: number };
  spawns?: number;
  deaths?: number;
  captures?: number;
  errors: Record<string, number>;
}

const LINE = /^\[(\d{4}-\d\d-\d\d) (\d\d:\d\d:\d\d)\.\d+\] \[\w+\] (.*)$/;
const JOINED = /^World::on_hello session=\d+ name='(.*)' -> player_id=(\d+)$/;
const CAME_BACK = /^World::on_hello session=\d+ (resumed|returned as) player_id=(\d+) name='(.*)'$/;
const SPAWNED = /^World::on_spawn session=\d+ player_id=(\d+) /;
const DROPPED = /^World::on_disconnect session=\d+ player_id=(\d+):/;
const LEFT = /^World: player_id=(\d+) left the world/;

interface Tracked {
  session: PlayerSession;
  firstJoinMs: number;
  inWorldSinceMs: number | undefined;
  inWorldMs: number;
}

const sum = (xs: number[]) => xs.reduce((a, b) => a + b, 0);
const round2 = (x: number) => Math.round(x * 100) / 100;
// The p-th percentile by nearest rank; 0 for none.
const nearestRank = (xs: number[], p: number) => {
  if (xs.length === 0) return 0;
  const sorted = [...xs].sort((a, b) => a - b);
  return sorted[Math.max(1, Math.ceil((p / 100) * sorted.length)) - 1]!;
};

export function summarize(log: string): PlaytestSummary {
  const minutes: { time: string; m: MetricsLine }[] = [];
  const players = new Map<number, Tracked>();
  let from = "";
  let to = "";
  let lastMs = 0;

  const track = (id: number, name: string, atMs: number, at: string): Tracked => {
    let t = players.get(id);
    if (!t) {
      t = {
        session: {
          id,
          name,
          enteredAt: at,
          minutesInWorld: 0,
          firstSpawnAfterS: undefined,
          disconnects: 0,
          resumes: 0,
          returns: 0,
        },
        firstJoinMs: atMs,
        inWorldSinceMs: undefined,
        inWorldMs: 0,
      };
      players.set(id, t);
    }
    t.session.name = name;
    return t;
  };

  for (const raw of log.split("\n")) {
    const line = LINE.exec(raw);
    if (!line) continue;
    const [, date, time, text] = line as unknown as [string, string, string, string];
    const atMs = Date.parse(`${date}T${time}`);
    if (!from) from = `${date} ${time}`;
    to = `${date} ${time}`;
    lastMs = atMs;

    let m: RegExpExecArray | null;
    if (text.startsWith("metrics {")) {
      minutes.push({ time: time.slice(0, 5), m: JSON.parse(text.slice("metrics ".length)) as MetricsLine });
    } else if ((m = JOINED.exec(text))) {
      const t = track(Number(m[2]), m[1]!, atMs, time);
      t.inWorldSinceMs ??= atMs;
    } else if ((m = CAME_BACK.exec(text))) {
      const t = track(Number(m[2]), m[3]!, atMs, time);
      if (m[1] === "resumed") {
        t.session.resumes++;
      } else {
        t.session.returns++;
        t.inWorldSinceMs ??= atMs;
      }
    } else if ((m = SPAWNED.exec(text))) {
      const t = players.get(Number(m[1]));
      if (t && t.session.firstSpawnAfterS === undefined) {
        t.session.firstSpawnAfterS = (atMs - t.firstJoinMs) / 1000;
      }
    } else if ((m = DROPPED.exec(text))) {
      const t = players.get(Number(m[1]));
      if (t) t.session.disconnects++;
    } else if ((m = LEFT.exec(text))) {
      const t = players.get(Number(m[1]));
      if (t && t.inWorldSinceMs !== undefined) {
        t.inWorldMs += atMs - t.inWorldSinceMs;
        t.inWorldSinceMs = undefined;
      }
    }
  }

  const sessions = [...players.values()]
    .map((t) => {
      const open = t.inWorldSinceMs === undefined ? 0 : lastMs - t.inWorldSinceMs;
      return { ...t.session, minutesInWorld: round2((t.inWorldMs + open) / 60000) };
    })
    .sort((a, b) => a.id - b.id);

  const ms = minutes.map((x) => x.m);
  const playerMinutes = sum(ms.map((m) => (m.ccu * m.period_s) / 60));
  const correctionsTotal = sum(ms.map((m) => m.corrections?.count ?? 0));
  const snapshots = sum(ms.map((m) => m.snapshots));
  const errors: Record<string, number> = {};
  for (const m of ms) for (const [code, n] of Object.entries(m.errors)) errors[code] = (errors[code] ?? 0) + n;

  return {
    from,
    to,
    minutes: sum(ms.map((m) => m.period_s)) / 60,
    players: players.size,
    peakCcu: Math.max(0, ...ms.map((m) => m.ccu)),
    avgCcu: ms.length === 0 ? 0 : sum(ms.map((m) => m.ccu)) / ms.length,
    rtt: {
      p50: nearestRank(ms.map((m) => m.rtt_ms?.p50 ?? 0).filter((x) => x > 0), 50),
      p95: Math.max(0, ...ms.map((m) => m.rtt_ms?.p95 ?? 0)),
      max: Math.max(0, ...ms.map((m) => m.rtt_ms?.max ?? 0)),
    },
    corrections: {
      total: correctionsTotal,
      perPlayerMinute: playerMinutes === 0 ? 0 : correctionsTotal / playerMinutes,
      max: Math.max(0, ...ms.map((m) => m.corrections?.max ?? 0)),
    },
    tick: {
      p99Us: Math.max(0, ...ms.map((m) => m.tick_us.p99)),
      maxUs: Math.max(0, ...ms.map((m) => m.tick_us.max)),
    },
    snapshotBytesAvg:
      snapshots === 0 ? 0 : Math.round(sum(ms.map((m) => m.snapshot_bytes_avg * m.snapshots)) / snapshots),
    delivery: {
      drops: sum(ms.map((m) => m.drops)),
      resyncs: sum(ms.map((m) => m.resyncs)),
      closedBehind: sum(ms.map((m) => m.closed_behind)),
    },
    presence: {
      joins: sum(ms.map((m) => m.joins)),
      leaves: sum(ms.map((m) => m.leaves)),
      resumes: sum(ms.map((m) => m.resumes)),
    },
    game: {
      spawns: sum(ms.map((m) => m.spawns ?? 0)),
      deaths: sum(ms.map((m) => m.deaths ?? 0)),
      captures: sum(ms.map((m) => m.captures ?? 0)),
    },
    errors: Object.fromEntries(Object.entries(errors).sort(([a], [b]) => a.localeCompare(b))),
    perMinute: minutes.map(({ time, m }) => ({
      time,
      ccu: m.ccu,
      rttP50: m.rtt_ms?.p50 ?? 0,
      rttP95: m.rtt_ms?.p95 ?? 0,
      corrections: m.corrections?.count ?? 0,
      correctionMax: m.corrections?.max ?? 0,
      deaths: m.deaths ?? 0,
      captures: m.captures ?? 0,
      tickP99Us: m.tick_us.p99,
    })),
    sessions,
  };
}

const cell = (x: string | number | undefined) => String(x ?? "—").replaceAll("|", "\\|");
const row = (xs: (string | number | undefined)[]) => `| ${xs.map(cell).join(" | ")} |`;
const fixed1 = (x: number) => (Math.round(x * 10) / 10).toString();

// The summary as Markdown, for the report (in Russian, as the report is).
export function toMarkdown(s: PlaytestSummary): string {
  const out = [
    `## Сводка: ${s.from || "—"} — ${s.to || "—"} (${s.minutes} мин метрик)`,
    "",
    "| Показатель | Значение |",
    "|---|---|",
    row(["Игроков вошло", s.players]),
    row(["CCU: пик / среднее", `${s.peakCcu} / ${fixed1(s.avgCcu)}`]),
    row(["RTT p50 (медиана минут), мс", s.rtt.p50]),
    row(["RTT p95 (худшая минута), мс", s.rtt.p95]),
    row(["RTT max, мс", s.rtt.max]),
    row([
      "Коррекции предсказания: всего / на игрока в минуту",
      `${s.corrections.total} / ${fixed1(s.corrections.perPlayerMinute)}`,
    ]),
    row(["Самая большая коррекция, единиц", s.corrections.max]),
    row(["Тик p99 (худшая минута) / max, мкс", `${s.tick.p99Us} / ${s.tick.maxUs}`]),
    row(["Снапшот в среднем, байт", s.snapshotBytesAvg]),
    row(["Спавны / смерти / захваты клеток", `${s.game.spawns} / ${s.game.deaths} / ${s.game.captures}`]),
    row(["Входы / уходы после грейса / возвраты в грейс", `${s.presence.joins} / ${s.presence.leaves} / ${s.presence.resumes}`]),
    row(["Отказы очереди / resync / закрыто за отставание", `${s.delivery.drops} / ${s.delivery.resyncs} / ${s.delivery.closedBehind}`]),
    "",
    "### Ошибки",
    "",
    "| Код | Сколько |",
    "|---|---|",
    ...Object.entries(s.errors).map(([code, n]) => row([code, n])),
    "",
    "### По минутам",
    "",
    "| Время | CCU | RTT p50 | RTT p95 | Коррекции | Макс. коррекция | Смерти | Захваты | Тик p99, мкс |",
    "|---|---|---|---|---|---|---|---|---|",
    ...s.perMinute.map((m) =>
      row([m.time, m.ccu, m.rttP50, m.rttP95, m.corrections, m.correctionMax, m.deaths, m.captures, m.tickP99Us]),
    ),
    "",
    "### Игроки",
    "",
    "| id | Ник | Вошёл | В мире, мин | До первого спавна, с | Обрывов | Вернулся в грейс | Вернулся после |",
    "|---|---|---|---|---|---|---|---|",
    ...s.sessions.map((p) =>
      row([p.id, p.name, p.enteredAt, p.minutesInWorld, p.firstSpawnAfterS, p.disconnects, p.resumes, p.returns]),
    ),
    "",
  ];
  return out.join("\n");
}
