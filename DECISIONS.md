# DECISIONS.md — Architecture Decision Records

Każda znacząca decyzja projektowa udokumentowana w formacie ADR.
Format: status, kontekst, decyzja, konsekwencje.

---

## ADR-001: Qt 6 Widgets, nie Qt Quick / QML

**Status:** zaakceptowane
**Data:** iteracja 1

**Kontekst:** Aplikacja desktop, retro/minimalistyczny styl, dużo gęstych
list i tabel (biblioteka muzyczna z 100k+ rekordów). Potrzeba szybkiego renderingu
list, integracja z natywnym wyglądem systemu.

**Decyzja:** Qt Widgets z `QStyle::Fusion`.

**Konsekwencje:**
- ✅ Lepsze tabele (`QTableView` + `QSortFilterProxyModel`) niż Qt Quick `TableView`.
- ✅ Mniejszy overhead startowy.
- ✅ Native look-and-feel out-of-the-box.
- ❌ Brak nowoczesnych animacji (akceptowalne dla retro stylu).
- ❌ Mniej "modne" — ale to nie jest aplikacja konsumencka.

---

## ADR-002: libmpv jako audio backend

**Status:** zaakceptowane
**Data:** iteracja 3

**Kontekst:** QtMultimedia ma niespójną obsługę kodeków cross-platform.
GStreamer (Linux), DirectShow/MediaFoundation (Windows), AVFoundation (macOS) —
każdy z innymi codeami i bugami.

**Decyzja:** libmpv (wraps FFmpeg) jako jedyny audio backend.

**Konsekwencje:**
- ✅ Wszystkie kodeki przez FFmpeg (MP3, FLAC, OGG, OPUS, AAC, WAV, ALAC, APE, WV).
- ✅ Gapless playback out-of-the-box.
- ✅ Audio filter graph (EQ, ReplayGain, sample rate conversion).
- ✅ Streaming z URL.
- ❌ +10MB binarki.
- ❌ LGPL — akceptowalne, możemy linkować dynamicznie.
- ❌ Wymaga nauki API mpv (nie banalne).

---

## ADR-003: SQLite, nie Postgres/MySQL

**Status:** zaakceptowane
**Data:** iteracja 1

**Kontekst:** Aplikacja desktop musi działać out-of-the-box. Setup serwera
bazy by zabił UX.

**Decyzja:** SQLite 3 z WAL i FTS5.

**Konsekwencje:**
- ✅ Zero config — jeden plik `~/.local/share/soundshelf/library.db`.
- ✅ FTS5 dla full-text search z `remove_diacritics`.
- ✅ Wystarczy na 1M+ utworów.
- ✅ Library jako zip → dump SQLite + folder muzyki.
- ❌ Brak współdzielenia bazy między użytkownikami (ale to nie jest cel).
- ❌ Concurrent writes ograniczone (ale WAL pomaga).

**Powiązane:** ADR-014 (FTS5 z `remove_diacritics 2`).

---

## ADR-004: TagLib dla tagów

**Status:** zaakceptowane
**Data:** iteracja 1

**Kontekst:** Format tagów audio: ID3v1, ID3v2.3, ID3v2.4, APEv2, Vorbis Comment,
FLAC blocks, MP4 atoms, WavPack, AIFF chunks. Pisanie własnego parsera dla każdego
to robota na pół roku z buggy edge case'ami.

**Decyzja:** TagLib jako jedyne API tagów. Wrapper `TagInfo` w `core/`.

**Konsekwencje:**
- ✅ Jedno API dla wszystkich formatów (`TagLib::FileRef::tag()`).
- ✅ Battle-tested.
- ✅ LGPL.
- ❌ Brak natywnego wsparcia dla niektórych egzotycznych pól (np. ReplayGain w
  niektórych kodekach) — czasami trzeba czytać raw frames.

---

## ADR-005: libcdio + paranoia dla CD

**Status:** zaakceptowane (z zastrzeżeniem GPL)
**Data:** iteracja 2

**Kontekst:** Czytanie CD-DA cross-platform jest trudne. cdparanoia, libcdio,
QCDMetadata — kilka opcji.

**Decyzja:** libcdio + libcdio-paranoia.

