# A separate project that knows nothing about the maul-unicode source
# tree and consumes only the installed packages: the library, and the
# HarfBuzz functions when the install has them. CI installs the library
# and builds this; if it builds, links and runs, the packages work.
#   cmake -S samples/minimal -B build-minimal -DCMAKE_PREFIX_PATH=<prefix>
cmake_minimum_required(VERSION 3.25)
project(maul-unicode_minimal LANGUAGES C)

find_package(maul-unicode REQUIRED CONFIG)
add_executable(minimal main.c)
target_link_libraries(minimal PRIVATE maul-unicode::maul-unicode)
set_target_properties(minimal PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON)

find_package(maul-unicode-harfbuzz QUIET CONFIG)
if(maul-unicode-harfbuzz_FOUND)
    target_link_libraries(minimal PRIVATE maul-unicode::maul-unicode-harfbuzz)
    target_compile_definitions(minimal PRIVATE MINIMAL_HARFBUZZ)
endif()
