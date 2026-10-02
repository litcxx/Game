#!/usr/bin/env bash
# Makes this checkout the running game, on a VPS prepared by deploy/setup.sh. Run
# it as yourself: it builds here and asks sudo only to install. Builds the server
# (Release, unit tests first) and the client into a new release in
# /opt/territory/releases, points /opt/territory/current at it, restarts the
# service, and joins as a player to check. Keeps the last 3 releases.
#
#   git pull && deploy/deploy.sh    build and deploy this checkout
#   deploy/deploy.sh rollback       back to the release before the current one
#
# The world is saved on the restart (GAME-019, /var/lib/territory) and loaded by
# the new release: players reconnect by themselves, as the same characters.
set -euo pipefail

ROOT=/opt/territory
KEEP=3
PORT=27998
REPO=$(cd "$(dirname "$0")/.." && pwd)

step() { printf '\n== %s\n' "$*"; }

# Points `current` at releases/$1 in one step and restarts the game there; the
# server must take connections within 10 s.
switch_to() {
    sudo ln -sfn "releases/$1" "$ROOT/current.new"
    sudo mv -T "$ROOT/current.new" "$ROOT/current"
    sudo systemctl restart territory
    for _ in $(seq 50); do
        if (: > "/dev/tcp/127.0.0.1/$PORT") 2> /dev/null; then
            echo "running: $1"
            return
        fi
        sleep 0.2
    done
    echo "the server did not come up; see journalctl -u territory -n 50" >&2
    exit 1
}

if [ "${1:-}" = rollback ]; then
    current=$(basename "$(readlink "$ROOT/current" || true)")
    previous=$(ls "$ROOT/releases" | sort | grep -B1 -x "$current" | head -1 || true)
    if [ -z "$previous" ] || [ "$previous" = "$current" ]; then
        echo "no release to roll back to from '${current:-nothing deployed}'" >&2
        exit 1
    fi
    step "Rollback: $current -> $previous"
    switch_to "$previous"
    exit 0
fi

name=$(date -u +%Y%m%d-%H%M%S)-$(git -C "$REPO" rev-parse --short HEAD)
[ -z "$(git -C "$REPO" status --porcelain)" ] || name+=-dirty

step "Server: Release build, unit tests"
cd "$REPO/server"
cmake --preset release -G Ninja -DUSE_CLANG_FORMAT=OFF -DUSE_CLANG_TIDY=OFF
cmake --build --preset release
ctest --preset release

step "Client: build (it connects to wss://<its host>/ws)"
cd "$REPO/client"
npm ci
npm run build

step "Release $name"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
install -D -m 755 "$REPO/server/build/bin/server" "$stage/bin/server"
# The repo's config, with the server on 127.0.0.1: players reach it through Caddy only.
jq '.server.ip = "127.0.0.1" | .save.dir = "/var/lib/territory"' \
    "$REPO/server/config/config.json" > "$stage/config.json"
cp -r "$REPO/client/dist" "$stage/client"
chmod -R a+rX "$stage"
sudo cp -r "$stage" "$ROOT/releases/$name"
sudo chown -R root:root "$ROOT/releases/$name"
switch_to "$name"

step "Check: a player joins"
SERVER_URL="ws://127.0.0.1:$PORT/" npx tsx scripts/smoke.ts || {
    echo "a player could not join $name; back to the previous release: deploy/deploy.sh rollback" >&2
    exit 1
}

step "Old releases: keep $KEEP"
ls "$ROOT/releases" | sort | head -n -"$KEEP" | while read -r old; do
    sudo rm -rf "${ROOT:?}/releases/$old"
    echo "removed $old"
done
