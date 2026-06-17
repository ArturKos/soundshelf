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

## D. Additional accepted tasks (added by user 2026-06-16)

Pick these in order. They ARE in scope and gate completion.

| # | Task | Status |
|---|------|--------|
| D1 | **Fix GitHub Actions CI** — it currently fails on *every* commit. Investigate `.github/workflows/ci.yml` and the failing runs (`GH_TOKEN=$(cat ~/git_token) gh run list` / `gh run view <id> --log-failed`), find the real cause, and fix the workflow so both the Linux and Windows jobs pass. Verify by checking a fresh run after the fix is pushed. | ✅ |
| D2 | **CLI test suite covering all functionality** — add automated tests that exercise every `soundshelf-cli` subcommand (play/pause/resume/stop/seek/volume/status, import/list/search/info, tag, disc list/search/add/tracks/play, replaygain, fingerprint, convert, duplicates, playlist, export, stats, scrobble, db migrate/vacuum/info, podcast, remote, serve, plugin). Build a harness that runs the **built** `soundshelf-cli` against a temp DB + generated audio fixtures (ffmpeg) and asserts expected output / exit codes. Register it so `ctest` runs it; must pass on Linux. | ✅ |
| D3 | **Equalizer has no audible effect** — the UI is correctly wired (EqualizerWidget→PlayerEngine `setEqualizerBand`/`setEqualizerEnabled`/`setEqualizerPreset`, `attachEngine` in MainWindow), so the bug is in PlayerEngine's mpv audio-filter chain (`buildAudioFilterChain`/`applyAudioFilters`). Verify the `af` string mpv actually accepts — the current `lavfi=[equalizer=f=..:t=q:w=..:g=..]` form is suspect; mpv may need `af=equalizer=...` or `firequalizer`/`anequalizer`. Confirm `mpv_set_property("af", …)` returns no error, and that the chain is re-applied after every `loadfile`. Add a unit test asserting `buildAudioFilterChain()` yields the expected mpv-valid string for given bands/preset. **Verification:** the loop validates the filter string + that setting the mpv `af` property logs no error; audible confirmation is the human's (not an autonomous gate). | ⬜ |
| D4 | **Spectrum shows nothing** — `PlayerEngine::pushVisualizationPcm()` is never called by anything, so `spectrumData()` always returns zeros and SpectrumWidget draws an empty field. Wire a **live PCM source** feeding the visualizer during playback. libmpv has no direct PCM callback, so a workable approach: a lightweight parallel `io::PcmDecoder` stream of the current file advanced in step with `PlayerEngine::positionMs()`, pushing frames via `pushVisualizationPcm()` on a timer, reset on track change / pause / stop. **Verification:** the loop confirms the seam fires (a test where, given a decodable file + simulated position, `spectrumData()` becomes non-zero); the live on-screen bars are the human's to confirm. | ⬜ |

## C. Out of scope — DO NOT implement (autonomously unverifiable / future work)

These require hardware, a running GUI/display, deploy artifacts, or are explicit
"future work". They are **not** backlog tasks and must not be picked:

- True simultaneous-overlap crossfade via a 2nd libmpv handle.
- Winamp vis `*.dll` runtime test (needs a Windows GUI session + a sample DLL +
  deployed `libmpv-2.dll`).
- Windows SMTC (System Media Transport Controls) — Windows-only, needs a desktop session.
- GUI visual/UX polish, additional translations beyond the seed `.ts`.
- Speculative refactors or extra tests for already-passing, already-covered code.

---

### Completion

When sections A, B and D are all `✅` (verified against the code, tests green on
Linux, builds clean on Linux and Windows, CI green), the architecture is
**complete** — emit `all_implemented`. Everything else lives in section C and is
the human's call.
