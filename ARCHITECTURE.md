# ARCHITECTURE.md — szczegóły techniczne

## Spis treści
1. [Model warstwowy](#model-warstwowy)
2. [Player pipeline](#player-pipeline)
3. [Disc reading pipeline](#disc-reading-pipeline)
4. [Library import pipeline](#library-import-pipeline)
5. [Plugin loading](#plugin-loading)
6. [Network architecture](#network-architecture)
7. [CLI ↔ GUI IPC](#cli--gui-ipc)
8. [Threading model](#threading-model)

---

## Model warstwowy

```
                    ┌──────────────────┐
                    │   main.cpp       │
                    │  (entrypoint)    │
                    └────────┬─────────┘
                             │
        ┌────────────────────┼────────────────────┐
        ▼                    ▼                    ▼
   ┌─────────┐         ┌──────────┐        ┌─────────┐
   │   GUI   │         │  HEADLESS│        │   CLI   │
   │ MainWin │         │  Server  │        │Controller│
   └────┬────┘         └─────┬────┘        └────┬────┘
        │                    │                  │
        └────────────────────┼──────────────────┘
                             │
                  ┌──────────▼──────────┐
                  │   CORE FACADES      │
                  │  PlayerEngine       │
                  │  LibraryManager     │
                  │  DiscManager        │
                  │  PlaylistManager    │
                  │  SettingsManager    │
                  └──────────┬──────────┘
                             │
        ┌────────────────────┼────────────────────┐
        ▼                    ▼                    ▼
   ┌─────────┐         ┌──────────┐        ┌─────────┐
   │   I/O   │         │   DATA   │        │ NETWORK │
   │ (TagLib,│         │ (SQLite, │        │ (REST   │
   │  libcdio│         │  FTS5)   │        │  clients│
   │  libmpv)│         └──────────┘        └─────────┘
   └─────────┘
```

Zasada: **akceptujemy strzałki w dół, odrzucamy w górę.**

Wyjątek: sygnały Qt nie są strzałkami w architektonicznym sensie — Core
emituje sygnał, UI subskrybuje. Z punktu widzenia kompilacji UI zna Core,
ale Core nie zna UI. Subskrypcja jest registracją typu obserwatora.

---

## Player pipeline

```
   File / URL / CD device
            │
            ▼
   ┌────────────────────┐
   │  PlayerEngine      │
   │  (libmpv wrapper)  │
   └──────────┬─────────┘
              │ mpv handle
              ▼
   ┌────────────────────┐
   │  libmpv core       │
   │  ─ demuxer (FFmpeg)│
   │  ─ decoder         │
   │  ─ resampler       │
   │  ─ filter graph    │ ← EQ, ReplayGain, crossfade
   │  ─ audio output    │
   └──┬─────────────────┘
      │
      ├─ PCM tap ─────────► SpectrumWidget (FFT → bars)
      │                  └► VisualizationPlugin (Winamp adapter)
      │
      ├─ position events ──► PlayerWidget (progress bar)
      │
      ├─ track-changed ──► HistoryRecorder ─► PlayHistory (DB)
      │                  └► Scrobbler ─► scrobble_queue
      │                  └► MprisAdapter (D-Bus signal)
      │
      └─ end-of-file ────► next track / playlist advance
```

Kluczowe wywołania:

```cpp
// Załadowanie pliku z gapless setup
mpv_set_property_string(mpv, "gapless-audio", "yes");
mpv_set_property_string(mpv, "audio-channels", "stereo");
mpv_set_property_string(mpv, "af", buildFilterChain());  // EQ, RG

const char* args[] = {"loadfile", path, "replace", nullptr};
mpv_command(mpv, args);
```

Filter chain budowany dynamicznie z ustawień EQ:

```cpp
QString PlayerEngine::buildFilterChain() {
    QStringList filters;
    if (m_replayGainEnabled) {
        // libmpv przyjmuje --replaygain=track/album, ale wolimy ręcznie
        // żeby też móc renderować w Spectrum
        double gain = m_currentTrack.replayGainDb();
        filters << QString("volume=replaygain-track:replaygain-fallback=-6:replaygain-preamp=%1").arg(gain);
    }
    if (m_eqEnabled) {
        // 10-band equalizer — libmpv używa lavfi
        for (int band = 0; band < 10; ++band) {
            double freq = bandFrequencies[band];  // 60, 170, 310, 600, 1k, 3k, 6k, 12k, 14k, 16k
            double q = 1.0;
            double gain = m_eqGains[band];
            filters << QString("lavfi=[equalizer=f=%1:t=q:w=%2:g=%3]").arg(freq).arg(q).arg(gain);
        }
    }
    return filters.join(",");
}
```

PCM tap dla wizualizacji:

```cpp
// W mpv_set_wakeup_callback obsługujemy event MPV_EVENT_AUDIO_RECONFIG
// i podpinamy lavfi audiobuffer sink:
mpv_set_property_string(mpv, "af-add", "lavfi=[asplit=2]");
// dalej PCM trafia do bufora cyklicznego, który czyta SpectrumWidget co 40ms
```

**Crossfade vs gapless:**

- Gapless: domyślnie `--gapless-audio=yes`, bez fade. Działa tylko dla utworów
  z tej samej płyty. Filtr nie potrzebny.
- Crossfade: dwa instancje libmpv, jedna kończy z fade-out, druga zaczyna z
  fade-in. Synchronizacja przez `QTimer`. Implementacja w `core/Crossfader.cpp`.

---

## Disc reading pipeline

### Fizyczny CD

```
   /dev/sr0 (Linux) lub \\.\D: (Windows)
            │
            ▼
   ┌────────────────────┐
   │   CDDAReader       │
   │  (libcdio wrapper) │
   └──────────┬─────────┘
              │
              ├─ cdio_open() ────► cdio_t* drive
              │
              ├─ cdio_get_first_track_num()
              ├─ cdio_get_last_track_num()
              ├─ cdio_get_track_lsn(t)        — start sector
              ├─ cdio_get_track_last_lsn(t)   — end sector
              │
              └─ build TOC struct
                       │
                       ▼
              ┌────────────────────┐
              │  computeDiscId()   │
              │  (libdiscid SHA1)  │
              └──────────┬─────────┘
                         │ "z9TF.gNzdgxqbCKJSEzJyDPJymU-"
                         ▼
              ┌────────────────────┐
              │ MusicBrainzClient  │
              │ GET /ws/2/discid/  │
              │ {discid}?inc=...   │
              └──────────┬─────────┘
                         │ Release JSON
                         ▼
              ┌────────────────────┐
              │ Match selection    │
              │ (best score first) │
              └──────────┬─────────┘
                         │
                  ┌──────┴──────┐
                  ▼             ▼
             ┌─────────┐  ┌──────────┐
             │Catalog  │  │ Rip      │
             │ only    │  │ to FLAC  │
             └─────────┘  └────┬─────┘
                               │
                               ▼
                      ┌──────────────────┐
                      │ DiscRipper       │
                      │ ─ paranoia mode  │
                      │ ─ track per file │
                      │ ─ tag write      │
                      │ ─ RG analyze     │
                      │ ─ AccurateRip    │
                      └──────────────────┘
```

Paranoia setup:

```cpp
cdrom_paranoia_t* paranoia = paranoia_init(cdda_drive);
paranoia_modeset(paranoia, PARANOIA_MODE_FULL ^ PARANOIA_MODE_NEVERSKIP);
paranoia_seek(paranoia, start_lsn, SEEK_SET);

QFile out(target_path);
out.open(QIODevice::WriteOnly);

for (lsn_t s = start_lsn; s < end_lsn; ++s) {
    int16_t* buf = paranoia_read(paranoia, paranoiaCallback);
    out.write(reinterpret_cast<char*>(buf), CDIO_CD_FRAMESIZE_RAW);
    emit progress(s - start_lsn, end_lsn - start_lsn);
}
```

### Folder z plikami

`FolderReader` po prostu skanuje katalog rekursywnie, wczytuje tagi przez
TagLib i grupuje po polu `disc_number` z ID3v2 (`TPOS`). Domyślnie traktuje
folder jako jedną płytę, chyba że `TPOS` mówi inaczej.

### CUE sheet

`CueParser` parsuje plik `.cue`:

```
PERFORMER "Air"
TITLE "Moon Safari"
FILE "Moon Safari.flac" WAVE
  TRACK 01 AUDIO
    TITLE "La Femme d'Argent"
    INDEX 01 00:00:00
  TRACK 02 AUDIO
    TITLE "Sexy Boy"
    INDEX 01 07:11:32
```

Wynikiem jest lista `Track` z `cue_offset_ms` i `cue_duration_ms`. `PlayerEngine`
przy `play(track)` wywołuje `mpv_set_property("start", offset_seconds)` i
`mpv_set_property("end", end_seconds)`.

---

## Library import pipeline

```
    User: "import ~/Music/"
                │
                ▼
   ┌────────────────────┐
   │ LibraryManager     │
   │ ::importFolder()   │
   └──────────┬─────────┘
              │
              │ for each file matching audio extensions
              ▼
   ┌────────────────────┐
   │ Skip if filepath   │
   │ already in DB and  │
   │ mtime unchanged    │
   └──────────┬─────────┘
              │
              ▼
   ┌────────────────────┐
   │ TagInfo::fromFile()│
   │ ─ try ID3v2        │
   │ ─ fallback ID3v1   │
   │ ─ Vorbis/FLAC/...  │
   └──────────┬─────────┘
              │
              ▼
   ┌────────────────────┐
   │ Extract format     │
   │ codec, duration,   │
   │ bitrate, samplerate│
   │ (libmpv probe)     │
   └──────────┬─────────┘
              │
              ▼
   ┌────────────────────┐
   │ Compute MD5 of     │
   │ first 64 KB        │
   │ (cheap dedup hint) │
   └──────────┬─────────┘
              │
              ▼
   ┌────────────────────┐
   │ Optional:          │
   │ ChromaprintEngine  │
   │ generate AcoustID  │
   │ (slow — opt-in)    │
   └──────────┬─────────┘
              │
              ▼
   ┌────────────────────┐
   │ Resolve artist_id, │
   │ album_artist_id,   │
   │ genre_id           │
   │ (insert if new)    │
   └──────────┬─────────┘
              │
              ▼
   ┌────────────────────┐
   │ Resolve disc_id    │
   │ (DiscManager)      │
   └──────────┬─────────┘
              │
              ▼
   ┌────────────────────┐
   │ INSERT or UPDATE   │
   │ tracks row         │
   └──────────┬─────────┘
              │
              │ trigger ai/au populates tracks_fts
              ▼
   ┌────────────────────┐
   │ Optional:          │
   │ ReplayGain analyze │
   │ (slow — opt-in)    │
   └────────────────────┘
```

Cały pipeline biegnie w worker thread (`QtConcurrent::run`). UI dostaje
sygnały `progress(processed, total)` i `trackImported(Track)`.

---

## Plugin loading

```cpp
// PluginManager::loadPlugin(QString path)

QLibrary lib(path);
if (!lib.load()) return Result::Error(lib.errorString());

// 1. Sprawdź czy to natywny SoundShelf plugin
auto initFn = (NativePluginInitFn)lib.resolve("soundshelf_plugin_init");
if (initFn) {
    auto* plugin = initFn();
    if (plugin && plugin->apiVersion == NATIVE_PLUGIN_API_VERSION) {
        m_plugins.append(plugin);
        return Result::Ok();
    }
}

// 2. Sprawdź czy to Winamp vis
auto winampGetMod = (winampVisGetHeader_t)lib.resolve("winampVisGetHeader");
if (winampGetMod) {
    auto* header = winampGetMod();
    auto* adapter = new WinampVisAdapter(header, &lib);
    m_plugins.append(adapter);
    return Result::Ok();
}

return Result::Error("Unknown plugin format");
```

`WinampVisAdapter` mapuje wywołania:

```cpp
class WinampVisAdapter : public VisualizationPlugin {
public:
    QString name() const override {
        return QString::fromLocal8Bit(m_module->description);
    }

    void render(const float* pcm, int n, QPainter& p, QRect area) override {
        // Wypełnij m_module->spectrumData[0..1][0..575] z pcm
        // Wypełnij m_module->waveformData[0..1][0..575] z pcm
        fillVisData(pcm, n);

        // Winamp render rysuje do swojego HWND. Na Linuksie używamy
        // X11 child window osadzonego w QWidget. Na Windows: HWND z QWidget::winId().
        m_module->Render(m_module);
    }

private:
    winampVisModule* m_module;
};
```

Linux: Winamp DLL może działać tylko przez Wine — `WinampVisAdapter`
opcjonalnie uruchamia subprocess `wine soundshelf-vis-host.exe` i komunikuje
się przez named pipe. To zaawansowana ścieżka, domyślnie wyłączona.

---

## Network architecture

### REST clients (outbound)

Wszystkie klienty dziedziczą po wspólnej bazie:

```cpp
class RestClient : public QObject {
public:
    explicit RestClient(QObject* parent, QString baseUrl, QString userAgent);

    QFuture<QJsonDocument> get(const QString& path,
                                const QUrlQuery& query = {});
    QFuture<QJsonDocument> post(const QString& path,
                                 const QJsonDocument& body);

protected:
    QNetworkAccessManager* m_nam;
    QString m_baseUrl;
    QString m_userAgent;
    int m_rateLimitPerSec = 1;  // MusicBrainz wymaga 1 req/sec
};
```

`MusicBrainzClient`, `AcoustIDClient`, `CoverArtClient`, `LyricsClient`,
`LastFmClient`, `ListenBrainzClient` — wszystkie używają tego samego API.

### HTTP server (headless mode)

```cpp
class HttpServer : public QObject {
public:
    bool start(int port, const QString& bindAddress, const QString& authToken);

private:
    QHttpServer m_server;
    QString m_authToken;

    void registerRoutes();
    bool checkAuth(const QHttpServerRequest& req);
};

void HttpServer::registerRoutes() {
    m_server.route("/api/tracks", [this](const QHttpServerRequest& req) {
        if (!checkAuth(req)) return unauthorized();
        return QJsonDocument(LibraryManager::instance().tracksAsJson());
    });

    m_server.route("/api/track/<arg>/stream", [this](int id, const QHttpServerRequest& req) {
        if (!checkAuth(req)) return unauthorized();
        auto track = LibraryManager::instance().track(id);
        return streamFile(track.filepath, req.headers());  // Range support
    });
    // ... reszta endpoints
}
```

Range support jest kluczowy dla streamingu — klient (np. inny SoundShelf
łączący się jako "remote source") prosi o bytes 0-1MB, potem 1-2MB itd.
Bez Range cały plik byłby ładowany do pamięci.

---

## CLI ↔ GUI IPC

Gdy CLI wykrywa, że GUI już chodzi (przez `QDBusConnection::sessionBus().interface()->isServiceRegistered("org.soundshelf.Player")`),
deleguje wszystkie komendy odtwarzania przez D-Bus:

```cpp
// CLI: soundshelf play "kraftwerk"
auto iface = QDBusInterface("org.soundshelf.Player", "/Player",
                             "org.soundshelf.Player");
iface.call("playByQuery", "kraftwerk");
```

GUI exposuje ten interfejs:

```cpp
// W MainWindow setup
auto* adapter = new PlayerDBusAdapter(this, m_player);
QDBusConnection::sessionBus().registerObject("/Player", adapter,
    QDBusConnection::ExportAllSlots);
QDBusConnection::sessionBus().registerService("org.soundshelf.Player");
```

Gdy GUI nie chodzi, CLI tworzy własną instancję `PlayerEngine` i `DatabaseManager`
i robi wszystko lokalnie. Wadą jest, że nie słychać dźwięku jeśli inny terminal
zacznie grać równolegle (jedna płyta dźwiękowa). Workaround: tryb `daemon`
uruchamia tło z D-Bus listenerem, kolejne CLI delegują do daemona.

Windows: D-Bus nie jest natywny. Używamy named pipe (`\\.\pipe\soundshelf`)
z prostym JSON-RPC.

---

## Threading model

```
Main thread (UI)
  ─ event loop, signal/slot dispatch
  ─ wszystkie operacje na widgetach

Worker thread (LibraryWorker)
  ─ skanowanie folderów
  ─ tag reading/writing
  ─ ReplayGain analysis
  ─ Chromaprint generation
  ─ MD5 hash
  ─ konwersje
  ─ rip CD

Worker thread (NetworkWorker)
  ─ wszystkie REST calls
  ─ scrobble queue flush
  ─ HTTP server requests

libmpv thread (zarządzane przez libmpv)
  ─ decode + playback
  ─ wakeup callback przekazywany do Main thread przez postEvent

Plugin thread (per-plugin)
  ─ Winamp vis pluginy często chcą własnego thread'u
  ─ izolowany — crash pluginu nie zabija aplikacji (z try/catch i SEH na Win)
```

Komunikacja worker → main: zawsze `QMetaObject::invokeMethod(target, ..., Qt::QueuedConnection)`
albo `emit signal` (sygnał propaguje się przez event loop). Bezpośrednie wywołania
metod widgetów z worker thread = crash.

```cpp
// Dobrze:
QtConcurrent::run([this]() {
    auto tracks = scanFolder("/home/x/Music");
    QMetaObject::invokeMethod(this, [this, tracks]() {
        m_libraryView->refresh(tracks);
    }, Qt::QueuedConnection);
});

// Źle:
QtConcurrent::run([this]() {
    auto tracks = scanFolder("/home/x/Music");
    m_libraryView->refresh(tracks);  // ← UB, zabraniamy
});
```
