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

soundshelf_add_test(test_smart_playlist test_smart_playlist.cpp)
soundshelf_add_test(test_track_format    test_track_format.cpp)
soundshelf_add_test(test_taginfo         test_taginfo.cpp)
soundshelf_add_test(test_translator      test_translator.cpp)
