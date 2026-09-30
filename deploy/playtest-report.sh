#!/usr/bin/env bash
# A playtest's data from the game's journal, on the VPS, into one folder: the
# server's log of the window, its metrics as JSON Lines, and a summary for the
# report (Markdown: the network, the server, the game, each player's time in
# the world). See docs/playtests/.
#
#   deploy/playtest-report.sh "2026-10-02 19:00" "2026-10-02 19:35" [folder]
#
# The times are the VPS's local time, as journalctl takes them. Reading the
# journal takes the adm or systemd-journal group (the cloud image's user is in
# adm), or sudo.
set -euo pipefail

SINCE=${1:?"usage: $0 <since> <until> [folder]"}
UNTIL=${2:?"usage: $0 <since> <until> [folder]"}
OUT=$(realpath -m "${3:-playtest-$(date -d "$SINCE" +%Y%m%d-%H%M)}")
REPO=$(cd "$(dirname "$0")/.." && pwd)

mkdir -p "$OUT"
journalctl -u territory -o cat --since "$SINCE" --until "$UNTIL" > "$OUT/server.log"
sed -nE 's/^\[([^]]+)\] \[info\] metrics \{/{"time":"\1",/p' "$OUT/server.log" > "$OUT/metrics.jsonl"
(cd "$REPO/client" && npx tsx scripts/playtest_report.ts "$OUT/server.log") > "$OUT/summary.md"

echo "$OUT:"
echo "  server.log     the log of the window ($(wc -l < "$OUT/server.log") lines)"
echo "  metrics.jsonl  its metrics, a line a minute ($(wc -l < "$OUT/metrics.jsonl"))"
echo "  summary.md     the summary for the report"
