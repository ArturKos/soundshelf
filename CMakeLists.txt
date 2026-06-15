cmake_minimum_required(VERSION 3.21)
project(SoundShelf
    VERSION 0.3.0
    DESCRIPTION "Cross-platform audio catalog and player"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------

option(SOUNDSHELF_BUILD_GUI         "Build the Qt GUI binary"        ON)
option(SOUNDSHELF_BUILD_CLI         "Build the standalone CLI"       ON)
option(SOUNDSHELF_BUILD_TESTS       "Build unit tests"               OFF)
option(SOUNDSHELF_ENABLE_LIBCDIO    "Enable libcdio + paranoia"      ON)
option(SOUNDSHELF_ENABLE_CHROMAPRINT "Enable AcoustID fingerprint"   ON)
option(SOUNDSHELF_ENABLE_EBUR128    "Enable EBU R128 (libebur128)"   ON)
option(SOUNDSHELF_ENABLE_FFTW3      "Enable FFTW3 for spectrum FFT"  ON)
option(SOUNDSHELF_ENABLE_HTTPSERVER "Enable QHttpServer (Qt 6.4+)"   ON)
option(SOUNDSHELF_ENABLE_MPRIS      "Enable MPRIS2 D-Bus (Linux)"    ON)

# ---------------------------------------------------------------------------
# Compiler setup
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Find dependencies
# ---------------------------------------------------------------------------

find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core Gui Widgets Network Sql LinguistTools Concurrent Multimedia Svg
)

if(SOUNDSHELF_ENABLE_HTTPSERVER)
    find_package(Qt6 6.4 OPTIONAL_COMPONENTS HttpServer)
    if(NOT Qt6HttpServer_FOUND)
        message(STATUS "QHttpServer not found, headless server disabled")
        set(SOUNDSHELF_ENABLE_HTTPSERVER OFF)
    endif()
endif()

if(SOUNDSHELF_ENABLE_MPRIS)
    if(UNIX AND NOT APPLE)
        find_package(Qt6 OPTIONAL_COMPONENTS DBus)
        if(NOT Qt6DBus_FOUND)
            message(STATUS "QtDBus not found, MPRIS2 disabled")
            set(SOUNDSHELF_ENABLE_MPRIS OFF)
        endif()
    else()
        message(STATUS "MPRIS2 is Linux-only, disabled on this platform")
        set(SOUNDSHELF_ENABLE_MPRIS OFF)
    endif()
endif()

# ---------------------------------------------------------------------------
# Dependency resolution: vcpkg (find_package CONFIG) or pkg-config fallback
# ---------------------------------------------------------------------------

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(DEFINED VCPKG_TARGET_TRIPLET)
    set(SOUNDSHELF_USE_VCPKG ON)
    message(STATUS "vcpkg detected (triplet: ${VCPKG_TARGET_TRIPLET})")
else()
    set(SOUNDSHELF_USE_VCPKG OFF)
endif()

find_package(SQLite3 REQUIRED)

if(SOUNDSHELF_USE_VCPKG)
    # -- vcpkg path: CONFIG packages (static on x64-windows-static-md) ------

    find_package(ZLIB REQUIRED)
    find_package(TagLib CONFIG REQUIRED)
    set(SS_TAGLIB_TARGET TagLib::tag)

    find_package(MPV REQUIRED)
    set(SS_MPV_TARGET MPV::mpv)

    if(SOUNDSHELF_ENABLE_CHROMAPRINT)
        find_package(unofficial-chromaprint CONFIG QUIET)
        if(unofficial-chromaprint_FOUND)
            set(SS_CHROMAPRINT_TARGET unofficial::chromaprint::chromaprint)
        else()
            find_package(Chromaprint CONFIG QUIET)
            if(Chromaprint_FOUND)
                set(SS_CHROMAPRINT_TARGET Chromaprint::chromaprint)
            else()
                message(STATUS "Chromaprint not found via vcpkg, AcoustID disabled")
                set(SOUNDSHELF_ENABLE_CHROMAPRINT OFF)
            endif()
        endif()
    endif()

    if(SOUNDSHELF_ENABLE_EBUR128)
        find_package(ebur128 CONFIG QUIET)
        if(ebur128_FOUND)
            set(SS_EBUR128_TARGET ebur128::ebur128)
        else()
            # vcpkg's libebur128 port ships headers + lib + pkg-config but no
            # CMake config package, so fall back to locating them by hand.
            find_path(EBUR128_INCLUDE_DIR ebur128.h)
            find_library(EBUR128_LIBRARY NAMES ebur128 libebur128)
            if(EBUR128_INCLUDE_DIR AND EBUR128_LIBRARY)
                add_library(ebur128_local INTERFACE)
                target_include_directories(ebur128_local INTERFACE ${EBUR128_INCLUDE_DIR})
                target_link_libraries(ebur128_local INTERFACE ${EBUR128_LIBRARY})
                set(SS_EBUR128_TARGET ebur128_local)
            else()
                message(STATUS "ebur128 not found via vcpkg, R128 RG disabled")
                set(SOUNDSHELF_ENABLE_EBUR128 OFF)
            endif()
        endif()
    endif()

    if(SOUNDSHELF_ENABLE_FFTW3)
        # The spectrum analyser uses the single-precision API (fftwf_*). vcpkg
        # ships single precision as its own config package named "FFTW3f"
        # (separate from the double-precision "FFTW3"), exporting FFTW3::fftw3f.
        find_package(FFTW3f CONFIG QUIET)
        if(FFTW3f_FOUND AND TARGET FFTW3::fftw3f)
            set(SS_FFTW3_TARGET FFTW3::fftw3f)
        else()
            message(STATUS "FFTW3 (single-precision) not found via vcpkg, spectrum disabled")
            set(SOUNDSHELF_ENABLE_FFTW3 OFF)
        endif()
    endif()

    if(SOUNDSHELF_ENABLE_LIBCDIO)
        message(STATUS "libcdio not available via vcpkg, CD-DA reading disabled")
        set(SOUNDSHELF_ENABLE_LIBCDIO OFF)
    endif()

