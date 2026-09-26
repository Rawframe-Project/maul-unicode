cmake_minimum_required(VERSION 3.25)

# The library's identity. The family's CMake modules and tools read these.
set(MAUL_LIBRARY maul-unicode)
set(MAUL_API_PREFIX muni)
set(MAUL_MACRO_PREFIX MUNI)
set(MAUL_TITLE "Maul Unicode")
set(MAUL_PREFIX MAUL_UNICODE)

include(cmake/ReadVersion.cmake)
maul_read_version(${CMAKE_CURRENT_SOURCE_DIR}/include/maul-unicode/base.h MUNI MAUL_VERSION)
project(maul-unicode
    VERSION ${MAUL_VERSION}
    DESCRIPTION "Unicode algorithms and character properties: validation, segmentation, line breaking, bidi"
    LANGUAGES C)

option(MAUL_UNICODE_BUILD_TESTS "Build the test suites" ${PROJECT_IS_TOP_LEVEL})
option(MAUL_UNICODE_BUILD_BENCH "Build the benchmarks" ${PROJECT_IS_TOP_LEVEL})
option(MAUL_UNICODE_BUILD_TOOLS "Build the table generator" ${PROJECT_IS_TOP_LEVEL})
option(MAUL_UNICODE_BUILD_SHARED "Build a shared library instead of a static one" OFF)
option(MAUL_UNICODE_INSTALL "Generate install rules" ${PROJECT_IS_TOP_LEVEL})
option(MAUL_UNICODE_WERROR "Treat compiler warnings as errors" OFF)
option(MAUL_UNICODE_SANITIZE "Instrument with AddressSanitizer and UBSan" OFF)
option(MAUL_UNICODE_TSAN "Instrument with ThreadSanitizer" OFF)
option(MAUL_UNICODE_COVERAGE "Instrument for gcov coverage" OFF)

# Every executable (tests, benchmarks, tools) lands in the top of the
# build tree, so scripts and CI find them in one place.
if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
endif()

include(GNUInstallDirs)
include(cmake/CompilerFlags.cmake)
include(cmake/Library.cmake)
include(cmake/Testing.cmake)

if(MAUL_UNICODE_BUILD_SHARED)
    set(MAUL_LIBRARY_KIND SHARED)
else()
    set(MAUL_LIBRARY_KIND STATIC)
endif()
add_library(maul-unicode ${MAUL_LIBRARY_KIND}
    src/base.c
    src/generated/extended_pictographic.c
    src/generated/general_category.c
    src/generated/grapheme_cluster_break.c
    src/generated/indic_conjunct_break.c
    src/generated/sentence_break.c
    src/generated/word_break.c
    src/properties.c)
maul_configure_library(maul-unicode)

if(MAUL_UNICODE_BUILD_TESTS)
    enable_testing()
    maul_add_test(base)
    maul_add_test(properties WHITEBOX)
    maul_add_header_tests()
endif()

# The generator writes src/generated/ from tools/ucd/:
#   munigen tools/ucd src/generated
if(MAUL_UNICODE_BUILD_TOOLS)
    add_executable(munigen tools/munigen.c)
    maul_apply_flags(munigen)
    # The generator uses standard fopen, which the Windows C runtime
    # reports as deprecated in favor of its own fopen_s.
    if(WIN32)
        target_compile_definitions(munigen PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

if(MAUL_UNICODE_BUILD_BENCH AND EXISTS ${PROJECT_SOURCE_DIR}/bench/bench_main.c)
    add_subdirectory(bench)
endif()

if(MAUL_UNICODE_INSTALL)
    include(cmake/Install.cmake)
endif()
