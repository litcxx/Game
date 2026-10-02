#!/usr/bin/env bash
# Installs the CMake the server is built with: Kitware's own build of CMake 4.4 for
# Linux in /opt/cmake-$VERSION, as cmake, ctest and cpack in /usr/local/bin,
# checked by SHA-256. CI, local development and deployment all use this one.
# Running it again keeps what is in place. Needs sudo (unless run as root).
#
#   server/scripts/install-cmake.sh
set -euo pipefail

VERSION=4.4.3
RELEASE=https://github.com/Kitware/CMake/releases/download/v$VERSION
ARCH=$(dpkg --print-architecture)
case $ARCH in
    amd64)
        NAME=cmake-$VERSION-linux-x86_64
        SHA256=d6c83076c575bc00b823522ac974bda66d0af05d6ddc30e739c12385cf32c6cc
        ;;
    arm64)
        NAME=cmake-$VERSION-linux-aarch64
        SHA256=2efc974dbd63b4444c0e8494b92f2e80c2d7e635b4b80eac2916985ddd8f72a6
        ;;
    *)
        echo "$0: unsupported architecture $ARCH (amd64 or arm64)" >&2
        exit 1
        ;;
esac
PREFIX=/opt/cmake-$VERSION
sudo=()
[ "$(id -u)" = 0 ] || sudo=(sudo)

if [ "$(cat "$PREFIX/release" 2> /dev/null)" = "$NAME" ]; then
    echo "CMake $VERSION is installed already"
else
    work=$(mktemp -d)
    trap 'rm -rf "$work"' EXIT
    curl -fsSL -o "$work/cmake.tar.gz" "$RELEASE/$NAME.tar.gz"
    echo "$SHA256  $work/cmake.tar.gz" | sha256sum -c -
    "${sudo[@]}" rm -rf "$PREFIX"
    "${sudo[@]}" mkdir -p "$PREFIX"
    "${sudo[@]}" tar -xzf "$work/cmake.tar.gz" -C "$PREFIX" --strip-components=1 --no-same-owner
    echo "$NAME" | "${sudo[@]}" tee "$PREFIX/release" > /dev/null
fi
for tool in cmake ctest cpack; do
    "${sudo[@]}" ln -sfn "$PREFIX/bin/$tool" "/usr/local/bin/$tool"
done
cmake --version | head -1