else()
    # -- pkg-config path: system / MSYS2 packages ---------------------------

    find_package(PkgConfig REQUIRED)

    pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
    set(SS_MPV_TARGET PkgConfig::MPV)

    pkg_check_modules(TAGLIB REQUIRED IMPORTED_TARGET taglib)
    set(SS_TAGLIB_TARGET PkgConfig::TAGLIB)

    if(SOUNDSHELF_ENABLE_LIBCDIO)
        pkg_check_modules(LIBCDIO IMPORTED_TARGET libcdio libcdio_paranoia libcdio_cdda libdiscid)
        if(NOT LIBCDIO_FOUND)
            message(STATUS "libcdio not found, CD-DA reading disabled")
            set(SOUNDSHELF_ENABLE_LIBCDIO OFF)
        else()
            set(SS_LIBCDIO_TARGET PkgConfig::LIBCDIO)
        endif()
    endif()

    if(SOUNDSHELF_ENABLE_CHROMAPRINT)
        pkg_check_modules(CHROMAPRINT IMPORTED_TARGET libchromaprint)
        if(NOT CHROMAPRINT_FOUND)
            message(STATUS "Chromaprint not found, AcoustID disabled")
            set(SOUNDSHELF_ENABLE_CHROMAPRINT OFF)
        else()
            set(SS_CHROMAPRINT_TARGET PkgConfig::CHROMAPRINT)
        endif()
    endif()

    if(SOUNDSHELF_ENABLE_EBUR128)
        pkg_check_modules(EBUR128 IMPORTED_TARGET libebur128)
        if(NOT EBUR128_FOUND)
            message(STATUS "libebur128 not found, R128 RG disabled")
            set(SOUNDSHELF_ENABLE_EBUR128 OFF)
        else()
            set(SS_EBUR128_TARGET PkgConfig::EBUR128)
        endif()
    endif()

    if(SOUNDSHELF_ENABLE_FFTW3)
        pkg_check_modules(FFTW3 IMPORTED_TARGET fftw3f)
        if(NOT FFTW3_FOUND)
            message(STATUS "FFTW3 not found, spectrum will use Qt fallback")
            set(SOUNDSHELF_ENABLE_FFTW3 OFF)
        else()
            set(SS_FFTW3_TARGET PkgConfig::FFTW3)
        endif()
    endif()

endif()

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------

set(SS_CORE_SOURCES
    src/core/Track.cpp
    src/core/Translator.cpp
    src/core/SettingsManager.cpp
    src/core/SmartPlaylistEvaluator.cpp
    src/core/PlayerEngine.cpp
    src/core/LibraryManager.cpp
    src/core/DiscManager.cpp
    src/core/PlaylistManager.cpp
    src/core/PluginManager.cpp
    src/core/FolderWatcher.cpp
    src/core/HotkeyManager.cpp
    src/core/Scrobbler.cpp
    src/core/ScrobbleDrainer.cpp
    src/core/DiscEnricher.cpp
    src/core/ChromaprintEngine.cpp
    src/core/ReplayGainAnalyzer.cpp
    src/core/DuplicateDetector.cpp
    src/core/Crossfader.cpp
    src/core/MprisAdapter.cpp
)

