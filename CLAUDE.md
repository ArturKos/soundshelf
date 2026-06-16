# CLAUDE.md — kontekst projektu dla Claude Code

> Ten plik jest specjalnie przygotowany dla Claude Code (i innych asystentów LLM)
> pracujących nad projektem SoundShelf. Trzymaj go zawsze pod ręką — opisuje
> intencje, decyzje, ograniczenia i to, czego *nie* robimy.

---

## TL;DR

**SoundShelf** to cross-platform (Linux + Windows) katalog plików audio z wbudowanym
odtwarzaczem i CLI. Stack: Qt 6 / C++ / SQLite / libmpv / TagLib. Styl wizualny:
retro unix, minimalistyczny — inspiracje to Winamp, mc, ranger, cmus, Foobar2000.

Aplikacja ma być:

- **biblioteką** (jak Foobar/iTunes) — kataloguje, taguje, wyszukuje
- **odtwarzaczem** (jak Winamp) — z EQ, wizualizacjami, pluginami Winamp vis
- **dyskotekarzem** (specyficzne dla nas) — kataloguje *płyty* (CD-DA, foldery, obrazy CUE) jako encje pierwszej klasy
- **serwerem** (jak Plex/Jellyfin) — headless mode, REST API, klient zdalny

---

## Geneza projektu (z rozmowy projektowej)

User (Artur, Polska) prowadzi domową infrastrukturę self-hosted (Home Assistant,
Traccar, OSRM w Dockerze). SoundShelf zaprojektowany jest tak, żeby pasować
do tego ekosystemu — uruchamiany albo jako pełny GUI, albo jako headless serwer
na Raspberry Pi z biblioteką serwowaną do innych urządzeń.

Projekt powstawał iteracyjnie:

1. **Iteracja 1:** podstawowy katalog MP3/FLAC/OGG/AAC/OPUS/WAV, ID3v1/v2,
   Winamp-style player + spectrum, Winamp vis pluginy, sieć, CLI, SQLite
2. **Iteracja 2:** *płyty* jako encje pierwszej klasy (fizyczne CD przez libcdio,
   foldery, obrazy CUE), wyszukiwanie po albumach, i18n EN/PL/DE/FR
3. **Iteracja 3 (obecna):** 20 dodatkowych usprawnień — patrz lista poniżej

---

## 20 usprawnień, które zaakceptowaliśmy

Pełna lista jest realizowana — nie traktuj ich jako "nice to have", to wymagania.

| # | Funkcja | Moduł odpowiedzialny | Biblioteka |
|---|---|---|---|
| 1 | AcoustID / Chromaprint fingerprinting | `core::ChromaprintEngine`, `network::AcoustIDClient` | `libchromaprint` |
| 2 | ReplayGain / EBU R128 | `core::ReplayGainAnalyzer` | `libebur128` |
| 3 | Watcher katalogów (auto-import) | `core::FolderWatcher` | `QFileSystemWatcher` |
| 4 | CUE sheet support (jeden duży plik = wiele tracków) | `io::CueParser` | własna implementacja |
| 5 | Korektor graficzny (10-band EQ + presety) | `ui::EqualizerWidget`, `core::PlayerEngine` | filtry `libmpv` `af=equalizer=...` |
| 6 | Tray + global hotkeys + MPRIS2 + SMTC | `core::TrayIcon`, `core::HotkeyManager`, `core::MprisAdapter` | `QSystemTrayIcon`, QtDBus, Windows SMTC API |
| 7 | Crossfade i gapless | `core::PlayerEngine` | `libmpv` `--gapless-audio=yes`, własny crossfader |
| 8 | Historia odsłuchów + statystyki | `data::PlayHistory`, `ui::StatsWidget` | SQLite |
| 9 | Smart playlisty (rule-based) | `core::SmartPlaylistEvaluator`, `ui::SmartPlaylistBuilder` | SQLite, JSON rules |
| 10 | Duplicate detector | `core::DuplicateDetector`, `ui::DuplicateDialog` | MD5 + AcoustID + tag matching |
| 11 | Batch tag editor | `ui::BatchTagEditor` | TagLib |
| 12 | Podcasty / audiobooki | `core::PodcastManager`, `data::Bookmark` | SQLite + RSS parser |
| 13 | Eksport/import biblioteki + playlisty (M3U/PLS/XSPF + JSON catalog) | `io::PlaylistImporter`, `io::PlaylistExporter`, `io::LibraryExporter`, `io::LibraryImporter` | własne |
| 14 | AccurateRip verification | `io::AccurateRipClient` | REST + CRC32 |
| 15 | Format converter (FFmpeg wrapper) | `io::FormatConverter` | `QProcess` → `ffmpeg` |
| 16 | Lyrics (LRCLib + USLT/SYLT) | `network::LyricsClient`, `ui::LyricsWidget` | REST + parser LRC |
| 17 | Themes (Modern dark / Amber CRT / Phosphor / Light) | `ui::ThemeManager` | QSS stylesheets |
| 18 | MPRIS2 / D-Bus API | `core::MprisAdapter` | QtDBus (Linux only; Windows = SMTC) |
| 19 | Headless mode (server) | `network::HttpServer`, `cli::DaemonController` | `QHttpServer` (Qt 6.4+) |
| 20 | Seed nieznanych płyt do MusicBrainz | `network::MusicBrainzSubmitter` | REST + OAuth |