**Konsekwencje:**
- ✅ Standard de facto, ten sam kod Linux + Windows.
- ✅ Paranoia mode dla błędnych płyt.
- ❌ **GPL** — to oznacza, że SoundShelf staje się GPL.
  Akceptujemy. Jeśli kiedyś będziemy chcieli LGPL/Apache, można:
  1. Wydzielić rip do osobnego procesu (subprocess).
  2. Użyć tylko libcdio (LGPL) bez paranoia (gorzej dla scratched CD).

**Powiązane:** ADR-008 (licencja całości).

---

## ADR-006: Disc jako encja pierwszej klasy

**Status:** zaakceptowane
**Data:** iteracja 2 (kluczowa decyzja od usera)

**Kontekst:** Klasyczne biblioteki muzyczne mają `albums` (zbiór po `album_id`).
User chce katalogować *płyty* — fizyczne CD i logiczne foldery.

**Decyzja:** Tabela `discs` z `type` ∈ {`physical`, `folder`, `image`, `remote`}.
Tabela `albums` w tym schemacie pominięta — `discs.title` i `discs.artist_id`
służą jako "album". Może wrócić w przyszłości jeśli potrzeba (np. album wydany
na 2 CD jako osobne `disc_number`).

**Konsekwencje:**
- ✅ Naturalny model dla *fizycznej kolekcji* (CD = jedna płyta).
- ✅ TOC + discid jako stały fingerprint — ta sama płyta zawsze ten sam wpis.
- ✅ Różne wydania (CD/winyl/digital) jako osobne `Disc`.
- ❌ Klasyczny "album" jako logiczna grupa wydań nie jest jawnie modelowany.
  Jeśli okaże się potrzebny, dodamy tabelę `releases` z `release_group_id`.

---

## ADR-007: SmartPlaylist rules jako JSON

**Status:** zaakceptowane
**Data:** iteracja 3

**Kontekst:** Smart playlisty mają strukturę drzewiastą: match all/any × N rules,
gdzie reguła ma field/op/value. Można normalizować na osobne tabele
(`playlist_rules`, `playlist_rule_values`) lub trzymać jako JSON w jednej kolumnie.

**Decyzja:** JSON w `playlists.smart_rules_json`. Schema wersjonowana.

**Konsekwencje:**
- ✅ Czytelne (jeden SELECT).
- ✅ Łatwe export/import playlist.
- ✅ Brak joinów przy ewaluacji.
- ❌ Nie da się szukać "playlists with rule X" przez SQL bez JSON1 extension
  (akceptowalne — to rzadki use case).

---

## ADR-008: Licencja projektu — GPL v3

**Status:** zaakceptowane
**Data:** iteracja 3

**Kontekst:** Część zależności (libcdio-paranoia, FFTW3) jest GPL.
W praktyce wymusza to GPL na całości.

**Decyzja:** GPL v3.

**Alternatywy odrzucone:**
- LGPL: musielibyśmy zastąpić paranoia → gorszy rip
- Apache 2.0: wymusiłaby zamiana FFTW na KissFFT/PFFFT (do zrobienia, ale +praca)
- BSD: jak wyżej

**Konsekwencje:**
- ✅ Wszystkie dependencies kompatybilne.
- ✅ Forki muszą zostać open source (zgodne z duchem projektu).
- ❌ Niektórzy enterprise userzy mogą się wzdragać (akceptowalne — to nie jest produkt B2B).

---

## ADR-009: Wybór języka — angielski jako źródłowy

**Status:** zaakceptowane
**Data:** iteracja 2

**Kontekst:** User polski, ale chce wsparcia EN/PL/DE/FR.

**Decyzja:** Wszystkie stringi w kodzie po angielsku w `tr("...")`. Tłumaczenia
PL/DE/FR w `translations/*.ts`. Domyślny język: detekcja `QLocale::system()`,
fallback EN.

**Konsekwencje:**
- ✅ Standard Qt workflow (lupdate/lrelease).
- ✅ Kontrybucje tłumaczeń łatwe (Qt Linguist).
- ✅ Dla CLI: też tłumaczone, przez `QCoreApplication::translate()`.
- ❌ Polski programista czasami pisze polskie stringi w kodzie pomyłkowo —
  trzeba pilnować w code review.

---

