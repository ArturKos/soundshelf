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
| core     | LibraryManager                  | done          |
| core     | DiscManager                     | done          |
| core     | PlaylistManager                 | done          |
| core     | FolderWatcher                   | done          |
| core     | HotkeyManager                   | done          |
| core     | ChromaprintEngine               | done          |
| core     | ReplayGainAnalyzer              | scaffold      |
| core     | DuplicateDetector               | done          |
| core     | Crossfader                      | done          |
| core     | Scrobbler                       | done          |
| core     | ScrobbleDrainer                 | done          |
| core     | DiscEnricher (MusicBrainz)      | done          |
| core     | PluginManager                   | done          |
| core     | MprisAdapter                    | done          |
| io       | TagInfo (TagLib)                | done          |
| io       | FolderReader                    | done          |
| io       | CDDAReader (libcdio)            | scaffold      |
| io       | CueParser                       | done          |
| io       | ImageReader                     | done          |
| io       | DiscRipper                      | done          |
| io       | FormatConverter (ffmpeg)        | done          |
| io       | PlaylistImporter / Exporter     | done          |
| data     | DatabaseManager                 | done          |
| data     | SchemaMigrator                  | done          |
| data     | PlayHistory                     | done          |
| data     | FTS5Index                       | done          |
| network  | RestClient                      | done          |
| network  | MusicBrainzClient               | done          |
| network  | AcoustIDClient                  | done          |
| network  | CoverArtClient                  | done          |
| network  | LyricsClient                    | done          |
| network  | LastFmClient                    | done          |
| network  | ListenBrainzClient              | done          |
| network  | AccurateRipClient               | done          |
| network  | NetworkLibrary                  | done          |
| network  | HttpServer (headless mode)      | done (verified) |
| plugins  | VisualizationPlugin (abstract)  | done          |
| plugins  | NativeVisPlugin (spectrum bars) | done          |
| plugins  | WinampVisAdapter (vis_*.dll)    | done (shell)  |
| ui       | MainWindow                      | scaffold      |
| ui       | ThemeManager                    | scaffold      |
| ui       | PlayerWidget                    | done          |
| ui       | EqualizerWidget                 | done          |
| ui       | SpectrumWidget                  | done          |
| ui       | LibraryView                     | done          |
| ui       | DiscView                        | done          |
| ui       | LyricsWidget                    | done          |
| ui       | StatsWidget                     | done          |
| ui       | TrayIcon                        | done          |
| ui       | BatchTagEditor                  | done          |
| ui       | DuplicateDialog                 | done          |
| ui       | SmartPlaylistBuilder            | done          |
| ui       | PreferencesDialog               | done          |
| ui       | ConverterDialog                 | done          |
| ui       | DiscReadDialog                  | done          |
| cli      | CLIController                   | scaffold      |

`scaffold` rows are non-trivial code that boots and links but still
depends on a backend feature that is not yet wired up — typically
PCM-tap from PlayerEngine for the analyser-style modules and the full
libcdio/CDDA path. `done (shell)` means the public API is complete and
DLLs load, but the platform-specific work (Winamp `winampVisModule`
binding, full MPRIS PropertiesChanged emission) still needs the
respective platform headers to be vendored in.

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
