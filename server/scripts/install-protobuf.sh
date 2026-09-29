#!/usr/bin/env bash
# Builds and installs the protobuf the server is built and tested with — the C++
# runtime, protoc and the abseil they need — from the pinned release source,
# checked by SHA-256. CI, local development and deployment all use this one.
#
#   server/scripts/install-protobuf.sh [prefix]   # default /usr/local (sudo for the install)
#
# CMake then finds it by itself under /usr/local, or with
# -DCMAKE_PREFIX_PATH=<prefix> (or the CMAKE_PREFIX_PATH environment variable).
set -euo pipefail

VERSION=36.2
SHA256=3d9642a662d10e68ebae5e53f14dcce5105684212d5078f8e0d47d1ab3ae6b64
PREFIX=${1:-/usr/local}
JOBS=${JOBS:-$(nproc)}

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work"

curl -fsSL -o protobuf.tar.gz \
    "https://github.com/protocolbuffers/protobuf/releases/download/v${VERSION}/protobuf-${VERSION}.tar.gz"
echo "${SHA256}  protobuf.tar.gz" | sha256sum -c -
tar xzf protobuf.tar.gz

generator=()
if command -v ninja > /dev/null; then generator=(-G Ninja); fi
# Release, without its tests. Abseil is fetched by protobuf's own configure, at
# the version it pins.
cmake -S "protobuf-${VERSION}" -B build "${generator[@]}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -Dprotobuf_BUILD_TESTS=OFF
cmake --build build -j "$JOBS"

install=(cmake --install build)
if ! mkdir -p "$PREFIX" 2> /dev/null || [ ! -w "$PREFIX" ]; then install=(sudo "${install[@]}"); fi
"${install[@]}"
echo "protobuf ${VERSION} installed to ${PREFIX}"
