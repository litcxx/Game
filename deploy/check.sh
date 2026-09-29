#!/usr/bin/env bash
# Checks what the deployment promises, on the VPS: the game and Caddy run; the
# site serves the page over https and takes the game's WebSocket at /ws; after
# kill -9 the game is back within 10 s (it takes connections again).
#
#   deploy/check.sh game.example.com
set -euo pipefail

DOMAIN=${1:?"usage: $0 <domain>"}
PORT=27998

ok() { printf 'ok   %s\n' "$*"; }
fail() {
    printf 'FAIL %s\n' "$*"
    exit 1
}
now_ms() { echo $(($(date +%s%N) / 1000000)); }

systemctl is-active --quiet territory && systemctl is-active --quiet caddy ||
    fail "territory and caddy should be running: systemctl status territory caddy"
ok "territory and caddy are running"

page=$(curl -fsS --max-time 10 "https://$DOMAIN/") || fail "no page at https://$DOMAIN/"
grep -q '<div id="app"' <<< "$page" || fail "https://$DOMAIN/ is not the game's page"
ok "https://$DOMAIN/ serves the game"

# A WebSocket handshake: 101 from the game server behind Caddy (curl then waits
# for data; the time limit ends it).
status=$(curl -sS -o /dev/null -w '%{http_code}' --max-time 2 --http1.1 \
    -H 'Connection: Upgrade' -H 'Upgrade: websocket' -H 'Sec-WebSocket-Version: 13' \
    -H 'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==' "https://$DOMAIN/ws" 2> /dev/null || true)
[ "$status" = 101 ] || fail "wss://$DOMAIN/ws answered $status, not 101"
ok "wss://$DOMAIN/ws takes the game's WebSocket"

old=$(systemctl show -p MainPID --value territory)
sudo kill -9 "$old"
killed=$(now_ms)
until new=$(systemctl show -p MainPID --value territory) &&
    [ "$new" != 0 ] && [ "$new" != "$old" ] && (: > "/dev/tcp/127.0.0.1/$PORT") 2> /dev/null; do
    [ $(($(now_ms) - killed)) -lt 10000 ] || fail "not back within 10 s of kill -9"
    sleep 0.1
done
ok "kill -9: back in $(($(now_ms) - killed)) ms (pid $old -> $new)"
