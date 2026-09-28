// Runs every pure check (`check`) or every end-to-end smoke (`smoke` — start the
// server first) one after another, by exit code, and fails if any failed. The
// same command locally and in CI:
//   npm run check     # scripts/*_check.ts
//   npm run smoke     # scripts/smoke.ts, then scripts/*_smoke.ts
import { spawnSync } from "node:child_process";
import { readdirSync } from "node:fs";
import { fileURLToPath } from "node:url";

const kind = process.argv[2];
if (kind !== "check" && kind !== "smoke") {
  console.error("usage: tsx scripts/run_all.ts check|smoke");
  process.exit(2);
}

const dir = fileURLToPath(new URL(".", import.meta.url));
const scripts = readdirSync(dir)
  .filter((f) => f.endsWith(`_${kind}.ts`))
  .sort();
if (kind === "smoke") scripts.unshift("smoke.ts"); // the plainest first: connect + Welcome

const results: { script: string; ok: boolean; seconds: number }[] = [];
for (const script of scripts) {
  console.log(`\n=== ${script}`);
  const started = performance.now();
  const run = spawnSync(process.execPath, ["--import", "tsx", `${dir}${script}`], { stdio: "inherit" });
  results.push({ script, ok: run.status === 0, seconds: (performance.now() - started) / 1000 });
}

console.log(`\n=== ${kind}: ${results.filter((r) => r.ok).length}/${results.length} passed`);
for (const r of results) console.log(`${r.ok ? "ok  " : "FAIL"} ${r.script} (${r.seconds.toFixed(1)} s)`);
process.exit(results.every((r) => r.ok) ? 0 : 1);
