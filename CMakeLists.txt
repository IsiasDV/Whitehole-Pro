cmake_minimum_required(VERSION 3.16)

project(WhiteholeNeo VERSION 0.1.0 LANGUAGES CXX)

option(WHITEHOLE_BUILD_TESTS "Build the native core tests" ON)

add_library(whitehole_core
    cpp/src/io/binary_file.cpp
    cpp/src/io/directory_filesystem.cpp
    cpp/src/io/rarc.cpp
    cpp/src/io/yaz0.cpp
    cpp/src/smg/bcsv.cpp
    cpp/src/smg/hash.cpp
)

target_include_directories(whitehole_core PUBLIC cpp/include)
target_compile_features(whitehole_core PUBLIC cxx_std_20)
set_target_properties(whitehole_core PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(whitehole_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(whitehole_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_executable(whitehole-neo cpp/src/main.cpp)
target_link_libraries(whitehole-neo PRIVATE whitehole_core)

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