## ADR-010: ReplayGain w bazie + tagach (redundancja)

**Status:** zaakceptowane
**Data:** iteracja 3

**Kontekst:** ReplayGain może być w tagach (`REPLAYGAIN_TRACK_GAIN`) lub w bazie.
Tagi mogą być modyfikowane przez inne aplikacje, baza jest pod naszą kontrolą.

**Decyzja:** Trzymamy w obu miejscach. Baza jest "source of truth" dla
SoundShelfa. Przy zapisie zapisujemy też do tagów (jeśli `Settings::writeReplayGainToTags`).

**Konsekwencje:**
- ✅ Jeśli inna aplikacja przelicza RG, my widzimy update przy następnym scan.
- ✅ Jeśli plik jest kopiowany na inny komputer, RG jedzie z tagami.
- ❌ Drobna redundancja (akceptowalne).

---

## ADR-011: D-Bus dla CLI ↔ GUI IPC (Linux)

**Status:** zaakceptowane
**Data:** iteracja 3

**Kontekst:** CLI musi delegować odtwarzanie do działającego GUI (jeden audio
output). Musi też móc działać samodzielnie gdy GUI nie chodzi.

**Decyzja:** D-Bus na Linuksie (`org.soundshelf.Player`). Named pipe na Windows
(`\\.\pipe\soundshelf`). CLI próbuje połączyć się z istniejącą instancją,
fallback na własny PlayerEngine.

**Konsekwencje:**
- ✅ Standardowy mechanizm na Linuksie.
- ✅ Automatyczna integracja z GNOME/KDE Now Playing widgets (gratis przez MPRIS2).
- ❌ Windows musi mieć własną implementację (named pipe + JSON-RPC).

---

## ADR-012: Plugin API z dwoma typami

**Status:** zaakceptowane
**Data:** iteracja 1

**Kontekst:** User wymaga obsługi **Winamp visualization plugins** (vis_*.dll).
Chcemy też mieć natywne pluginy w przyszłości.

**Decyzja:** `PluginManager::loadPlugin(path)` najpierw szuka symbolu
`soundshelf_plugin_init` (natywny ABI), potem `winampVisGetHeader` (Winamp ABI).
Każdy plugin owijany w jednolity interfejs `VisualizationPlugin`.

**Konsekwencje:**
- ✅ Ekosystem Winamp pluginów dostępny od razu.
- ✅ Natywny API może być bardziej nowoczesne (RGBA buffer, Vulkan etc.).
- ❌ Winamp DLL na Linuksie wymaga Wine — opcjonalne, włączane explicite.
- ❌ Winamp ABI używa HWND (Win32), trzeba go emulować na Linux (wrapper X11).

---

## ADR-013: HTTP server jako Qt 6.4+ QHttpServer

**Status:** zaakceptowane
**Data:** iteracja 3

