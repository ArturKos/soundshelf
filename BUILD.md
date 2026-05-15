# BUILD.md

## Linux (Debian / Ubuntu)

```bash
sudo apt install \
    qt6-base-dev qt6-tools-dev qt6-multimedia-dev qt6-l10n-tools \
    libmpv-dev libtag1-dev \
    libcdio-paranoia-dev libdiscid-dev \
    libchromaprint-dev libebur128-dev \
    libsqlite3-dev libfftw3-dev \
    cmake g++ pkg-config

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build  # /usr/local
```

## Linux (Arch / Manjaro)

```bash
sudo pacman -S \
    qt6-base qt6-tools qt6-multimedia \
    mpv taglib \
    libcdio libcdio-paranoia libdiscid \
    chromaprint libebur128 \
    sqlite fftw \
    cmake gcc pkgconf
```

## Linux (Fedora)

```bash
sudo dnf install \
    qt6-qtbase-devel qt6-qttools-devel qt6-qtmultimedia-devel \
    mpv-libs-devel taglib-devel \
    libcdio-paranoia-devel libdiscid-devel \
    chromaprint-devel libebur128-devel \
    sqlite-devel fftw-devel \
    cmake gcc-c++ pkgconf-pkg-config
```

## Windows — Option A: MSVC + vcpkg (recommended)

Static-links C/C++ deps (TagLib, SQLite, fftw3, chromaprint) into the
executable. Only Qt and libmpv ship as DLLs → bundle drops from ~250 MB /
147 DLLs to ~80 MB / ~20 DLLs.

**Prerequisites:** Visual Studio 2022 Build Tools (C++ workload), CMake 3.21+,
Ninja, Qt 6.5+ (via [aqtinstall](https://github.com/miurahr/aqtinstall) or the
Qt Online Installer).

```powershell
# 1. Run the one-time setup (clones vcpkg, downloads mpv-dev SDK)
.\scripts\setup-windows-vcpkg.ps1

# 2. Configure + build via CMake preset
cmake --preset windows-vcpkg
cmake --build build-vcpkg -j

# 3. Deploy standalone bundle
bash scripts/windows-deploy-vcpkg.sh
# → dist/soundshelf-win64-vcpkg/
```

Or manually:
```powershell
cmake -G Ninja -B build-vcpkg `
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake `
    -DVCPKG_TARGET_TRIPLET=x64-windows-static-md `
    -DMPV_DIR=external/mpv-dev `
    -DCMAKE_BUILD_TYPE=Release
cmake --build build-vcpkg -j
```

## Windows — Option B: MSYS2 MinGW64 (legacy)

Uses MSYS2 pacman packages (all dynamic). Larger bundle but simpler toolchain.

```bash
# In MSYS2 MINGW64 shell:
bash scripts/install-deps-windows.sh
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
bash scripts/windows-deploy.sh   # → dist/soundshelf-win64/
```

## macOS (Homebrew)

```bash
brew install qt@6 mpv taglib libcdio libdiscid chromaprint libebur128 sqlite fftw cmake

cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake --build build -j$(sysctl -n hw.ncpu)
```

## Build options

| Option | Default | Description |
|---|---|---|
| `-DSOUNDSHELF_BUILD_TESTS=ON/OFF` | OFF | Build unit tests |
| `-DSOUNDSHELF_BUILD_GUI=ON/OFF` | ON | Build the Qt GUI binary |
| `-DSOUNDSHELF_BUILD_CLI=ON/OFF` | ON | Build separate `soundshelf-cli` binary |
| `-DSOUNDSHELF_ENABLE_HTTPSERVER=ON/OFF` | ON | QHttpServer for headless mode |
| `-DSOUNDSHELF_ENABLE_MPRIS=ON/OFF` | ON (Linux) | MPRIS2 D-Bus integration |
| `-DSOUNDSHELF_ENABLE_LIBCDIO=ON/OFF` | ON | CD-DA reading via libcdio |
| `-DSOUNDSHELF_ENABLE_CHROMAPRINT=ON/OFF` | ON | AcoustID fingerprinting |
| `-DSOUNDSHELF_ENABLE_EBUR128=ON/OFF` | ON | EBU R128 loudness analysis |
| `-DSOUNDSHELF_ENABLE_FFTW3=ON/OFF` | ON | FFTW3 for spectrum FFT |
| `-DCMAKE_BUILD_TYPE` | Release | Debug / Release / RelWithDebInfo |

## Compiling translations

Translation files (`translations/*.ts`) compile to `*.qm` automatically during
build. To regenerate `.ts` after string changes:

```bash
cd translations
lupdate ../src ../include -ts soundshelf_en.ts soundshelf_pl.ts soundshelf_de.ts soundshelf_fr.ts
# Edit .ts files in Qt Linguist
lrelease *.ts  # produces .qm
```

## Running tests

```bash
cmake -B build -DSOUNDSHELF_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Database location

- **Linux:** `~/.local/share/soundshelf/library.db`
- **Windows:** `%LOCALAPPDATA%\SoundShelf\library.db`
- **macOS:** `~/Library/Application Support/SoundShelf/library.db`

Override with `--db PATH` flag or `SOUNDSHELF_DB` environment variable.

## Configuration location

- **Linux:** `~/.config/soundshelf/config.ini`
- **Windows:** `%APPDATA%\SoundShelf\config.ini`
- **macOS:** `~/Library/Preferences/SoundShelf.ini`

## Troubleshooting

### "Cannot find Qt6"
Set `CMAKE_PREFIX_PATH` to your Qt6 installation:
```bash
cmake -B build -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake/Qt6
```

### "libmpv not found"
On Debian < 12, libmpv-dev is too old. Install from repos:
```bash
sudo add-apt-repository ppa:mpv/release
sudo apt update && sudo apt install libmpv-dev
```

### "libcdio-paranoia not available"
On macOS, paranoia is bundled in libcdio (no separate package). On Windows
via vcpkg, only `libcdio` is available — paranoia features are degraded
(no error correction during rip). For full paranoia on Windows, build from
source.

### Permission denied on /dev/sr0
```bash
sudo usermod -aG cdrom $USER
# log out and back in
```
