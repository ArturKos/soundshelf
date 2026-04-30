#!/usr/bin/env bash
# Installs SoundShelf build dependencies on Debian/Ubuntu/Linux Mint.
#
# Usage:
#   ./scripts/install-deps.sh           # install everything
#   ./scripts/install-deps.sh --check   # only print what's missing
#
# Requires sudo for the actual install pass.

set -euo pipefail

# Mandatory packages (as named in noble/wilma APT).
PACKAGES=(
    # Build toolchain
    cmake
    g++
    pkg-config
    git

    # Qt 6 — Core, Gui, Widgets, Network, Sql, Concurrent, DBus all live in qt6-base-dev
    qt6-base-dev
    qt6-base-dev-tools
    qt6-tools-dev          # lupdate / lrelease (LinguistTools)
    qt6-tools-dev-tools
    qt6-multimedia-dev
    qt6-svg-dev
    qt6-httpserver-dev     # optional but in our build matrix

    # Audio engine + tags
    libmpv-dev
    libtag1-dev

    # Database
    libsqlite3-dev

    # CD-DA + AcoustID + R128 + FFT
    libcdio-dev
    libcdio-paranoia-dev
    libdiscid-dev
    libchromaprint-dev
    libebur128-dev
    libfftw3-dev

    # Format converter shells out to ffmpeg(1)
    ffmpeg

    # Documentation
    doxygen
    graphviz
)

red()    { printf "\033[31m%s\033[0m\n" "$*"; }
green()  { printf "\033[32m%s\033[0m\n" "$*"; }
yellow() { printf "\033[33m%s\033[0m\n" "$*"; }

is_installed() {
    dpkg-query -W -f='${Status}' "$1" 2>/dev/null | grep -q "ok installed"
}

check_only=0
case "${1:-}" in
    --check) check_only=1 ;;
    --help|-h) sed -n '2,12p' "$0"; exit 0 ;;
esac

missing=()
for pkg in "${PACKAGES[@]}"; do
    if is_installed "$pkg"; then
        green "  [ok]    $pkg"
    else
        yellow "  [need]  $pkg"
        missing+=("$pkg")
    fi
done

echo
if (( ${#missing[@]} == 0 )); then
    green "All ${#PACKAGES[@]} dependencies are installed."
    exit 0
fi

if (( check_only )); then
    yellow "Missing ${#missing[@]} package(s). Re-run without --check to install."
    exit 1
fi

yellow "Installing ${#missing[@]} missing package(s) via sudo apt-get…"
sudo apt-get update
sudo apt-get install -y "${missing[@]}"

green "Done. You can now run:"
echo "  cmake -B build -DCMAKE_BUILD_TYPE=Release -DSOUNDSHELF_BUILD_TESTS=ON"
echo "  cmake --build build -j"
