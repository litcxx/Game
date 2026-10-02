#!/usr/bin/env bash
# Installs the GCC the server is built with: GCC 16.2 for Ubuntu 24.04 in
# /opt/gcc-$VERSION, as gcc-16 and g++-16 in /usr/local/bin. It is the build of
# scripts/build-gcc.sh that the toolchain workflow (.github/workflows/toolchain.yml)
# publishes, checked by SHA-256. CI, local development and deployment all use this
# one. Running it again keeps what is in place. Needs sudo (unless run as root),
# and from the system what any GCC needs: binutils and libc6-dev.
#
#   server/scripts/install-gcc.sh
#
# What it links runs with this GCC's libstdc++ (a RUNPATH to /opt/gcc-$VERSION):
# keep the directory while a server built with it runs.
set -euo pipefail

VERSION=16.2.0
REVISION=1
RELEASE=https://github.com/litcxx/Game/releases/download/gcc-$VERSION-r$REVISION
ARCH=$(dpkg --print-architecture)
case $ARCH in
    amd64) SHA256=TODO ;;
    arm64) SHA256=TODO ;;
    *)
        echo "$0: unsupported architecture $ARCH (amd64 or arm64)" >&2
        exit 1
        ;;
esac
NAME=gcc-$VERSION-r$REVISION-ubuntu-24.04-$ARCH
PREFIX=/opt/gcc-$VERSION
sudo=()
[ "$(id -u)" = 0 ] || sudo=(sudo)

if [ "$(cat "$PREFIX/release" 2> /dev/null)" = "$NAME" ]; then
    echo "GCC $VERSION is installed already"
else
    work=$(mktemp -d)
    trap 'rm -rf "$work"' EXIT
    curl -fsSL -o "$work/gcc.tar.xz" "$RELEASE/$NAME.tar.xz"
    echo "$SHA256  $work/gcc.tar.xz" | sha256sum -c -
    "${sudo[@]}" rm -rf "$PREFIX"
    "${sudo[@]}" tar -xJf "$work/gcc.tar.xz" -C /
    echo "$NAME" | "${sudo[@]}" tee "$PREFIX/release" > /dev/null
fi
for tool in gcc g++; do
    "${sudo[@]}" ln -sfn "$PREFIX/bin/$tool" "/usr/local/bin/$tool-${VERSION%%.*}"
done
"g++-${VERSION%%.*}" --version | head -1
