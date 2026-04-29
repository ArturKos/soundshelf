# SoundShelf

> Cross-platform audio catalog and player with retro-unix aesthetics.

A self-hosted music library for people who care about their physical and digital
collection — CDs, FLAC archives, MP3 folders, all in one place. Built with Qt 6,
SQLite, and libmpv.

## Features

- **Catalogs everything:** MP3, FLAC, OGG, OPUS, AAC, WAV, ALAC, APE, WV
- **First-class disc support:** physical CD-DA (libcdio + paranoia), folder-as-disc, CUE images, remote
- **MusicBrainz integration:** disc ID lookup, AcoustID fingerprinting, Cover Art Archive
- **Built-in player:** libmpv backend, gapless playback, 10-band EQ, ReplayGain (EBU R128)
- **Winamp visualization plugins:** load any vis_*.dll
- **Smart playlists:** rule-based, live-updating
- **Batch tag editor, duplicate detector, format converter**
- **Headless mode:** REST API server for remote access (web client / mobile)
- **CLI:** full feature parity with GUI, scriptable, JSON output
- **Multi-language:** English, Polski, Deutsch, Français
- **Themes:** Modern dark, Amber CRT, Phosphor, Light

## Status

Pre-alpha — actively under construction. See `CLAUDE.md` for the project context
and `ARCHITECTURE.md` for layer boundaries. The matrix below tracks what is
already wired up vs. still a stub.

### Implementation status

| Layer    | Module                          | Status        |
|----------|---------------------------------|---------------|
| core     | Track / Disc structs            | done          |
| core     | Translator                      | done          |
| core     | SettingsManager                 | done          |
| core     | SmartPlaylistEvaluator          | done          |
| core     | PlayerEngine (libmpv)           | scaffold      |
| core     | LibraryManager                  | stub          |
| core     | DiscManager                     | stub          |
| core     | PlaylistManager                 | stub          |
| core     | FolderWatcher                   | stub          |
| core     | HotkeyManager                   | stub          |
| core     | ChromaprintEngine               | stub          |
| core     | ReplayGainAnalyzer              | stub          |
| core     | DuplicateDetector               | stub          |
| core     | Crossfader                      | stub          |
| core     | Scrobbler                       | stub          |
| core     | PluginManager                   | stub          |
| core     | MprisAdapter                    | stub          |
| io       | TagInfo (TagLib)                | done          |
| io       | FolderReader                    | done          |
| io       | CDDAReader (libcdio)            | scaffold      |
| io       | CueParser                       | stub          |
| io       | ImageReader                     | stub          |
| io       | DiscRipper                      | stub          |
| io       | FormatConverter (ffmpeg)        | stub          |
| io       | PlaylistImporter / Exporter     | stub          |
| data     | DatabaseManager                 | done          |
| data     | SchemaMigrator                  | done          |
| data     | PlayHistory                     | stub          |
| data     | FTS5Index                       | stub          |
| network  | RestClient                      | stub          |
| network  | MusicBrainz / AcoustID / ...    | stub          |
| network  | HttpServer (headless mode)      | stub          |
| plugins  | VisualizationPlugin / Winamp    | stub          |
| ui       | MainWindow                      | scaffold      |
| ui       | ThemeManager                    | scaffold      |
| ui       | other widgets/dialogs           | stub          |
| cli      | CLIController                   | scaffold      |

This file is updated as the stubs land their first implementations.

## Quick start

```bash
# Build (Linux)
sudo apt install qt6-base-dev qt6-tools-dev libmpv-dev libtag1-dev \
                 libcdio-paranoia-dev libchromaprint-dev libebur128-dev \
                 libdiscid-dev libsqlite3-dev libfftw3-dev cmake g++

git clone https://github.com/ArturKos/soundshelf.git
cd soundshelf
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSOUNDSHELF_BUILD_TESTS=ON
cmake --build build -j

# Run GUI
./build/soundshelf

# Run CLI
./build/soundshelf-cli list --fmt json | jq '.[0:5]'

# Run as headless server
./build/soundshelf --serve --port 8080 --bind 0.0.0.0

# Run unit tests
ctest --test-dir build --output-on-failure
```

See `BUILD.md` for Windows and detailed build instructions.

## Documentation

- **`CLAUDE.md`** — project context, intended for AI assistants and new contributors
- **`ARCHITECTURE.md`** — technical architecture, data flow diagrams
- **`DECISIONS.md`** — Architecture Decision Records (ADRs)
- **`BUILD.md`** — build instructions per platform
- **`docs/html/index.html`** — generated API reference (Doxygen, see below)

### Generating Doxygen documentation

Source files use Doxygen / JavaDoc-style comments (`///`, `/** ... */`).
Configuration lives in `Doxyfile` at the repo root.

```bash
# Install the toolchain
sudo apt install doxygen graphviz

# Generate HTML docs into docs/html/
doxygen Doxyfile

# Open the result
xdg-open docs/html/index.html
```

`Doxyfile` enables `EXTRACT_ALL`, treats `README.md` as the main page, and
expands the Qt-specific macros (`Q_OBJECT`, `Q_SIGNALS`, `Q_SLOTS`, ...) so the
class graphs render correctly. Output is written to `docs/html/` and is
gitignored.

## License

GPL v3. See `LICENSE` for the full text.

The GPL choice is forced by some dependencies (libcdio-paranoia, FFTW3).
See `DECISIONS.md` ADR-008.

## Acknowledgments

- Qt — application framework
- libmpv — audio playback
- TagLib — tag reading/writing
- libcdio + paranoia — CD reading
- libchromaprint + AcoustID — fingerprinting
- libebur128 — loudness analysis
- MusicBrainz, Cover Art Archive, Last.fm, ListenBrainz, LRCLib — metadata
