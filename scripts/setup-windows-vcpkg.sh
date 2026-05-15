#!/bin/bash
# Bootstrap vcpkg + download pre-built libmpv for a Windows build.
#
# Run from the project root in Git Bash, MSYS2, or WSL:
#   bash scripts/setup-windows-vcpkg.sh
#
# This works for both MSVC and MinGW toolchains — the key difference
# is the import library format (.lib for MSVC, .dll.a for MinGW).

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

MPV_URL="${MPV_URL:-https://sourceforge.net/projects/mpv-player-windows/files/libmpv/mpv-dev-x86_64-v3-20260419-git-06f4ce7.7z/download}"

# ---------- 1. vcpkg ----------------------------------------------------------
if [ "${SKIP_VCPKG:-}" != "1" ]; then
    if [ ! -f vcpkg/.vcpkg-root ]; then
        echo "==> Cloning vcpkg..."
        git clone https://github.com/microsoft/vcpkg.git vcpkg
    fi
    echo "==> Bootstrapping vcpkg..."
    if [ -f vcpkg/bootstrap-vcpkg.bat ]; then
        # Native Windows (Git Bash / MSYS2)
        cmd.exe //C "cd vcpkg && bootstrap-vcpkg.bat -disableMetrics" 2>/dev/null \
            || ./vcpkg/bootstrap-vcpkg.sh -disableMetrics
    else
        ./vcpkg/bootstrap-vcpkg.sh -disableMetrics
    fi
    echo "==> vcpkg ready."
fi

# ---------- 2. Pre-built libmpv -----------------------------------------------
if [ "${SKIP_MPV:-}" != "1" ]; then
    MPV_DIR="external/mpv-dev"
    if [ -f "$MPV_DIR/include/mpv/client.h" ]; then
        echo "==> mpv-dev already present, skipping."
    else
        mkdir -p external
        ARCHIVE="external/mpv-dev.7z"

        echo "==> Downloading mpv-dev SDK..."
        curl -L -o "$ARCHIVE" "$MPV_URL"

        echo "==> Extracting..."
        if command -v 7z &>/dev/null; then
            7z x "$ARCHIVE" -o"$MPV_DIR" -y
        elif command -v 7zz &>/dev/null; then
            7zz x "$ARCHIVE" -o"$MPV_DIR" -y
        else
            echo "ERROR: 7-Zip (7z or 7zz) is required to extract the mpv-dev archive." >&2
            echo "Install: pacman -S p7zip   (MSYS2)  or  apt install p7zip-full  (Debian)" >&2
            exit 1
        fi
        rm -f "$ARCHIVE"

        # MinGW import lib (.dll.a) is usually included in the archive.
        # For MSVC builds, run the PowerShell script or use:
        #   lib /def:mpv.def /out:lib/mpv.lib /machine:x64
        echo "==> mpv-dev ready at $MPV_DIR"
    fi
fi

echo
echo "Setup complete. Build with:"
echo "  cmake --preset windows-vcpkg"
echo "  cmake --build build-vcpkg -j"
