#!/bin/bash
# Bundles soundshelf.exe + soundshelf-cli.exe with every DLL they
# need into a standalone folder that runs from cmd.exe / Explorer
# without MSYS2 on PATH.
#
# Run from a MSYS2 MINGW64 shell after a successful build:
#
#     bash scripts/windows-deploy.sh           # → dist/soundshelf-win64/
#     bash scripts/windows-deploy.sh out_dir   # custom output dir
#
# Strategy:
#   1. windeployqt collects Qt 6 runtime DLLs + plugins (platforms/
#      iconengines/imageformats/sqldrivers/...) per Qt's recipe.
#   2. ldd-walk soundshelf.exe and copy every non-system DLL it
#      transitively pulls from /mingw64/bin (libmpv-2.dll, libtag.dll,
#      libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll, …).
#   3. Re-walk windeployqt's output to catch DLLs that depend on
#      additional MinGW runtime libs.
#
# Output layout:
#   dist/soundshelf-win64/
#     ├── soundshelf.exe
#     ├── soundshelf-cli.exe
#     ├── *.dll                 (~50 runtime libs)
#     ├── platforms/
#     │     └── qwindows.dll
#     ├── iconengines/, imageformats/, sqldrivers/, styles/, tls/
#     └── translations/
#           └── qt_*.qm
#
# Distribution: zip the folder and ship.

set -euo pipefail

if [ "${MSYSTEM:-}" != "MINGW64" ]; then
    echo "ERROR: run from MSYS2 MINGW64 shell (set MSYSTEM=MINGW64)." >&2
    exit 1
fi

OUT="${1:-dist/soundshelf-win64}"
BUILD_DIR="${BUILD_DIR:-build}"

red()    { printf "\033[31m%s\033[0m\n" "$*"; }
green()  { printf "\033[32m%s\033[0m\n" "$*"; }
yellow() { printf "\033[33m%s\033[0m\n" "$*"; }

if [ ! -x "$BUILD_DIR/soundshelf.exe" ] || [ ! -x "$BUILD_DIR/soundshelf-cli.exe" ]; then
    red "Build artifacts missing in $BUILD_DIR/."
    red "Run: cmake -G Ninja -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release && cmake --build $BUILD_DIR -j"
    exit 2
fi

green "==> Output:    $OUT"
rm -rf "$OUT"
mkdir -p "$OUT"

cp "$BUILD_DIR/soundshelf.exe"     "$OUT/"
cp "$BUILD_DIR/soundshelf-cli.exe" "$OUT/"

# ---------- 1. Qt 6 runtime via windeployqt --------------------------------
green "==> windeployqt"
windeployqt6 \
    --release \
    --no-translations \
    --no-system-d3d-compiler \
    --no-virtualkeyboard \
    --no-quick-import \
    --no-opengl-sw \
    "$OUT/soundshelf.exe"
# CLI uses the same Qt libs; reuse the GUI bundle.

# ---------- 2. ldd-walk to catch non-Qt DLLs -------------------------------
green "==> Resolving non-Qt dependencies"

# `ldd` on Windows MSYS2 prints lines like
#   libmpv-2.dll => /mingw64/bin/libmpv-2.dll (0x...)
# We pick everything under /mingw64/.
walk_deps() {
    local exe="$1"
    local seen_var="$2"
    local -n seen="$seen_var"
    local out
    out=$(ldd "$exe" 2>/dev/null | awk '/=> \/mingw64\// {print $3}') || return 0
    for dll in $out; do
        local base
        base=$(basename "$dll")
        if [ -z "${seen[$base]:-}" ]; then
            seen[$base]=1
            cp -n "$dll" "$OUT/" 2>/dev/null || true
            walk_deps "$dll" seen
        fi
    done
}

declare -A SEEN
walk_deps "$OUT/soundshelf.exe"     SEEN
walk_deps "$OUT/soundshelf-cli.exe" SEEN

# windeployqt's plugins also drag in extra DLLs (e.g. sqldrivers/qsqlite
# needs sqlite3.dll). Re-walk the freshly-copied DLLs.
for dll in "$OUT"/*.dll; do
    walk_deps "$dll" SEEN
done
for sub in platforms iconengines imageformats sqldrivers styles tls; do
    [ -d "$OUT/$sub" ] || continue
    for dll in "$OUT/$sub"/*.dll; do
        [ -f "$dll" ] && walk_deps "$dll" SEEN
    done
done

# ---------- 3. Sanity check -------------------------------------------------
green "==> Verifying — every dep should resolve under $OUT or system32"
MISSING=0
check_deps() {
    local exe="$1"
    local out
    out=$(ldd "$exe" 2>/dev/null) || return
    while IFS= read -r line; do
        # Skip the binary's own line and lines without "=>".
        [[ "$line" == *"=>"* ]] || continue
        local target
        target=$(echo "$line" | awk '{print $3}')
        # not found
        if [ "$target" = "not" ]; then
            yellow "  $exe -> ${line%% (*}"
            MISSING=$((MISSING + 1))
            continue
        fi
        # mingw64/usr — should have been copied
        if [[ "$target" == /mingw64/* || "$target" == /usr/bin/* ]]; then
            local base
            base=$(basename "$target")
            if [ ! -f "$OUT/$base" ]; then
                yellow "  $exe still references uncopied $target"
                MISSING=$((MISSING + 1))
            fi
        fi
    done <<< "$out"
}
check_deps "$OUT/soundshelf.exe"
check_deps "$OUT/soundshelf-cli.exe"
for dll in "$OUT"/*.dll; do check_deps "$dll"; done

DLL_COUNT=$(find "$OUT" -maxdepth 1 -name '*.dll' | wc -l)
TOTAL_SIZE=$(du -sh "$OUT" | cut -f1)

echo
green "==> Done."
echo "    Output:        $OUT"
echo "    DLLs bundled:  $DLL_COUNT"
echo "    Plugins:       $(find "$OUT" -mindepth 2 -name '*.dll' | wc -l)"
echo "    Total size:    $TOTAL_SIZE"
if [ "$MISSING" -gt 0 ]; then
    red "    Unresolved:    $MISSING"
    red "    Run from cmd.exe to confirm — Windows might still find them on PATH."
else
    green "    All deps resolved."
fi
echo
echo "Test from cmd.exe (not MSYS2):"
echo "    $OUT\\soundshelf.exe"
