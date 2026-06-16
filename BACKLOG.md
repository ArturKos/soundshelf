# BACKLOG — finite implementation scope (source of truth for the architect agent)

This file is the **authoritative, finite checklist** the autonomous workflow's
architect agent works from. The architect must:

1. Read this file every iteration.
2. **Verify each `⬜` item against the actual code** (it may already be done).
3. Tick it `✅` when fully satisfied (feature + Doxygen + unit tests that build &
   pass + clean Linux *and* Windows build), updating this file.
4. Pick the **next `⬜`** item as the iteration's task.
5. When **every item below is `✅`**, emit `all_implemented` and STOP.

**Do NOT invent new scope.** Speculative refactors, extra "nice to have" tests,
micro-helpers, or cosmetic polish are NOT tasks. If only items in "Out of scope"
remain, the project is complete → `all_implemented`.

---

## A. The 20 accepted improvements (the product scope)

| # | Feature | Status |
|---|---------|--------|
| 1 | AcoustID / Chromaprint fingerprinting | ✅ |
| 2 | ReplayGain / EBU R128 | ✅ |
| 3 | Folder watcher (auto-import) | ✅ |
| 4 | CUE sheet support (incl. multi-FILE) | ✅ |
| 5 | Graphic equalizer (10-band + presets) | ✅ |
| 6 | Tray + global hotkeys + MPRIS2 (SMTC → out of scope) | ✅ |
| 7 | Crossfade + gapless (fade-out; true overlap → out of scope) | ✅ |
| 8 | Play history + statistics | ✅ |
| 9 | Smart playlists (rule-based) | ✅ |
| 10 | Duplicate detector | ✅ |
| 11 | Batch tag editor | ✅ |
| 12 | Podcasts / audiobooks (feeds, episodes, bookmarks) | ✅ |
| 13 | Library + playlist export/import (M3U/PLS/XSPF + catalog) | ✅ |
| 14 | AccurateRip verification | ✅ |
| 15 | Format converter (ffmpeg) | ✅ |
| 16 | Lyrics (LRCLib + LRC parser + USLT/SYLT) | ✅ |
| 17 | Themes (dark / amber CRT / phosphor / light) | ✅ |
| 18 | MPRIS2 / D-Bus API | ✅ |
| 19 | Headless server (REST + remote client + Range streaming) | ✅ |
| 20 | Seed unknown discs to MusicBrainz (submitter) | ✅ |

## B. Core architecture modules (ARCHITECTURE.md)

| Area | Status |
|------|--------|
| Schema + migrations (001–008) | ✅ |
| TagInfo (TagLib read/write) | ✅ |
| DiscReader: Folder / CDDA / Image / CUE | ✅ |
| PcmDecoder (shared ffmpeg PCM source) | ✅ |
| PlayerEngine (libmpv): playback, EQ, spectrum FFT, crossfade | ✅ |
| MainWindow + UI widgets wired end-to-end | ✅ |
| Network clients (MusicBrainz, AcoustID, Last.fm, ListenBrainz, CoverArt, Lyrics, AccurateRip, Remote) | ✅ |
| HTTP server (headless `--serve`, bearer token, Range) | ✅ |
| Scrobbler + offline drainer | ✅ |
| CLI: all subcommands wired | ✅ |
| Cross-platform build (Linux + Windows/vcpkg) + CI | ✅ |

## C. Out of scope — DO NOT implement (autonomously unverifiable / future work)

These require hardware, a running GUI/display, deploy artifacts, or are explicit
"future work". They are **not** backlog tasks and must not be picked:

- True simultaneous-overlap crossfade via a 2nd libmpv handle.
- Real-time libmpv PCM tap feeding the live visualizer (`pushVisualizationPcm`
  is the seam; wiring the tap needs a running audio device).
- Winamp vis `*.dll` runtime test (needs a Windows GUI session + a sample DLL +
  deployed `libmpv-2.dll`).
- Windows SMTC (System Media Transport Controls) — Windows-only, needs a desktop session.
- GUI visual/UX polish, additional translations beyond the seed `.ts`.
- Speculative refactors or extra tests for already-passing, already-covered code.

---

### Completion

When sections A and B are all `✅` (verified against the code, tests green on
Linux, builds clean on Linux and Windows), the architecture is **complete** —
emit `all_implemented`. Everything else lives in section C and is the human's call.
