#!/usr/bin/env bash
# Builds the GCC the server is built with (C and C++, for this machine's
# architecture) from the GNU release, its signature checked against the keys of
# the GCC release managers, and packs it for install-gcc.sh. The toolchain
# workflow (.github/workflows/toolchain.yml) runs it on Ubuntu 24.04 and publishes
# the archive: it is for Ubuntu 24.04 (its glibc and binutils), at /opt/gcc-$VERSION
# only. It builds right there, so run it on a machine without that directory.
#
#   server/scripts/build-gcc.sh <out dir>   # -> gcc-<version>-r<revision>-ubuntu-24.04-<arch>.tar.xz
#
# The compiler adds a RUNPATH to its own libraries to everything it links (a specs
# file, as Homebrew's GCC does): the server, the tests and protoc then run with this
# GCC's libstdc++ and sanitizer runtimes, not with the system's older ones.
set -euo pipefail

VERSION=16.2.0
# A new revision rebuilds the same version (other options): a new release, the
# published one stays as it is.
REVISION=1
# The GCC release managers (the keys the Docker Official Image gcc checks).
GPG_KEYS=(
    B215C1633BCA0477615F1B35A5B3A004745C015A
    B3C42148A44E6983B3E4CC0793FA9B1AB75C61B8
    90AA470469D3965A87A5DCB494D03953902C9419
    80F98B2E0DAB6C8281BDF541A7C8C3B2F71EDF1C
    7F74F97C103468EE5D750B583AB00996FC26A641
    33C235A34C46AA3FFB293709A328C3A2C3C45C06
    D3A93CAD751C2AF4F8C7AD516C35B99309B5FA62
)
MIRRORS=(https://ftpmirror.gnu.org/gcc https://ftp.gnu.org/gnu/gcc https://sourceware.org/pub/gcc/releases)
PREFIX=/opt/gcc-$VERSION
OUT=$(realpath -m "${1:?usage: $0 <out dir>}")
JOBS=${JOBS:-$(nproc)}
NAME=gcc-$VERSION-r$REVISION-ubuntu-24.04-$(dpkg --print-architecture)

[ ! -e "$PREFIX" ] || { echo "$0: $PREFIX exists: build on a machine without it" >&2; exit 1; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work"

fetch() {
    for mirror in "${MIRRORS[@]}"; do
        curl -fsSL --retry 3 -o "$2" "$mirror/$1" && return 0
    done
    echo "$0: cannot download $1" >&2
    return 1
}
fetch "gcc-$VERSION/gcc-$VERSION.tar.xz" gcc.tar.xz
fetch "gcc-$VERSION/gcc-$VERSION.tar.xz.sig" gcc.tar.xz.sig
export GNUPGHOME=$work/gnupg
install -d -m 700 "$GNUPGHOME"
gpg --batch --keyserver hkps://keyserver.ubuntu.com --recv-keys "${GPG_KEYS[@]}"
gpg --batch --verify gcc.tar.xz.sig gcc.tar.xz
tar -xf gcc.tar.xz
# GMP, MPFR, MPC and ISL go into the tree (checked by the SHA-512 sums the GCC
# release carries) and into the compiler itself: nothing to install next to it.
(cd "gcc-$VERSION" && ./contrib/download_prerequisites)

# Debian's triplet (x86_64-linux-gnu, aarch64-linux-gnu), so that the compiler
# finds the multiarch headers and libraries of Ubuntu's glibc.
mkdir build
cd build
"../gcc-$VERSION/configure" \
    --build="$(gcc -dumpmachine)" \
    --prefix="$PREFIX" \
    --enable-languages=c,c++ \
    --disable-multilib
make -j "$JOBS"
sudo=()
[ "$(id -u)" = 0 ] || sudo=(sudo)
"${sudo[@]}" install -d -o "$(id -u)" -g "$(id -g)" "$PREFIX"
make install-strip

# Everything it links finds its runtime libraries: a RUNPATH to them.
libdir=$(dirname "$(realpath "$("$PREFIX/bin/g++" -print-file-name=libstdc++.so)")")
specs=$(dirname "$("$PREFIX/bin/gcc" -print-libgcc-file-name)")/specs
"$PREFIX/bin/gcc" -dumpspecs > "$specs"
printf '%s\n' '*link_libgcc:' "+ %{!static:%{!static-pie:-rpath $libdir}}" '' >> "$specs"

# It builds C++23 that runs here without the system's help, and with the sanitizers.
cat > hello.cpp << 'EOF'
#include <print>
int main() { std::println("hello from GCC {}.{}", __GNUC__, __GNUC_MINOR__); }
EOF
"$PREFIX/bin/g++" -std=c++23 hello.cpp -o hello
readelf -d hello | grep -q "RUNPATH.*\[$libdir\]" || { echo "$0: no RUNPATH to $libdir" >&2; exit 1; }
./hello
"$PREFIX/bin/g++" -std=c++23 -fsanitize=address,undefined hello.cpp -o hello-asan

mkdir -p "$OUT"
XZ_OPT=-T0 tar -C / --owner=0 --group=0 --numeric-owner -cJf "$OUT/$NAME.tar.xz" "${PREFIX#/}"
(cd "$OUT" && sha256sum "$NAME.tar.xz" | tee "$NAME.tar.xz.sha256")
