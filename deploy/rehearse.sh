#!/usr/bin/env bash
# Rehearses docs/ops.md on a fresh Ubuntu 24.04 in Docker, systemd and all, with
# the domain `localhost` (Caddy's own CA instead of Let's Encrypt), and checks
# what the deployment promises:
#   setup.sh on a clean machine, then deploy.sh from a checkout of this tree;
#   the page over https, the game over wss through Caddy (join, resume);
#   the server reachable only through Caddy (it listens on 127.0.0.1);
#   check.sh: kill -9 -> back within 10 s;
#   a deploy with a player online: the player is back in the game by itself, as
#   the same character (the world saved on the restart and loaded);
#   the last 3 releases kept; rollback; setup.sh again changes nothing.
# Needs Linux with Docker that runs privileged containers (systemd as PID 1). The
# container shares the host's network: ports 80, 443 and 27998 must be free.
#   deploy/rehearse.sh
# REHEARSE_BASE_IMAGE (default ubuntu:24.04) is the machine to start from — e.g.
# an image prepared for a network behind a proxy. REHEARSE_KEEP=1 leaves the
# container running at the end, to look around: docker exec -it territory-rehearsal bash
set -euo pipefail

BASE_IMAGE=${REHEARSE_BASE_IMAGE:-ubuntu:24.04}
IMAGE=territory-rehearsal
VPS=territory-rehearsal
REPO=$(cd "$(dirname "$0")/.." && pwd)

step() { printf '\n=== %s\n' "$*"; }
fail() {
    printf 'FAIL %s\n' "$*"
    exit 1
}
# Runs a command on the "VPS" as the operator (a sudoer, as on a cloud image).
ops() { docker exec -u ops -w /home/ops/Game "$VPS" bash -lc "$*"; }
root() { docker exec "$VPS" bash -lc "$*"; }
# Writes stdin to a file on the "VPS", as the operator.
put() { docker exec -i -u ops "$VPS" bash -c "cat > $1"; }

step "A fresh VPS: $BASE_IMAGE with systemd, sudo, git, curl and an operator"
docker build -q --network host -t "$IMAGE" - << EOF
FROM $BASE_IMAGE
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        systemd systemd-sysv dbus sudo git curl ca-certificates iproute2 \
    && useradd -m -s /bin/bash -G sudo ops \
    && echo 'ops ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/ops
STOPSIGNAL SIGRTMIN+3
CMD ["/lib/systemd/systemd"]
EOF
docker rm -f "$VPS" > /dev/null 2>&1 || true
[ "${REHEARSE_KEEP:-}" = 1 ] || trap 'docker rm -f "$VPS" > /dev/null 2>&1 || true' EXIT
docker run -d --name "$VPS" --privileged --network host --cgroupns=host \
    -v /sys/fs/cgroup:/sys/fs/cgroup:rw --tmpfs /run --tmpfs /run/lock "$IMAGE" > /dev/null
until root 'systemctl is-system-running 2> /dev/null | grep -qE "running|degraded"'; do sleep 1; done

step "The operator's checkout: this working tree, with its git history"
(cd "$REPO" && { git ls-files -co --exclude-standard; find .git -type f; } | tar -cf - -T -) |
    docker exec -i -u ops "$VPS" bash -c 'mkdir -p ~/Game && tar -xf - -C ~/Game'

step "setup.sh localhost"
ops 'sudo deploy/setup.sh localhost'
root 'systemctl is-enabled territory && systemctl is-active caddy' || fail "services after setup"

step "deploy.sh"
ops 'deploy/deploy.sh'
root 'systemctl is-active territory' || fail "territory is not running after deploy"
# Trust Caddy's local CA, as a browser trusts Let's Encrypt.
root 'caddy trust > /dev/null 2>&1'
export_ca='export NODE_EXTRA_CA_CERTS=/etc/ssl/certs/ca-certificates.crt'

step "The page over https, the game over wss"
page=$(root 'curl -fsS https://localhost/') || fail "no page at https://localhost/"
grep -q '<div id="app"' <<< "$page" || fail "the page is not the game's"
root 'curl -fsSI https://localhost/ | grep -qi "^cache-control: no-cache"' || fail "the page may be cached"
asset=$(grep -oE '/assets/[^"]+\.js' <<< "$page" | head -1)
root "curl -fsSI https://localhost$asset | grep -qi 'immutable'" || fail "assets are not cached for good"
ops "$export_ca; cd client && SERVER_URL=wss://localhost/ws npx tsx scripts/smoke.ts" || fail "join over wss"
ops "$export_ca; cd client && SERVER_URL=wss://localhost/ws npx tsx scripts/reconnect_smoke.ts" || fail "resume over wss"
root 'ss -ltnH | grep -q "127.0.0.1:27998" && ! ss -ltnH | grep -qE "(0.0.0.0|\*|\[::\]):27998"' ||
    fail "the game server listens beyond 127.0.0.1"

step "check.sh: kill -9 -> back within 10 s"
ops 'deploy/check.sh localhost'

step "A deploy with a player online: the player is back by itself, the same character"
put /tmp/online.mts << 'EOF'
// Joins over wss and stays; exits 0 once a second Welcome came (the server
// restarted under it and the session reconnected) for the same player_id (the
// world was saved and loaded), 1 otherwise or after 5 minutes.
import { SessionPlayer } from "/home/ops/Game/client/scripts/sessionPlayer.js";

const player = new SessionPlayer("wss://localhost/ws");
player.session.join(`online-${Math.random().toString(36).slice(2, 6)}`);
const back = await player.until(() => player.welcomes()[1], 300_000);
const first = player.welcomes()[0];
const same = back !== undefined && back.playerId === first?.playerId;
console.log(same ? "back in the game, the same character" : `not back as ${first?.playerId}: ${back?.playerId} ${JSON.stringify(player.statuses)}`);
process.exit(same ? 0 : 1);
EOF
ops "$export_ca; cd client && exec npx tsx /tmp/online.mts" &
online=$!
sleep 5
ops 'deploy/deploy.sh'
wait "$online" || fail "the player online did not come back after the deploy"

step "Releases: the last 3 kept"
ops 'deploy/deploy.sh'
ops 'deploy/deploy.sh'
count=$(root 'ls /opt/territory/releases | wc -l')
[ "$count" = 3 ] || fail "$count releases kept, not 3"

step "Rollback"
before=$(root 'readlink /opt/territory/current')
ops 'deploy/deploy.sh rollback'
after=$(root 'readlink /opt/territory/current')
previous=$(root 'ls /opt/territory/releases | sort | tail -2 | head -1')
[ "$after" = "releases/$previous" ] && [ "$after" != "$before" ] || fail "rollback: $before -> $after"
root 'systemctl is-active territory' || fail "territory is not running after rollback"
ops "$export_ca; cd client && SERVER_URL=wss://localhost/ws npx tsx scripts/smoke.ts" || fail "join after rollback"

step "setup.sh again: nothing to change, nothing broken"
ops 'sudo deploy/setup.sh localhost'
root 'systemctl is-active territory caddy' || fail "services after setup again"
ops "$export_ca; cd client && SERVER_URL=wss://localhost/ws npx tsx scripts/smoke.ts" || fail "join after setup again"

printf '\nREHEARSAL: PASS\n'
