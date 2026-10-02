#!/usr/bin/env bash
# The smoke suite's server config, printed: the shipped config/config.json — the
# rules — with config/smoke-map.json's factions and capitals, one per smoke
# script role (where its players spawn: at their capital, GAME-016), saving to
# build/smoke-saves. Start it --fresh: the scripts expect a new world.
#   ./scripts/smoke-config.sh > build/smoke-config.json
#   ./build/bin/server build/smoke-config.json --fresh
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
jq --slurpfile map "$here/config/smoke-map.json" \
  '.game.factions = $map[0].factions | .game.capitals = $map[0].capitals
   | .save.dir = "build/smoke-saves"' \
  "$here/config/config.json"
