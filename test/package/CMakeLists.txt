# A consumer of the installed packages, which CI builds against an
# install prefix: find_package finds the library, and the HarfBuzz
# functions when the install has them.
#   cmake -S test/package -B build-package -DCMAKE_PREFIX_PATH=<prefix>
cmake_minimum_required(VERSION 3.25)
project(maul-unicode-package-test LANGUAGES C)

find_package(maul-unicode REQUIRED)
add_executable(package_test package_test.c)
target_link_libraries(package_test PRIVATE maul-unicode::maul-unicode)
set_target_properties(package_test PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON)

find_package(maul-unicode-harfbuzz QUIET)
if(maul-unicode-harfbuzz_FOUND)
    target_link_libraries(package_test PRIVATE maul-unicode::maul-unicode-harfbuzz)
    target_compile_definitions(package_test PRIVATE PACKAGE_HARFBUZZ)
endif()
