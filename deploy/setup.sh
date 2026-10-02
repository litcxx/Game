#!/usr/bin/env bash
# Prepares a fresh Ubuntu 24.04 VPS for the game, once — running it again keeps
# what is in place. Installs the build tools, GCC 16.2, CMake 4.4, protobuf, Node.js
# and Caddy (pinned versions, checked by hash); creates the service user and /opt/territory;
# installs the game's systemd service and the Caddy site for DOMAIN. Then
# deploy/deploy.sh builds and starts the game. See docs/ops.md.
#
#   sudo deploy/setup.sh game.example.com
set -euo pipefail

DOMAIN=${1:?"usage: sudo $0 <domain>"}
[[ $DOMAIN =~ ^[A-Za-z0-9.-]+$ ]] || { echo "$0: not a domain name: $DOMAIN" >&2; exit 1; }
[ "$(id -u)" = 0 ] || { echo "$0: run it as root (sudo)" >&2; exit 1; }
REPO=$(cd "$(dirname "$0")/.." && pwd)
ROOT=/opt/territory

NODE_VERSION=22.23.3
CADDY_VERSION=2.11.4
ARCH=$(dpkg --print-architecture)
case $ARCH in
    amd64)
        NODE_ARCH=x64
        NODE_SHA256=df450af89261115ef9f9e3830c3eeb2cc9213b63c720b1af623cb5dcbe2e02de
        CADDY_SHA512=1c6f5404f3622e46d401d81f4af59677d46b886229c6694d60fd936b87c72d3bb5d1fcf42b55c8d555769fa75acf434ab618fc7e0df2c79cf8512ee580d38d06
        ;;
    arm64)
        NODE_ARCH=arm64
        NODE_SHA256=a44aeb94849a299b22df10b9e622ec2f605c2183501bc40590705131de7c740f
        CADDY_SHA512=c43c62b7b583b31c682b3c3e1a31cf03759fbab01dcb0fc7d7fc3a5ce1bef43403583e26133920634a730a9fe31dae1386af4d3f9f3fc19fcc2c29ebf19de235
        ;;
    *)
        echo "$0: unsupported architecture $ARCH (amd64 or arm64)" >&2
        exit 1
        ;;
esac

step() { printf '\n== %s\n' "$*"; }
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

step "Packages: the build tools and libraries"
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    binutils libc6-dev ninja-build libboost-dev libssl-dev git curl ca-certificates jq xz-utils

GCC_VERSION=$(sed -n 's/^VERSION=//p' "$REPO/server/scripts/install-gcc.sh")
step "GCC $GCC_VERSION (the server's compiler, prebuilt for Ubuntu 24.04)"
"$REPO/server/scripts/install-gcc.sh"

CMAKE_VERSION=$(sed -n 's/^VERSION=//p' "$REPO/server/scripts/install-cmake.sh")
step "CMake $CMAKE_VERSION (Kitware's build)"
"$REPO/server/scripts/install-cmake.sh"

step "Swap: the build needs about 2 GB of memory"
if [ "$(awk '/^MemTotal/ { print $2 }' /proc/meminfo)" -lt 3000000 ] && [ -z "$(swapon --show --noheadings)" ]; then
    [ -f /swapfile ] || { fallocate -l 2G /swapfile && chmod 600 /swapfile && mkswap /swapfile; }
    swapon /swapfile
    grep -q '^/swapfile ' /etc/fstab || echo '/swapfile none swap sw 0 0' >> /etc/fstab
    echo "2 GB of swap in /swapfile"
else
    echo "enough memory, or swap already on"
fi

PROTOBUF_VERSION=$(sed -n 's/^VERSION=//p' "$REPO/server/scripts/install-protobuf.sh")
step "protobuf $PROTOBUF_VERSION (built from source the first time: 10 minutes or so)"
if [ "$(protoc --version 2> /dev/null)" = "libprotoc $PROTOBUF_VERSION" ]; then
    echo "already installed"
else
    "$REPO/server/scripts/install-protobuf.sh" /usr/local
fi

step "Node.js $NODE_VERSION"
if [ "$(node --version 2> /dev/null)" = "v$NODE_VERSION" ]; then
    echo "already installed"
else
    curl -fsSL -o "$work/node.tar.xz" \
        "https://nodejs.org/dist/v$NODE_VERSION/node-v$NODE_VERSION-linux-$NODE_ARCH.tar.xz"
    echo "$NODE_SHA256  $work/node.tar.xz" | sha256sum -c -
    rm -rf /usr/local/lib/nodejs
    mkdir -p /usr/local/lib/nodejs
    tar -xJf "$work/node.tar.xz" -C /usr/local/lib/nodejs --strip-components=1
    ln -sf /usr/local/lib/nodejs/bin/node /usr/local/lib/nodejs/bin/npm /usr/local/lib/nodejs/bin/npx /usr/local/bin/
fi

step "Caddy $CADDY_VERSION"
if [ "$(caddy version 2> /dev/null | cut -d' ' -f1)" = "v$CADDY_VERSION" ]; then
    echo "already installed"
else
    curl -fsSL -o "$work/caddy.deb" \
        "https://github.com/caddyserver/caddy/releases/download/v$CADDY_VERSION/caddy_${CADDY_VERSION}_linux_$ARCH.deb"
    echo "$CADDY_SHA512  $work/caddy.deb" | sha512sum -c -
    # Keep our Caddyfile on an upgrade.
    dpkg -i --force-confold "$work/caddy.deb"
fi

step "The service user and $ROOT"
id -u territory > /dev/null 2>&1 ||
    useradd --system --no-create-home --home-dir /nonexistent --shell /usr/sbin/nologin territory
install -d -m 755 "$ROOT" "$ROOT/releases"

step "The game service"
if cmp -s "$REPO/deploy/territory.service" /etc/systemd/system/territory.service; then
    echo "unchanged"
else
    install -m 644 "$REPO/deploy/territory.service" /etc/systemd/system/territory.service
    systemctl daemon-reload
    # A running game takes the new unit now; a new VPS starts it at its first deploy.
    systemctl try-restart territory
fi
systemctl enable territory

step "The site: https://$DOMAIN"
sed "s/{\$DOMAIN}/$DOMAIN/" "$REPO/deploy/Caddyfile" > "$work/Caddyfile"
caddy validate --adapter caddyfile --config "$work/Caddyfile"
if cmp -s "$work/Caddyfile" /etc/caddy/Caddyfile; then
    echo "unchanged"
else
    install -m 644 "$work/Caddyfile" /etc/caddy/Caddyfile
    systemctl reload-or-restart caddy
fi

printf '\nThe VPS is ready. Next, as yourself (not root): deploy/deploy.sh\n'
