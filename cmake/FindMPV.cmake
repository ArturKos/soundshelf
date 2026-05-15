# FindMPV.cmake — locate a pre-built libmpv SDK.
#
# Searches for headers (mpv/client.h) and the import library, then
# creates an IMPORTED target MPV::mpv.
#
# Hints (set one of these before calling find_package):
#   MPV_DIR  — root of the extracted mpv-dev archive
#   ENV{MPV_DIR}
#   <source>/external/mpv-dev   (auto-detected)

find_path(MPV_INCLUDE_DIR
    NAMES mpv/client.h
    HINTS
        "${MPV_DIR}/include"
        "$ENV{MPV_DIR}/include"
        "${CMAKE_SOURCE_DIR}/external/mpv-dev/include"
    PATH_SUFFIXES include
)

find_library(MPV_IMPLIB
    NAMES mpv mpv.dll.a mpv.lib libmpv.dll.a
    HINTS
        "${MPV_DIR}/lib"   "${MPV_DIR}"
        "$ENV{MPV_DIR}/lib" "$ENV{MPV_DIR}"
        "${CMAKE_SOURCE_DIR}/external/mpv-dev/lib"
        "${CMAKE_SOURCE_DIR}/external/mpv-dev"
)

find_file(MPV_DLL
    NAMES mpv-2.dll libmpv-2.dll
    HINTS
        "${MPV_DIR}/bin"   "${MPV_DIR}"
        "$ENV{MPV_DIR}/bin" "$ENV{MPV_DIR}"
        "${CMAKE_SOURCE_DIR}/external/mpv-dev"
        "${CMAKE_SOURCE_DIR}/external/mpv-dev/bin"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MPV
    REQUIRED_VARS MPV_IMPLIB MPV_INCLUDE_DIR
)

if(MPV_FOUND AND NOT TARGET MPV::mpv)
    if(MPV_DLL)
        add_library(MPV::mpv SHARED IMPORTED)
        set_target_properties(MPV::mpv PROPERTIES
            IMPORTED_LOCATION "${MPV_DLL}"
            IMPORTED_IMPLIB   "${MPV_IMPLIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${MPV_INCLUDE_DIR}"
        )
    else()
        add_library(MPV::mpv UNKNOWN IMPORTED)
        set_target_properties(MPV::mpv PROPERTIES
            IMPORTED_LOCATION "${MPV_IMPLIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${MPV_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(MPV_INCLUDE_DIR MPV_IMPLIB MPV_DLL)
