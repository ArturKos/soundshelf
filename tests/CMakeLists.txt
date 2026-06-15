find_package(Qt6 REQUIRED COMPONENTS Test)

# Helper macro
function(soundshelf_add_test name source)
    qt_add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE
        soundshelf_lib
        Qt6::Test
        Qt6::Core
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

soundshelf_add_test(test_smart_playlist     test_smart_playlist.cpp)
soundshelf_add_test(test_track_format       test_track_format.cpp)
soundshelf_add_test(test_taginfo            test_taginfo.cpp)
soundshelf_add_test(test_translator         test_translator.cpp)
soundshelf_add_test(test_cue_parser         test_cue_parser.cpp)
soundshelf_add_test(test_fts5_query         test_fts5_query.cpp)
soundshelf_add_test(test_lastfm_sign        test_lastfm_sign.cpp)
soundshelf_add_test(test_duplicate_detector test_duplicate_detector.cpp)
soundshelf_add_test(test_playlist_io        test_playlist_io.cpp)
soundshelf_add_test(test_pure_helpers       test_pure_helpers.cpp)
soundshelf_add_test(test_pcm_decoder        test_pcm_decoder.cpp)
soundshelf_add_test(test_replaygain         test_replaygain.cpp)
soundshelf_add_test(test_fingerprint        test_fingerprint.cpp)
soundshelf_add_test(test_eq_presets         test_eq_presets.cpp)
soundshelf_add_test(test_spectrum           test_spectrum.cpp)
soundshelf_add_test(test_accuraterip        test_accuraterip.cpp)
soundshelf_add_test(test_bookmark_store       test_bookmark_store.cpp)
soundshelf_add_test(test_podcast_feed_parser test_podcast_feed_parser.cpp)