set(SS_IO_SOURCES
    src/io/TagInfo.cpp
    src/io/FolderReader.cpp
    src/io/CDDAReader.cpp
    src/io/ImageReader.cpp
    src/io/CueParser.cpp
    src/io/PodcastFeedParser.cpp
    src/io/DiscRipper.cpp
    src/io/FormatConverter.cpp
    src/io/PcmDecoder.cpp
    src/io/PlaylistImporter.cpp
    src/io/PlaylistExporter.cpp
)

set(SS_DATA_SOURCES
    src/data/DatabaseManager.cpp
    src/data/SchemaMigrator.cpp
    src/data/PlayHistory.cpp
    src/data/FTS5Index.cpp
    src/data/BookmarkStore.cpp
    src/data/PodcastStore.cpp
)

set(SS_NETWORK_SOURCES
    src/network/RestClient.cpp
    src/network/MusicBrainzClient.cpp
    src/network/AcoustIDClient.cpp
    src/network/CoverArtClient.cpp
    src/network/LyricsClient.cpp
    src/network/LastFmClient.cpp
    src/network/ListenBrainzClient.cpp
    src/network/AccurateRip.cpp
    src/network/AccurateRipClient.cpp
    src/network/NetworkLibrary.cpp
    src/network/HttpServer.cpp
)

set(SS_PLUGIN_SOURCES
    src/plugins/VisualizationPlugin.cpp
    src/plugins/WinampVisAdapter.cpp
    src/plugins/NativeVisPlugin.cpp
)

set(SS_UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/ThemeManager.cpp
    src/ui/LibraryView.cpp
    src/ui/DiscView.cpp
    src/ui/PlayerWidget.cpp
    src/ui/SpectrumWidget.cpp
    src/ui/EqualizerWidget.cpp
    src/ui/LyricsWidget.cpp
    src/ui/BatchTagEditor.cpp
    src/ui/DuplicateDialog.cpp
    src/ui/SmartPlaylistBuilder.cpp
    src/ui/StatsWidget.cpp
    src/ui/PreferencesDialog.cpp
    src/ui/ConverterDialog.cpp
    src/ui/DiscReadDialog.cpp
    src/ui/TrayIcon.cpp
)

set(SS_CLI_SOURCES
    src/cli/CLIController.cpp
)

