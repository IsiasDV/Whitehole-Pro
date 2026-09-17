cmake_minimum_required(VERSION 3.16)

project(WhiteholeNeo VERSION 0.2.0 LANGUAGES CXX)

option(WHITEHOLE_BUILD_TESTS "Build the native core tests" ON)

add_library(whitehole_core
    cpp/src/db/name_table.cpp
    cpp/src/io/binary_file.cpp
    cpp/src/io/directory_filesystem.cpp
    cpp/src/io/rarc.cpp
    cpp/src/io/yaz0.cpp
    cpp/src/smg/bcsv.cpp
    cpp/src/smg/field_hashes.cpp
    cpp/src/smg/galaxy_archive.cpp
    cpp/src/smg/game_archive.cpp
    cpp/src/smg/hash.cpp
    cpp/src/smg/stage_archive.cpp
)

target_include_directories(whitehole_core PUBLIC cpp/include)
target_compile_features(whitehole_core PUBLIC cxx_std_20)
set_target_properties(whitehole_core PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(whitehole_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(whitehole_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

set(WHITEHOLE_APP_SOURCES
    cpp/src/app/cli.cpp
    cpp/src/main.cpp
)

set(WHITEHOLE_HAVE_WIN32_GUI OFF)
if(WIN32 OR MINGW)
    set(WHITEHOLE_HAVE_WIN32_GUI ON)
elseif(DEFINED ENV{MSYSTEM} AND NOT "$ENV{MSYSTEM}" STREQUAL "MSYS")
    set(WHITEHOLE_HAVE_WIN32_GUI ON)
elseif(CMAKE_CXX_COMPILER MATCHES "ucrt64|mingw64|mingw32")
    set(WHITEHOLE_HAVE_WIN32_GUI ON)
endif()

if(WHITEHOLE_HAVE_WIN32_GUI)
    list(APPEND WHITEHOLE_APP_SOURCES cpp/src/app/gui_win32.cpp)
else()
    list(APPEND WHITEHOLE_APP_SOURCES cpp/src/app/gui_stub.cpp)
endif()

add_executable(whitehole-neo ${WHITEHOLE_APP_SOURCES})
target_link_libraries(whitehole-neo PRIVATE whitehole_core)
target_compile_definitions(whitehole-neo PRIVATE
    WHITEHOLE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
if(WHITEHOLE_HAVE_WIN32_GUI)
    target_compile_definitions(whitehole-neo PRIVATE UNICODE _UNICODE NOMINMAX)
    target_link_libraries(whitehole-neo PRIVATE comctl32 ole32 uuid shell32 comdlg32)
endif()
if(MSVC)
    target_compile_options(whitehole-neo PRIVATE /utf-8)
endif()

install(TARGETS whitehole-neo RUNTIME DESTINATION bin)
install(DIRECTORY data DESTINATION share/whitehole-neo)

if(WHITEHOLE_BUILD_TESTS)
    enable_testing()
    add_executable(whitehole_core_tests cpp/tests/core_tests.cpp)
    target_link_libraries(whitehole_core_tests PRIVATE whitehole_core)
    target_compile_definitions(whitehole_core_tests PRIVATE
        WHITEHOLE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(NAME whitehole_core_tests COMMAND whitehole_core_tests)
endif()
