// Pure checks for the playtest summary (scripts/playtestReport.ts): from the
// server's log of a playtest window to the numbers of the report — the
// network, the server, the game, and each player's time in the world. Run:
//   npx tsx scripts/playtest_report_check.ts
import { summarize, toMarkdown } from "./playtestReport.js";

let failures = 0;
const check = (name: string, cond: boolean, detail = "") => {
  console.log(`${cond ? "ok  " : "FAIL"} ${name}${detail ? `  [${detail}]` : ""}`);
  if (!cond) failures++;
};
const same = (a: unknown, b: unknown) => JSON.stringify(a) === JSON.stringify(b);

const metrics = (fields: Record<string, unknown>) =>
  JSON.stringify({
    period_s: 60,
    ccu: 0,
    ticks: 3600,
    tick_us: { avg: 200, p99: 900, max: 1500 },
    snapshot_ticks: 1200,
    snapshot_tick_us: { avg: 300, p99: 1000, max: 1500 },
    snapshots: 2400,
    snapshot_bytes_avg: 40,
    drops: 0,
    resyncs: 0,
    closed_behind: 0,
    joins: 0,
    leaves: 0,
    resumes: 0,
    rtt_ms: { p50: 0, p95: 0, max: 0 },
    corrections: { count: 0, max: 0 },
    spawns: 0,
    deaths: 0,
    captures: 0,
    errors: {},
    ...fields,
  });

// Ann joins, spawns after 20 s, drops for a moment and resumes. Bob (with a
// quote in his name) joins, drops, leaves after the grace, comes back.
const log = [
  "[2026-10-02 19:00:05.000] [info] Server ip=127.0.0.1 port=27998 io_threads=4",
  "[2026-10-02 19:00:10.000] [info] World::on_hello session=1 name='Ann' -> player_id=1",
  "[2026-10-02 19:00:20.000] [info] World::on_hello session=2 name='B'ob' -> player_id=2",
  "[2026-10-02 19:00:30.000] [info] World::on_spawn session=1 player_id=1 cell=5050 faction=1",
  "[2026-10-02 19:00:40.000] [info] World::on_spawn session=2 player_id=2 cell=5051 faction=2",
  `[2026-10-02 19:01:00.000] [info] metrics ${metrics({ ccu: 2, joins: 2, spawns: 2, rtt_ms: { p50: 40, p95: 90, max: 120 }, corrections: { count: 3, max: 12 }, captures: 1 })}`,
  "[2026-10-02 19:01:10.000] [info] World::on_disconnect session=1 player_id=1: away for 1800 ticks",
  "[2026-10-02 19:01:12.000] [info] World::on_hello session=3 resumed player_id=1 name='Ann'",
  "[2026-10-02 19:01:20.000] [info] World::on_disconnect session=2 player_id=2: away for 1800 ticks",
  "[2026-10-02 19:01:50.000] [info] World: player_id=2 left the world: its reconnect grace is over",
  `[2026-10-02 19:02:00.000] [info] metrics ${metrics({ ccu: 1, leaves: 1, resumes: 1, deaths: 2, captures: 4, rtt_ms: { p50: 60, p95: 150, max: 300 }, corrections: { count: 1, max: 30 }, tick_us: { avg: 250, p99: 2100, max: 4000 }, errors: { SPAWN_TOO_EARLY: 2 } })}`,
  "[2026-10-02 19:02:30.000] [info] World::on_hello session=4 returned as player_id=2 name='B'ob'",
  `[2026-10-02 19:03:00.000] [info] metrics ${metrics({ ccu: 2, joins: 1, rtt_ms: { p50: 50, p95: 100, max: 110 }, corrections: { count: 2, max: 8 }, drops: 1, resyncs: 1, errors: { SPAWN_TOO_EARLY: 1, RATE_LIMITED: 1 } })}`,
  "[2026-10-02 19:03:30.000] [info] World::on_spawn session=4 player_id=2 cell=5052 faction=2",
].join("\n");

const s = summarize(log);

check("the window: first to last line, whole minutes of metrics", s.from === "2026-10-02 19:00:05" && s.to === "2026-10-02 19:03:30" && s.minutes === 3, `${s.from}..${s.to} ${s.minutes}`);
check("players: everyone who joined; CCU peak and average", s.players === 2 && s.peakCcu === 2 && Math.abs(s.avgCcu - 5 / 3) < 1e-9);
check("round trips: the median minute's p50, the worst minute's p95, the max", same(s.rtt, { p50: 50, p95: 150, max: 300 }));
check("corrections: in all, per player-minute (5 player-minutes), the largest", s.corrections.total === 6 && Math.abs(s.corrections.perPlayerMinute - 6 / 5) < 1e-9 && s.corrections.max === 30);
check("the server: the worst tick p99 and max, the average snapshot", s.tick.p99Us === 2100 && s.tick.maxUs === 4000 && s.snapshotBytesAvg === 40);
check("delivery and presence, summed", same(s.delivery, { drops: 1, resyncs: 1, closedBehind: 0 }) && same(s.presence, { joins: 3, leaves: 1, resumes: 1 }));
check("the game, summed", same(s.game, { spawns: 2, deaths: 2, captures: 5 }));
check("errors by code, summed", same(s.errors, { RATE_LIMITED: 1, SPAWN_TOO_EARLY: 3 }));
check("a row per minute", s.perMinute.length === 3 && s.perMinute[1]?.time === "19:02" && s.perMinute[1]?.captures === 4 && s.perMinute[1]?.rttP95 === 150);

const ann = s.sessions.find((p) => p.name === "Ann");
const bob = s.sessions.find((p) => p.id === 2);
check("Ann: in the world from 19:00:10 to the end, one drop, resumed", ann !== undefined && ann.minutesInWorld === 3.33 && ann.disconnects === 1 && ann.resumes === 1 && ann.returns === 0, JSON.stringify(ann));
check("Ann: spawned 20 s after joining", ann?.firstSpawnAfterS === 20);
check("Bob: a quote in the name; in the world twice (19:00:20–19:01:50, 19:02:30–end)", bob?.name === "B'ob" && bob.minutesInWorld === 2.5 && bob.disconnects === 1 && bob.returns === 1, JSON.stringify(bob));

const md = toMarkdown(s);
check("the Markdown: the key numbers and every player", ["| 50 |", "| 150 |", "B'ob", "Ann", "SPAWN_TOO_EARLY", "19:02"].every((x) => md.includes(x)), md.slice(0, 200));
check("an empty log: no crash, nothing to report", summarize("").players === 0 && toMarkdown(summarize("")).length > 0);

console.log(failures === 0 ? "VERDICT: PASS" : `VERDICT: FAIL (${failures})`);
process.exit(failures === 0 ? 0 : 1);