---

## Architektura — warstwy i zasady

```
┌──────────────────────────────────────────────────────────┐
│ UI LAYER (Qt6 Widgets) ── tylko sygnały/sloty z Core    │
├──────────────────────────────────────────────────────────┤
│ CORE LAYER ── logika domenowa, agregaty, sekwencjonowanie│
├──────────────────────────────────────────────────────────┤
│ I/O LAYER ──── pliki, kodeki, taglib, libcdio, ffmpeg   │
├──────────────────────────────────────────────────────────┤
│ DATA LAYER ─── SQLite, schema migration, FTS5           │
├──────────────────────────────────────────────────────────┤
│ NETWORK LAYER ─ MusicBrainz, AcoustID, Last.fm, HTTP    │
└──────────────────────────────────────────────────────────┘
                         ▲
                         │
                  ┌──────┴──────┐
                  │ PLUGIN API  │  Winamp vis + native
                  └─────────────┘
                  ┌─────────────┐
                  │ CLI / DBUS  │  delegacja do Core
                  └─────────────┘
```

**Zasada główna:** każda warstwa zna tylko warstwy poniżej. Nigdy odwrotnie.
Plugin API i CLI to obywatele drugiej kategorii — wpinają się przez Core,
nigdy bezpośrednio do UI ani do Data.

**Anti-patterns, których unikamy:**

- ❌ UI klasa ładująca cokolwiek z `QSqlDatabase` bezpośrednio (ma być przez `LibraryManager`/`DiscManager`)
- ❌ `PlayerEngine` używający `MainWindow*` (player nie wie o UI)
- ❌ Core wołający widgety (sygnały tak, wskaźniki nie)
- ❌ `#include <Qt...>` w warstwie I/O (chyba że Qt-only typy jak `QString`, `QByteArray`)
- ❌ Hard-codedy ścieżek (zawsze przez `QStandardPaths` lub `SettingsManager`)

---

## Krytyczne decyzje projektowe (z uzasadnieniem)

### Audio backend: libmpv, nie QtMultimedia

QtMultimedia ma niespójną obsługę kodeków na Linuksie i Windows (zależna od
backendu Gstreamer/DirectShow/FFmpeg). `libmpv` daje:
- pełen FFmpeg pod spodem (wszystkie kodeki, jeden raz),
- gapless playback out-of-the-box (`--gapless-audio=yes`),
- audio filter graph (EQ, ReplayGain, sample rate conversion),
- callback PCM dla wizualizacji (`audio-channels`, `audio-samplerate`),
- streaming z URL (zdalna biblioteka),
- ten sam kod na Linux + Windows + macOS.

