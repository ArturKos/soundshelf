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

## Windows (vcpkg + MSVC 2022)

```powershell
# 1. Install vcpkg
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# 2. Install dependencies
.\vcpkg install qtbase:x64-windows qttools:x64-windows
.\vcpkg install mpv:x64-windows taglib:x64-windows
.\vcpkg install libcdio:x64-windows
.\vcpkg install chromaprint:x64-windows libebur128:x64-windows
.\vcpkg install sqlite3:x64-windows fftw3:x64-windows

# 3. Build SoundShelf
cd path\to\soundshelf
cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
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
| `-DBUILD_TESTING=ON/OFF` | OFF | Build unit tests |
| `-DBUILD_CLI=ON/OFF` | ON | Build separate `soundshelf-cli` binary |
| `-DENABLE_HEADLESS_SERVER=ON/OFF` | ON | Build HTTP server for headless mode |
| `-DENABLE_WINAMP_PLUGINS=ON/OFF` | ON | Winamp visualization plugin support |
| `-DENABLE_MPRIS=ON/OFF` | ON (Linux) | MPRIS2 D-Bus integration |
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
cmake -B build -DBUILD_TESTING=ON
cmake --build build -j
cd build && ctest --output-on-failure
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