# Public headers — listed explicitly so AUTOMOC sees Q_OBJECT decls
# even when src/<layer>/Foo.cpp lives apart from include/soundshelf/<layer>/Foo.hpp.
file(GLOB_RECURSE SS_PUBLIC_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/soundshelf/*.hpp")

add_library(soundshelf_lib STATIC
    ${SS_CORE_SOURCES}
    ${SS_IO_SOURCES}
    ${SS_DATA_SOURCES}
    ${SS_NETWORK_SOURCES}
    ${SS_PLUGIN_SOURCES}
    ${SS_UI_SOURCES}
    ${SS_CLI_SOURCES}
    ${SS_PUBLIC_HEADERS}
)

target_include_directories(soundshelf_lib
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(soundshelf_lib PUBLIC
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Sql
    Qt6::Concurrent Qt6::Multimedia Qt6::Svg
    ${SS_MPV_TARGET} ${SS_TAGLIB_TARGET} SQLite::SQLite3
)

if(SOUNDSHELF_ENABLE_HTTPSERVER)
    target_link_libraries(soundshelf_lib PUBLIC Qt6::HttpServer)
    target_compile_definitions(soundshelf_lib PUBLIC SOUNDSHELF_HAVE_HTTPSERVER=1)
endif()
if(SOUNDSHELF_ENABLE_MPRIS)
    target_link_libraries(soundshelf_lib PUBLIC Qt6::DBus)
    target_compile_definitions(soundshelf_lib PUBLIC SOUNDSHELF_HAVE_MPRIS=1)
endif()
if(SOUNDSHELF_ENABLE_LIBCDIO)
    target_link_libraries(soundshelf_lib PUBLIC ${SS_LIBCDIO_TARGET})
    target_compile_definitions(soundshelf_lib PUBLIC SOUNDSHELF_HAVE_LIBCDIO=1)
endif()
if(SOUNDSHELF_ENABLE_CHROMAPRINT)
    target_link_libraries(soundshelf_lib PUBLIC ${SS_CHROMAPRINT_TARGET})
    target_compile_definitions(soundshelf_lib PUBLIC SOUNDSHELF_HAVE_CHROMAPRINT=1)
endif()
if(SOUNDSHELF_ENABLE_EBUR128)
    target_link_libraries(soundshelf_lib PUBLIC ${SS_EBUR128_TARGET})
    target_compile_definitions(soundshelf_lib PUBLIC SOUNDSHELF_HAVE_EBUR128=1)
endif()
if(SOUNDSHELF_ENABLE_FFTW3)
    target_link_libraries(soundshelf_lib PUBLIC ${SS_FFTW3_TARGET})
    target_compile_definitions(soundshelf_lib PUBLIC SOUNDSHELF_HAVE_FFTW3=1)
endif()

if(WIN32 AND SOUNDSHELF_USE_VCPKG)
    target_link_libraries(soundshelf_lib PUBLIC bcrypt secur32 strmiids mfplat mfuuid)
endif()

# ---------------------------------------------------------------------------
# Translations
# ---------------------------------------------------------------------------

set(SS_TS_FILES
    translations/soundshelf_en.ts
    translations/soundshelf_pl.ts
    translations/soundshelf_de.ts
    translations/soundshelf_fr.ts
)

qt_add_lupdate(soundshelf_lib TS_FILES ${SS_TS_FILES})
qt_add_lrelease(soundshelf_lib
    TS_FILES ${SS_TS_FILES}
    QM_FILES_OUTPUT_VARIABLE SS_QM_FILES
)

# ---------------------------------------------------------------------------
# Resources
# ---------------------------------------------------------------------------

qt_add_resources(soundshelf_lib "soundshelf_resources"
    PREFIX "/"
    FILES
        migrations/001_initial.sql
        migrations/002_replaygain.sql
        migrations/003_acoustid.sql
        migrations/004_smart_playlists.sql
        migrations/005_play_history.sql
        migrations/006_bookmarks.sql
        migrations/007_podcasts.sql
        resources/themes/modern_dark.qss
        resources/themes/amber_crt.qss
        resources/themes/phosphor.qss
        resources/themes/light.qss
        resources/eq_presets/electronic.json
        resources/eq_presets/classical.json
        resources/eq_presets/rock.json
        resources/eq_presets/flat.json
        resources/eq_presets/jazz.json
        resources/eq_presets/bass_boost.json
        resources/eq_presets/treble_boost.json
        resources/eq_presets/vocal.json
)

qt_add_resources(soundshelf_lib "soundshelf_translations"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_BINARY_DIR}"
    FILES ${SS_QM_FILES}
)

# ---------------------------------------------------------------------------
# Executables
# ---------------------------------------------------------------------------

if(SOUNDSHELF_BUILD_GUI)
    qt_add_executable(soundshelf src/main.cpp)
    target_link_libraries(soundshelf PRIVATE soundshelf_lib)
    set_target_properties(soundshelf PROPERTIES
        WIN32_EXECUTABLE ON
        MACOSX_BUNDLE ON
    )
endif()

if(SOUNDSHELF_BUILD_CLI)
    qt_add_executable(soundshelf-cli src/cli/cli_main.cpp)
    target_link_libraries(soundshelf-cli PRIVATE soundshelf_lib)
endif()

if(SOUNDSHELF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------

include(GNUInstallDirs)
if(SOUNDSHELF_BUILD_GUI)
    install(TARGETS soundshelf
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(SOUNDSHELF_BUILD_CLI)
    install(TARGETS soundshelf-cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(UNIX AND NOT APPLE)
    install(FILES resources/desktop/soundshelf.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endif()

message(STATUS "----- SoundShelf configuration -----")
message(STATUS "Version:           ${PROJECT_VERSION}")
message(STATUS "Build GUI:         ${SOUNDSHELF_BUILD_GUI}")
message(STATUS "Build CLI:         ${SOUNDSHELF_BUILD_CLI}")
message(STATUS "Build tests:       ${SOUNDSHELF_BUILD_TESTS}")
message(STATUS "libcdio:           ${SOUNDSHELF_ENABLE_LIBCDIO}")
message(STATUS "Chromaprint:       ${SOUNDSHELF_ENABLE_CHROMAPRINT}")
message(STATUS "libebur128:        ${SOUNDSHELF_ENABLE_EBUR128}")
message(STATUS "FFTW3:             ${SOUNDSHELF_ENABLE_FFTW3}")
message(STATUS "HttpServer:        ${SOUNDSHELF_ENABLE_HTTPSERVER}")
message(STATUS "MPRIS2:            ${SOUNDSHELF_ENABLE_MPRIS}")
message(STATUS "------------------------------------")
