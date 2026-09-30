// The playtest summary (Markdown) from the server's log of its window — a file,
// or stdin. deploy/playtest-report.sh runs it on the VPS:
//   npx tsx scripts/playtest_report.ts server.log > summary.md
import { readFileSync } from "node:fs";

import { summarize, toMarkdown } from "./playtestReport.js";

const log = readFileSync(process.argv[2] ?? 0, "utf8");
process.stdout.write(toMarkdown(summarize(log)));
