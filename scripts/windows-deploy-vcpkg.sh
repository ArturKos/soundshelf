#!/bin/bash
# Simplified Windows deploy for vcpkg builds.
#
# With vcpkg static linking, most C/C++ deps (TagLib, SQLite, fftw3,
# chromaprint) are baked into the exe. Only Qt (dynamic, LGPL) and
# libmpv (pre-built DLL) need to be bundled.
#
# Usage (from Git Bash / MSYS2 / PowerShell):
#   bash scripts/windows-deploy-vcpkg.sh [output_dir]
#
# Prerequisites:
#   - Successful cmake --preset windows-vcpkg && cmake --build build-vcpkg
#   - windeployqt6 on PATH (Qt bin directory)
#   - external/mpv-dev populated (by setup-windows-vcpkg.sh)

set -euo pipefail

OUT="${1:-dist/soundshelf-win64}"
BUILD_DIR="${BUILD_DIR:-build-vcpkg}"
MPV_DIR="${MPV_DIR:-external/mpv-dev}"

red()   { printf "\033[31m%s\033[0m\n" "$*"; }
green() { printf "\033[32m%s\033[0m\n" "$*"; }

if [ ! -f "$BUILD_DIR/soundshelf.exe" ]; then
    red "soundshelf.exe not found in $BUILD_DIR/."
    red "Build first: cmake --preset windows-vcpkg && cmake --build build-vcpkg -j"
    exit 2
fi

green "==> Output: $OUT"
rm -rf "$OUT"
mkdir -p "$OUT"

# -- 1. Copy executables ------------------------------------------------------
cp "$BUILD_DIR/soundshelf.exe"     "$OUT/"
[ -f "$BUILD_DIR/soundshelf-cli.exe" ] && cp "$BUILD_DIR/soundshelf-cli.exe" "$OUT/"

# -- 2. Qt runtime via windeployqt ---------------------------------------------
green "==> windeployqt"
windeployqt6 \
    --release \
    --no-translations \
    --no-system-d3d-compiler \
    --no-quick-import \
    --no-opengl-sw \
    "$OUT/soundshelf.exe" 2>/dev/null || \
windeployqt \
    --release \
    --no-translations \
    --no-system-d3d-compiler \
    --no-quick-import \
    --no-opengl-sw \
    "$OUT/soundshelf.exe"

# -- 3. libmpv DLL -------------------------------------------------------------
green "==> Bundling libmpv"
MPV_DLLS=()
for name in libmpv-2.dll mpv-2.dll; do
    for dir in "$MPV_DIR" "$MPV_DIR/bin" "$MPV_DIR/lib"; do
        if [ -f "$dir/$name" ]; then
            cp "$dir/$name" "$OUT/"
            MPV_DLLS+=("$name")
        fi
    done
done
if [ ${#MPV_DLLS[@]} -eq 0 ]; then
    red "WARNING: libmpv DLL not found in $MPV_DIR — audio playback will fail."
fi

# -- 4. Clean up unnecessary files from windeployqt ----------------------------
green "==> Cleaning up"
rm -f "$OUT/vc_redist.x64.exe"
rm -f "$OUT/dxcompiler.dll" "$OUT/dxil.dll"
rm -f "$OUT/sqldrivers/qsqlmimer.dll" "$OUT/sqldrivers/qsqlodbc.dll" "$OUT/sqldrivers/qsqlpsql.dll"
rm -rf "$OUT/generic"

# -- 5. MSVC runtime (if not using static CRT) --------------------------------
# With x64-windows-static-md, the MSVC runtime (vcruntime140.dll, msvcp140.dll)
# is dynamically linked. These are usually already present on Windows 10+.
# If targeting older systems, uncomment below to bundle them.
# for rt in vcruntime140.dll vcruntime140_1.dll msvcp140.dll; do
#     src="$(dirname "$(which cl.exe 2>/dev/null)")/../../../redist/x64/.../$rt"
#     [ -f "$src" ] && cp "$src" "$OUT/"
# done

# -- 6. Summary ----------------------------------------------------------------
DLL_COUNT=$(find "$OUT" -maxdepth 1 -name '*.dll' | wc -l)
PLUGIN_COUNT=$(find "$OUT" -mindepth 2 -name '*.dll' 2>/dev/null | wc -l)
TOTAL_SIZE=$(du -sh "$OUT" | cut -f1)

echo
green "==> Done."
echo "    Output:       $OUT"
echo "    DLLs:         $DLL_COUNT (top-level) + $PLUGIN_COUNT (plugins)"
echo "    Total size:   $TOTAL_SIZE"
echo
echo "Test: $OUT\\soundshelf.exe"
