cmake_minimum_required(VERSION 3.20)

project(aao
    VERSION 2.0.0
    DESCRIPTION "Sorting and searching, from textbook to fast - every step measured"
    HOMEPAGE_URL "https://github.com/mbn-code/Algorithmic-Analysis-and-Optimization"
    LANGUAGES CXX)

set(AAO_TOP_LEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(AAO_TOP_LEVEL ON)
endif()

option(AAO_BUILD_TESTS  "Build the test suite"                     ${AAO_TOP_LEVEL})
option(AAO_BUILD_BENCH  "Build the benchmark runner"               ${AAO_TOP_LEVEL})
option(AAO_BUILD_VIEWER "Build the raylib results viewer (fetches raylib)" OFF)

# Benchmarks are meaningless without optimization: default to Release.
if(AAO_TOP_LEVEL AND NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- The library: header-only -------------------------------------------------
add_library(aao INTERFACE)
add_library(aao::aao ALIAS aao)
target_include_directories(aao INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(aao INTERFACE cxx_std_20)

install(DIRECTORY include/ DESTINATION include)

# --- Shared warning flags for our own executables ------------------------------
add_library(aao_warnings INTERFACE)
if(MSVC)
    target_compile_options(aao_warnings INTERFACE /W4 /permissive- /utf-8)
    target_compile_definitions(aao_warnings INTERFACE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(aao_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
                                                  -Wno-sign-conversion)
endif()

# --- Tests ----------------------------------------------------------------------
if(AAO_BUILD_TESTS)
    enable_testing()
    add_executable(aao_tests tests/tests.cpp)
    target_include_directories(aao_tests PRIVATE bench)
    target_link_libraries(aao_tests PRIVATE aao aao_warnings)
    add_test(NAME aao_tests COMMAND aao_tests)
endif()

# --- Benchmark ------------------------------------------------------------------
if(AAO_BUILD_BENCH)
    add_executable(aao_bench bench/main.cpp)
    target_link_libraries(aao_bench PRIVATE aao aao_warnings)
    # The textbook quicksort recurses n levels deep on sorted input; give the
    # main thread room for that where the linker allows it (Linux uses ulimit).
    if(MSVC)
        target_link_options(aao_bench PRIVATE /STACK:67108864)
    elseif(MINGW)
        target_link_options(aao_bench PRIVATE -Wl,--stack,67108864)
    elseif(APPLE)
        target_link_options(aao_bench PRIVATE -Wl,-stack_size,0x4000000)
    endif()
endif()

# --- Viewer (optional) ------------------------------------------------------------
if(AAO_BUILD_VIEWER)
    include(FetchContent)
    find_package(raylib 5.5 QUIET)
    if(NOT raylib_FOUND)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(raylib
            URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
            URL_HASH SHA256=aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_MakeAvailable(raylib)
    endif()
    add_executable(aao_viewer viewer/main.cpp)
    target_link_libraries(aao_viewer PRIVATE raylib aao_warnings)
    target_compile_features(aao_viewer PRIVATE cxx_std_20)
    if(APPLE)
        target_link_libraries(aao_viewer PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
    endif()
endif()
