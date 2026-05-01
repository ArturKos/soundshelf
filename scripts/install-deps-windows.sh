#!/bin/bash
# SoundShelf Windows build environment via MSYS2 / MinGW64.
#
# Run from a MSYS2 MINGW64 shell:
#   bash scripts/install-deps-windows.sh
#
# What this does:
#   1. pacman -Syu --noconfirm  (sync + upgrade base system, may need to be re-run once)
#   2. Installs the full mingw-w64 dev toolchain + Qt 6 + libmpv + TagLib +
#      libcdio + chromaprint + ebur128 + fftw3.
#   3. Patches three upstream .pc files that ship with hardcoded
#      `-I/mingw64/include` (isl, mujs, tre) so CMake's pkg-config
#      integration sees the correct Windows-native paths.
#
# The standalone MSYS2 installer can be obtained from
# https://www.msys2.org/ — install to C:\msys64 then run this script.
set -euo pipefail

if [ "${MSYSTEM:-}" != "MINGW64" ]; then
    echo "ERROR: this script must run from a MSYS2 MINGW64 shell." >&2
    echo "Open the 'MSYS2 MinGW 64-bit' shortcut and rerun." >&2
    exit 1
fi

echo "==> Synchronising package database…"
pacman -Sy --noconfirm

echo "==> Installing toolchain + libraries…"
pacman -S --noconfirm --needed \
    git \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-cmake \
    mingw-w64-x86_64-ninja \
    mingw-w64-x86_64-make \
    mingw-w64-x86_64-pkgconf \
    mingw-w64-x86_64-qt6-base \
    mingw-w64-x86_64-qt6-tools \
    mingw-w64-x86_64-qt6-multimedia \
    mingw-w64-x86_64-qt6-svg \
    mingw-w64-x86_64-qt6-httpserver \
    mingw-w64-x86_64-mpv \
    mingw-w64-x86_64-taglib \
    mingw-w64-x86_64-sqlite3 \
    mingw-w64-x86_64-libcdio-paranoia \
    mingw-w64-x86_64-libdiscid \
    mingw-w64-x86_64-chromaprint \
    mingw-w64-x86_64-libebur128 \
    mingw-w64-x86_64-fftw \
    mingw-w64-x86_64-doxygen \
    mingw-w64-x86_64-graphviz

echo "==> Patching upstream .pc files with hardcoded /mingw64/include paths…"
for f in /mingw64/lib/pkgconfig/isl.pc \
         /mingw64/lib/pkgconfig/mujs.pc \
         /mingw64/lib/pkgconfig/tre.pc; do
    if [ -f "$f" ] && grep -q -- '-I/mingw64/include' "$f"; then
        sed -i 's|-I/mingw64/include\b|-I${includedir}|g' "$f"
        echo "    patched $f"
    fi
done

echo
echo "✓ Done. Build with:"
echo "    cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release -DSOUNDSHELF_BUILD_TESTS=ON"
echo "    cmake --build build -j"
echo "    ctest --test-dir build --output-on-failure"