**Kontekst:** Tryb headless (#19) wymaga REST API. Opcje: napisać samemu z
`QTcpServer`, użyć `cpp-httplib`, użyć `QHttpServer` (Qt 6.4+).

**Decyzja:** `QHttpServer`.

**Konsekwencje:**
- ✅ Native Qt — automatyczna integracja z event loop, signals/slots.
- ✅ Range request support gratis.
- ✅ Async by default.
- ❌ Wymaga Qt 6.4+ (akceptowalne).
- ❌ Mniej dojrzałe niż cpp-httplib (akceptowalne — tylko REST API, bez
  zaawansowanych use case'ów jak websockets).

---

## ADR-014: FTS5 z `unicode61 remove_diacritics 2`

**Status:** zaakceptowane
**Data:** iteracja 2

**Kontekst:** User polski, francuski (Édith Piaf), niemiecki — diakrytyki są
częste. Wyszukiwanie "oxygene" musi znaleźć "Oxygène", "slowik" → "słowik".

**Decyzja:** Tokenizer `unicode61 remove_diacritics 2` (NFKD).

**Konsekwencje:**
- ✅ Search "just works" niezależnie od layout klawiatury.
- ✅ Standardowa opcja FTS5, nie wymaga custom code.

---

## ADR-015: AcoustID opcjonalne, nie domyślne

**Status:** zaakceptowane
**Data:** iteracja 3

**Kontekst:** Chromaprint generation jest powolne (~kilka sekund per utwór dla
długich plików). Dla biblioteki 1842 utworów to ~30 minut.

**Decyzja:** Fingerprinting domyślnie wyłączony. User włącza w ustawieniach lub
przez CLI (`soundshelf fingerprint --all`). Działa też on-demand dla pojedynczych
utworów (np. w batch tag editor → "AcoustID lookup").

**Konsekwencje:**
- ✅ Pierwszy import biblioteki nie zabiera 30 minut.
- ✅ User decyduje, kiedy uruchomić (np. w tle przed snem).
- ❌ Duplicate detector po AcoustID działa tylko dla utworów już ofingerprintowanych.

---

## ADR-016: ThemeManager z QSS, nie custom rendering

**Status:** zaakceptowane
**Data:** iteracja 3

**Kontekst:** User chce 4 motywy: Modern dark, Amber CRT, Phosphor, Light.

**Decyzja:** Qt Style Sheets (QSS) per-motyw, plik `resources/themes/*.qss`.
`ThemeManager::applyTheme(name)` wczytuje QSS i ustawia palette.

**Konsekwencje:**
- ✅ Edytowalne bez kompilacji (user może dodać własny motyw).
- ✅ Standardowy mechanizm Qt.
- ❌ QSS ma swoje ograniczenia (np. brak gradientów na niektórych widgetach
  na Windows). Akceptowalne — retro styl nie ma gradientów.

---

## ADR-017: C++20

**Status:** zaakceptowane
**Data:** iteracja 3

**Kontekst:** C++17 vs C++20.

**Decyzja:** C++20.

**Konsekwencje:**
- ✅ `std::format` (zastępuje `QString::arg` w niektórych miejscach).
- ✅ `concepts` dla template constraints.
- ✅ `ranges` dla operacji na kolekcjach.
- ✅ `std::optional`, `std::variant` (już w 17, ale w 20 są lepsze).
- ❌ Wymaga GCC 11+ / Clang 13+ / MSVC 19.30+ (akceptowalne).

---

## ADR-018: Migracje bazy nie są edytowalne

**Status:** zaakceptowane
**Data:** iteracja 3

**Kontekst:** Schema będzie ewoluować. Nie chcemy łamać istniejących baz userów.

**Decyzja:** Każda zmiana schematu = nowa migracja `migrations/NNN_name.sql`.
Stare migracje są **read-only**. `SchemaMigrator` przy starcie sprawdza
`schema_version` i aplikuje brakujące w kolejności.

**Konsekwencje:**
- ✅ Backward compatibility za darmo.
- ✅ Łatwe debugowanie ("co zmieniło się w 005?").
- ❌ Migracje muszą być idempotent / safe (CREATE TABLE IF NOT EXISTS, etc.).

---

## ADR-019: Gapless domyślnie, crossfade opcjonalnie

**Status:** zaakceptowane
**Data:** iteracja 3

**Kontekst:** Płyty koncepcyjne (Dark Side of the Moon, Autobahn) wymagają
gapless. Crossfade to inny use case (DJ-style mixing).

**Decyzja:**
- Gapless: domyślnie ON dla utworów z tej samej płyty (`tracks.disc_id` zgodne).
- Crossfade: domyślnie OFF, włączane w ustawieniach z czasem 2-8s.

**Konsekwencje:**
- ✅ "Just works" dla audiofilów.
- ✅ DJ-style mixing dostępny dla tych, którzy chcą.

---

## ADR-020: Scrobble queue zamiast natychmiastowego POST

**Status:** zaakceptowane
**Data:** iteracja 3

**Kontekst:** Last.fm/ListenBrainz POST może się nie udać (offline, rate limit).
Nie chcemy gubić scrobble.

**Decyzja:** Tabela `scrobble_queue` z `sent` flagą. Po `track_ended` event
INSERT do queue. Worker thread co 30 sekund próbuje wysłać niewysłane scrobble.

**Konsekwencje:**
- ✅ Robustness — można odsłuchiwać offline, scrobble dotrą po reconnect.
- ✅ Łatwe debugowanie ("ile zalega w queue?" → `soundshelf scrobble status`).
- ❌ Drobny overhead bazy (akceptowalne — kilka KB dla tysięcy scrobble).