Wadą jest LGPL i większy binarny ślad (≈10 MB), ale to akceptowalna cena.

### Database: SQLite, nie PostgreSQL/MySQL

Aplikacja desktop powinna być self-contained. SQLite + WAL + FTS5 spokojnie
obsługuje 100k+ utworów. PostgreSQL wymagałby setupu serwera u usera —
zabija UX. Dla trybu serwerowego (#19) SQLite też wystarczy — load testy
biblioteki Plex/Jellyfin pokazują, że płaski plik SQLite jest szybszy od
zdalnej bazy dla read-heavy workloadów < 1M rekordów.

### CD reading: libcdio + paranoia

Standard de facto na Linuksie. Ma binding na Windows (cygwin DLL lub natywny
build). Alternatywa cdparanoia jest GPL, libcdio-paranoia też — to oznacza
że SoundShelf staje się GPL. **Decyzja:** akceptujemy GPL. Jeśli kiedyś będziemy
chcieli LGPL/Apache, można wydzielić rip/paranoia do osobnego procesu (subprocess)
i dynamicznie linkować — ale to optymalizacja na później.

### Tagi: TagLib, nie własny parser ID3

TagLib obsługuje ID3v1, ID3v2.3, ID3v2.4, APEv2, Vorbis Comment, FLAC blocks,
MP4 atoms, WavPack, AIFF chunks. Pisanie własnego parsera dla każdego z tych
formatów to robota na pół roku. TagLib ma jedno wspólne API (`FileRef::tag()`)
i jest battle-tested.

### Wybór języka domyślnego: angielski

User określił "domyślna to angielski ale obsługa polskiego niemieckiego francuskiego".
**Wszystkie stringi w kodzie muszą być po angielsku** w `tr("...")`. Tłumaczenia
PL/DE/FR żyją w `translations/*.ts` i są kompilowane do `.qm`. Detekcja systemu
przez `QLocale::system()` przy pierwszym uruchomieniu, później `SettingsManager::locale`.

### Search: FTS5 z `unicode61 remove_diacritics 2`

Bez `remove_diacritics` user wpisujący "oxygene" nie znajdzie "Oxygène", a polski
user szukający "slowik" nie znajdzie "słowik". Z `remove_diacritics 2` (NFKD)
to działa transparentnie. Ten sam tokenizer dla `tracks_fts` i `discs_fts`.

### Smart playlist storage: JSON w kolumnie, nie osobne tabele

Reguły smart playlist są strukturą drzewiastą (match all/any × N rules).
Normalizacja na osobne tabele (`playlist_rules`, `playlist_rule_values`)
działa, ale dorzuca dwa joiny do każdego query. Reguły są **odczytywane razem**
z playlistą — nigdy nie chcemy "wszystkich reguł z gatunku Jazz across playlists".
JSON jest czytelniejszy i nie tracimy nic ważnego.

### Disc jako encja pierwszej klasy

Klasyczne biblioteki muzyczne mają `albums` (zbiór utworów po `album_id`).
SoundShelf rozróżnia *album* (logiczna grupa nagrań) od *disc* (fizyczny lub
logiczny nośnik). Powód: jedna płyta CD-DA = jeden `Disc` z TOC i discid,
nawet jeśli artyści są różni (kompilacje, soundtracki). Dwa wydania tego samego
albumu (CD vs winyl vs digital) to dwa różne `Disc`. Tagi `disc_number` i
`track_number` są pozycjami w `Disc`, nie w `Album`.

W schemacie `albums` to tabela widokowa lub całkiem pominięta — `discs.title`
i `discs.artist_id` służą jako "album". Możemy dodać tabelę `albums` w
przyszłości jeśli potrzeba, ale na razie nie.

---

## Konwencje kodu

- **Standard:** C++20 (nie 17 — chcemy `concepts`, `ranges`, `std::format`)
- **Qt:** 6.5+ (testowane na 6.5 i 6.7)
- **Naming:** `CamelCase` dla klas, `camelCase` dla metod, `m_camelCase` dla pól, `SCREAMING_CASE` dla makr
- **Headers:** każdy z `#pragma once`, nie include guards
- **Includes:** Qt po systemowych, projektowe ostatnie (lokalne `""` po `<>`)
- **Qt smart pointers:** `QScopedPointer` dla owned-by-class, raw `QObject*` dla parented (Qt zarządza), `std::unique_ptr` dla nie-Qt
- **Sygnały/sloty:** **zawsze** nowy syntax `connect(obj, &Class::sig, this, &Other::slot)`. Stary string-based zakazany.
- **Stringi:** `tr("...")` ZAWSZE dla user-facing, raw `"..."` tylko dla SQL/keys/internal
- **Logging:** `qCDebug(category) << ...` z kategoriami (`soundshelf.player`, `soundshelf.disc`, `soundshelf.cli`)
- **Errors:** `Result<T, Error>` template (zdefiniowany w `core/Result.hpp`), nie wyjątki Qt-style
- **Style:** clang-format z konfiguracją w `.clang-format`

---

## Struktura repo

```
soundshelf/
├── CMakeLists.txt              # główny build
├── CMakePresets.json           # presety: linux-debug, windows-vcpkg, etc.
├── vcpkg.json                  # vcpkg manifest (Windows MSVC path)
├── cmake/FindMPV.cmake         # custom Find module dla pre-built libmpv
├── CLAUDE.md                   # ten plik
├── README.md                   # README usera
├── ARCHITECTURE.md             # szczegóły architektury
├── BUILD.md                    # instrukcje budowania per-platform
├── DECISIONS.md                # ADR (architecture decision records)
├── src/
│   ├── ui/                     # Qt widgety
│   ├── core/                   # logika domenowa
│   ├── io/                     # pliki, kodeki, dyski
│   ├── data/                   # SQLite, migracje
│   ├── network/                # REST clients, HTTP server
│   ├── plugins/                # plugin API, Winamp adapter
│   ├── cli/                    # CLI parser, D-Bus IPC
│   └── main.cpp                # entrypoint
├── include/soundshelf/         # publiczne headery (per-warstwa)
├── translations/               # .ts files (en/pl/de/fr)
├── migrations/                 # 001_initial.sql, 002_*.sql, ...
├── resources/                  # ikony, presety EQ, themes
├── scripts/                    # install-deps, setup-vcpkg, deploy
├── tests/                      # unit testy (Qt Test)
├── external/                   # (gitignored) pre-built mpv-dev SDK
├── vcpkg/                      # (gitignored) vcpkg clone
└── docs/                       # dokumentacja deweloperska
```

---

## Jak testować

```bash
# Build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j

# Testy
cd build && ctest --output-on-failure

# Uruchomienie
./build/soundshelf                    # GUI
./build/soundshelf-cli list           # CLI (oddzielny binar)
./build/soundshelf --serve --port 8080 # headless mode

# Testowanie z różnymi językami
QT_DEBUG_PLUGINS=1 LANG=pl_PL.UTF-8 ./build/soundshelf
./build/soundshelf-cli --lang de play "kraftwerk"
```

---

## Co Claude Code powinien robić

✅ **Tak:**
- Implementuj brakujące metody w klasach które są stub'ami (TODO w komentarzach)
- Pisz testy jednostkowe dla nowej logiki w `tests/`
- Dodawaj nowe pola tłumaczeń do `translations/*.ts` gdy dodajesz user-facing string
- Aktualizuj `migrations/` przy każdej zmianie schematu (nie edytuj starych migracji!)
- Trzymaj się warstw — UI nie woła I/O bezpośrednio
- Dodawaj `qCDebug` w nowych funkcjach z odpowiednią kategorią

❌ **Nie:**
- Nie wprowadzaj zależności bez aktualizacji `CMakeLists.txt` i tej dokumentacji
- Nie używaj `QtMultimedia` zamiast `libmpv`
- Nie blokuj UI thread — używaj `QtConcurrent::run` lub `QThread` dla I/O
- Nie używaj `printf`/`std::cout` — log idzie przez `qCDebug`
- Nie commituj plików `*.qm` (są generowane), `*.user`, `build/`
- Nie zmieniaj smart playlist JSON schema bez bumpu wersji w `core::SmartPlaylistEvaluator::SCHEMA_VERSION`

---

## Skróty klawiszowe i UX (decyzje już podjęte)

| Akcja | Skrót | Uwagi |
|---|---|---|
| Play/Pause | `Space` | tylko gdy fokus nie na input |
| Next/Prev track | `→` / `←` | (z modyfikatorem `Ctrl` = skip 30s) |
| Open file | `Ctrl+O` | pojedynczy plik |
| Add disc from drive | `Ctrl+D` | menu Disc |
| Add disc from folder | `Ctrl+Shift+A` | |
| Search | `Ctrl+F` | fokusuje search box |
| Quick switch view | `Ctrl+1..5` | All / Discs / Artists / Genres / Stats |
| Tag editor | `Ctrl+I` | (Information / Info, jak macOS) |
| Preferences | `Ctrl+,` | macOS-style |
| Toggle EQ | `Ctrl+E` | |
| Toggle visualization | `Ctrl+V` | konflikt z paste? — NIE, tylko gdy fokus na liście |
| Volume up/down | `↑` / `↓` | gdy fokus nie na input |
| Mute | `M` | |

Global hotkeys (system-wide, można wyłączyć):
- Media keys: standardowe `Play`, `Pause`, `Next`, `Prev`, `Stop`
- `Ctrl+Alt+Space` — show/hide main window

---

## Tłumaczenia — uwagi praktyczne

- **EN źródłowy** — wszystkie `tr()` po angielsku.
- **Numerus forms:** PL ma trzy (`1`, `2-4`, `5+`), DE/FR mają dwie (`1` vs `≠1`). Używaj `tr("%n track(s)", nullptr, count)` zawsze gdy w stringu jest liczba.
- **Skróty:** `&Plik` w PL, `&File` w EN — accel keys mogą być różne. Tłumacz może zmienić `&` w `.ts`.
- **Długość:** niemiecki bywa o 30% dłuższy. Layout `QHBoxLayout`/`QVBoxLayout` z `QSizePolicy::Preferred`, a nie hardcoded width. **Testuj DE i FR przed releasem.**
- **Dla CLI:** komunikaty błędów / pomocy też przez `tr()`. CLI sprawdza `--lang` lub `LANG`/`LC_ALL`.
- **Co NIE jest tłumaczone:** nazwy utworów, artystów, albumów, gatunków (gatunki przychodzą z tagów ID3, są w oryginalnym języku tagu).

---

## Bezpieczeństwo i prywatność

- **Token storage:** Last.fm/ListenBrainz/MusicBrainz tokeny w systemowym keyring — `libsecret` (Linux), Credential Manager (Windows). Fallback: zaszyfrowane w `~/.config/soundshelf/secrets.enc` (AES-256, klucz pochodny od machine ID).
- **Network:** wszystkie zewnętrzne API tylko przez HTTPS. Cert validation enabled. User-Agent: `SoundShelf/0.x (https://...)`.
- **Headless server (#19):** **wymaga** Bearer token (generowany przy `soundshelf serve --bind`). Nie pozwalamy na anonymous access.
- **CLI:** `soundshelf scrobble auth lastfm` używa OAuth flow (browser-based), nie pyta o hasło w terminalu.

---

## Status implementacji (na 2026-06-15)

> Stan po fazach A/B/C + faza D (ReplayGain, AcoustID, EQ presety, spektrum FFT,
> crossfade, dokończone komendy CLI) i ścieżce build Windows (vcpkg + MSVC static).
> Cross-platform compile + testy zweryfikowane na Linux i Windows 10.

| Moduł | Status |
|---|---|
| Schema bazy + migracje | **działa** — migracje 001–007 (replaygain, acoustid, smart_playlists, play_history, bookmarks, podcasts), `SchemaMigrator` + `DatabaseManager` pełne |
| TagInfo (TagLib wrapper) | **działa** (read+write, encoding fallback) |
| DiscReader — `FolderReader` | **działa** |
| DiscReader — `CDDAReader` | **działa** — libcdio/paranoia + discid, WAV out (kompilowane pod `SOUNDSHELF_HAVE_LIBCDIO`) |
| DiscReader — `ImageReader` / `CueParser` | **działa** — `CueParser` supports single & multi-file sheets (z testem); `ImageReader` resolves all files, probes durations (z testem) |
| `DiscRipper` | **działa** (rip + tag) |
| PcmDecoder (ffmpeg → s16le) | **działa** — wspólne źródło PCM dla RG/AcoustID/spektrum (z testem) |
| ReplayGainAnalyzer | **działa** — EBU R128 przez libebur128 (track+album), zapis tagów, `db updateReplayGain`; wynik zgodny z ffmpeg ebur128 (z testem) |
| ChromaprintEngine / AcoustID | **działa** — `fingerprintFile` przez PcmDecoder; lookup `AcoustIDClient` (wymaga klucza API) (z testem) |
| Translator + tłumaczenia | **działa**; `.ts` dla en/pl/de/fr (stringi seedowe, do uzupełnienia) |
| SmartPlaylistEvaluator | **działa** |
| PlaylistManager + import/export (M3U/PLS/XSPF) | **działa** |
| `io::LibraryExporter` / `io::LibraryImporter` (feature #13 catalog) | **działa** — portable JSON export/import całego katalogu; `toJson`/`exportToFile` + `fromJson`/`importFromFile`; envelope z version/format/track_count; ReplayGain i cue offsets jako optional (omit gdy nullopt); QDateTime jako ISO 8601 UTC; DB-local pola (id/discId/coverHash) nie są eksportowane (z testem) |
| DuplicateDetector | **działa** (z testem) |
| FormatConverter (ffmpeg) | **działa** |
| `io::PodcastFeedParser` (feature #12 parser) | **działa** — RSS 2.0 + iTunes namespace → `Feed`/`Episode` structs; `parseFile`, `parseBytes`, `parseItunesDuration` (z testem) |
| `data::PodcastStore` + migration 007 (feature #12 DB) | **działa** — `podcast_feeds` + `podcast_episodes` tables, migration 007, `subscribe`/`updateFeedMetadata`/`upsertEpisodes`/`episodesForFeed`/`episode`/`setPlayed`/`setLocalPath`/`unsubscribe` (z testem) |
| `core::PodcastManager` (feature #12 orchestration) | **działa** — `subscribe`/`refreshFeed`/`refreshAll`/`downloadEpisode`; injectable `FeedFetcher` dla testów (stub bez sieci); signals: `feedRefreshed`, `episodeDownloaded`, `errorOccurred`; logging `soundshelf.podcast.manager`; default fetcher blokuje na `RestClient::getBytes().result()` — wywoływać z wątku roboczego lub CLI (z testem). *Integracja z GUI (wątek/QtConcurrent) = future work* |
| PlayerEngine (libmpv) | **działa** — play/seek/vol/auto-advance, presety EQ z JSON, spektrum FFT (FFTW3), crossfade (fade-out przez `Crossfader`). *Future work:* prawdziwy overlap (2. instancja mpv) i PCM tap z libmpv zasilający spektrum w czasie rzeczywistym |
| MainWindow + UI | **wpięte end-to-end** (import → biblioteka → playback); większość widgetów ma realny kod |
| MPRIS adapter | **działa** (Linux/QtDBus) |
| HTTP server (headless `--serve`) | **działa** — `main.cpp --serve --port`, Bearer token, REST przez `HttpServer` |
| `network::RemoteClient` (feature #19 client) | **działa** — `buildListQuery`/`trackFromJson`/`parseTrackList`/`parseTrack`/`streamUrl` (czyste statyczne, bez sieci); `listTracks`/`track` blokujące (RestClient.getJson.result()); logging `soundshelf.network.remote` (z testem) |
| Last.fm / ListenBrainz scrobbler | **działa** — `Scrobbler` + `ScrobbleDrainer` (kolejka offline) + podpis Last.fm (z testem) |
| MusicBrainz / CoverArt / DiscEnricher | **działa** (metadata fallback + enrichment płyt) |
| `network::MusicBrainzSubmitter` (feature #20) | **działa** — Release Editor Seeding (browser-based, no OAuth): `buildSeedFields` + `buildSeedUrl`; DiscType→MB format mapping (z testem) |
| LyricsClient (LRCLib) + LyricsWidget | **działa** |
| SpectrumWidget | **działa** — wbudowany retro renderer słupków z `spectrumData()`; ustępuje miejsca aktywnemu pluginowi |
| Visualization plugins (Winamp adapter) | **kompiluje się** (oba OS); realny test na `vis_*.dll` wymaga sprzętu Windows + przykładowej DLL (manualny) |
| CLI (`soundshelf-cli`) | **działa** — wszystkie komendy okablowane do backendów (replaygain, fingerprint, convert, duplicates, playlist, export, stats, scrobble, db, disc add/tracks/play, plugin, serve, **podcast list/subscribe/refresh/episodes/download/played/unsubscribe**, **remote list/get/url**). `next/prev/daemon` i `disc rip/lookup` dają uczciwy komunikat (wymagają działającej instancji / sprzętu); IPC do GUI = future work. Globalne flagi `--server`/`--token` dla komendy `remote`. |
| Build / CI | **działa** — CMake + presety, vcpkg/MSVC static (Windows), GitHub Actions (Linux+Windows). vcpkg: `libebur128` (find_path fallback), `FFTW3f` (osobny pakiet single-precision) |
| Testy | 24 pliki (cue +4 multi-file cases, duplicate, fts5, lastfm_sign, playlist_io, pure_helpers, smart_playlist, taginfo, track_format, translator, pcm_decoder, replaygain, fingerprint, eq_presets, spectrum, accuraterip, bookmark_store, podcast_feed_parser, podcast_store, podcast_manager, test_cli_podcast, test_musicbrainz_submitter, test_library_io, **test_remote_client**) |

**Następne kroki / co zostało (future work):**
- PlayerEngine: prawdziwy overlap crossfade (2. instancja mpv); PCM tap z libmpv zasilający `spectrumData`/wizualizacje w czasie rzeczywistym (dziś `pushVisualizationPcm` trzeba zasilić ręcznie)
- Visualization: test na realnej Winamp vis DLL (Windows + przykładowa DLL)
- CLI: IPC (D-Bus/named pipe) do działającego GUI dla `next/prev/daemon`
- AcoustID: konfiguracja klucza API (`acoustid.api_key`) w Preferencjach
- Tłumaczenia: uzupełnić `.ts` poza seedem
- PodcastManager: integracja z GUI (wątek roboczy / `QtConcurrent::run` żeby nie blokować UI thread)

Integracja z bibliotekami systemowymi: `qt6 libmpv taglib libcdio chromaprint libebur128 fftw3 ffmpeg`.

---

## Kontakt z autorem decyzji projektowych

Projekt powstał w rozmowie iteracyjnej. Jeśli natkniesz się na coś, co nie jest
opisane tutaj ani w `ARCHITECTURE.md` ani w `DECISIONS.md` — **zapytaj usera
zanim podejmiesz decyzję, która zmienia obraz całości**. Nie zgaduj, czy "może
powinniśmy dodać Qt Quick" — odpowiedź na pewno brzmi "nie, mamy decyzję na Qt Widgets".
