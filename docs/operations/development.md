# Development

How to build, debug and change Maul Unicode as a maintainer. Building and installing as a user is in [getting-started.md](../user/getting-started.md).

## Toolchain

The organization's CI image, `ghcr.io/rawframe-project/ci-cpp`, has everything below; CI runs every Linux job in it, pinned by digest. On another machine:

- CMake 3.25 or newer and Ninja.
- GCC 14 and Clang 20 (`clang-20`, `clang-tidy-20`), with the sanitizer and libFuzzer runtimes (`libclang-rt-20-dev` on Debian and Ubuntu), and GCC's 32-bit C libraries for `linux-i686-gcc-14` (`gcc-14-multilib`).
- `clang-format` at the version CI pins: `pip install clang-format==22.1.5`. Another version formats differently.
- Python 3 for the checks and the documentation site (`pip install markdown==3.6` for the site only).
- Optional: HarfBuzz (`libharfbuzz-dev`) for the HarfBuzz functions, ICU (`libicu-dev`) for the ICU fuzz target, Emscripten for web builds.

## Build options for development

| Option | Effect |
|---|---|
| `MAUL_UNICODE_WERROR` | Warnings as errors, as CI builds. |
| `MAUL_UNICODE_SANITIZE` | AddressSanitizer and UBSan. |
| `MAUL_UNICODE_TSAN` | ThreadSanitizer. |
| `MAUL_UNICODE_COVERAGE` | gcov instrumentation. |
| `MAUL_UNICODE_FUZZ` | The libFuzzer targets, `fuzz_<area>` at the top of the build tree (Clang only). |
| `MAUL_UNICODE_FUZZ_ICU` | With the fuzz targets, `fuzz_icu`, which compares with ICU. |
| `MAUL_UNICODE_BUILD_TESTS`, `_BENCH`, `_SAMPLES`, `_TOOLS` | On when the project is built on its own. |

`MUNI_ASSERT` is on in every build without `NDEBUG`: Debug, the sanitizer builds and the fuzz builds (which set no build type). A broken invariant traps with `__builtin_trap`, so a debugger or a core dump stops at the failing check.

## Presets and running one test

Every CI cell is a CMake preset in `CMakePresets.json`: a configure, a build and a test preset of the same name, built in `out/<preset>`. The Linux presets name `gcc-14` or `clang-20`; `linux-clang-20-asan` and `linux-clang-20-tsan` are Debug builds with assertions on. Every suite is one executable and one CTest test, labeled with its name:

```sh
cmake --preset linux-clang-20-asan
cmake --build --preset linux-clang-20-asan
ctest --preset linux-clang-20-asan -L segment
./out/linux-clang-20-asan/test_segment
```

On a host without the 32-bit C libraries, `linux-i686-gcc-14` runs in the CI image of `ci.yml`'s pin, on a copy of the tree:

```sh
mkdir <dir> && git archive HEAD | tar -x -C <dir>
docker run --rm --init -v <dir>:/src -w /src ghcr.io/rawframe-project/ci-cpp:<pin> sh -c \
  'cmake --preset linux-i686-gcc-14 && cmake --build --preset linux-i686-gcc-14 && ctest --preset linux-i686-gcc-14'
```

`linux-shared` and `linux-harfbuzz` also run the `package` test (`MAUL_UNICODE_TEST_PACKAGE`, `cmake/PackageTest.cmake`), which installs the build and builds `samples/minimal` against it.

A suite prints one line per failed check with its file and line, and the conformance suites print up to ten failing lines of the Unicode test file.

## The generator

`tools/munigen` writes `src/generated/` from `tools/ucd/`. It is built with the library (`MAUL_UNICODE_BUILD_TOOLS`) and run by hand:

```sh
cmake --build out/linux-gcc-14 --target munigen
./out/linux-gcc-14/munigen tools/ucd src/generated
```

It prints each table's size and layout, and stops at the first table that does not match the UCD. Then check the sizes against their ceilings with `./out/linux-gcc-14/munigen tools/ucd src/generated | python3 tools/table_budget.py`. Never edit `src/generated/` by hand. How the tables are built is explained in [tables.md](../internals/tables.md).

## A new Unicode version

A new Unicode version is a new minor release of the library, made in one pull request:

1. Replace the files in `tools/ucd/` with the new release's, from `https://www.unicode.org/Public/<version>/ucd/` (its `auxiliary/`, `extracted/` and `emoji/` folders flattened into `tools/ucd/`), `https://www.unicode.org/Public/<version>/emoji/` (`emoji-sequences.txt`, `emoji-zwj-sequences.txt`) and `https://www.unicode.org/Public/<version>/security/` (`confusables.txt`, `IdentifierStatus.txt`). The conformance test files come from the same `ucd/` and `ucd/auxiliary/` folders. Update the version and the SHA-256 of every file in `tools/ucd/README.md` (`(cd tools/ucd && sha256sum *.txt)`, which prints the bare names the list uses); `tools/check_ucd.py` fails until the list matches the files.
2. Set `MUNI_UNICODE_VERSION_*` in `include/maul-unicode/base.h`.
3. Regenerate the tables. New property values (a new script, a new line break class) need the public enumerations and the generator's value lists extended together; the known-value tests catch a mismatch.
4. Run the full check. Conformance failures point at rule changes: read the new UAX revisions' "Modifications" sections and change the rule files.
5. Ceilings in `tools/table-budget.txt` may rise by what the new data needs; say how much and why in the pull request.
6. The ICU fuzz target compares only where both Unicode versions agree; widen what it compares when a newer ICU is available.

## Benchmarks

`out/<preset>/maul-unicode_bench` measures every component on fixed inputs and prints the best of five runs in MiB/s. Timings are never a CI gate. To measure a change, build the base and the change in Release with the same compiler and run each pinned to one core (`taskset -c 2 ./out/linux-gcc-14/maul-unicode_bench`) several times; give both numbers in the pull request. The benchmark compares with a saved output when given one as its argument.

### Against ICU4C and ICU4X

`tools/oracle/bench/` does the same work with the library, ICU4C 79.1 and ICU4X 2.4 on the same texts and counts the instructions each executes with valgrind's cachegrind, which load on the shared machine does not change. The operations are the four segmentations, NFC, NFD and NFKC, lowercase, uppercase and folding, bidi (each paragraph resolved and ordered as one line) and script runs; the texts, in `corpus/`, are Latin prose, other alphabets (Russian, Greek, Arabic, Hebrew and Hindi) and Chinese, Japanese and Korean, each repeated to 64 KiB. Every operation runs once and six times, and a fifth of the difference is one pass, so loading the text and ICU's data and making ICU's objects drop out.

```sh
tools/oracle/bench/measure.sh out/linux-gcc-14 $ICU $ICU_SRC   # a Release build; ICU4C installed and its icu4c/source
```

It prints a Markdown table of instructions per byte of UTF-8 text, the library through its iterators and through `muniFind*Breaks`, and the library's better figure over the faster reference's. The work differs in three places, which the table's counts show: ICU4C's word break iterator segments Chinese and Japanese with a dictionary, which the library does not, and ICU4X's word rules join runs of ideographs even without its models (`new_for_non_complex_scripts`), so CJK words compare with neither; ICU4C's bidi takes UTF-16 and counts the runs of the whole text where the other two order each paragraph; and script runs come from ICU's `ScriptRun` sample (`source/extra/scrptrun`), as neither ICU has them in its libraries. Two differences of setup favor ICU4X: it takes `&str`, so it validates the UTF-8 once, before the passes, where the library and ICU4C handle ill-formed UTF-8 in every pass; and it is built with link-time optimization. A pull request that changes speed gives the table's rows it moves, before and after.

## Reproducing a fuzz finding

A fuzz target writes the input that failed as `crash-<hash>` in its working directory. CI keeps that input, its seed and the target's log on the runner and prints only the report of `tools/fuzz_target.sh`: the target, the kind of each finding and the sanitizers' report. So run the target locally on the same seeds:

```sh
cmake -B out/fuzz -G Ninja -DCMAKE_C_COMPILER=clang-20 -DMAUL_UNICODE_FUZZ=ON -DMAUL_UNICODE_BUILD_TESTS=OFF
cmake --build out/fuzz
./out/fuzz/fuzz_segment -max_total_time=300 -max_len=256 test/seeds/fuzz_segment
./out/fuzz/fuzz_segment crash-<hash>
```

Every crash a fuzzer finds becomes a test in the suite of its module, with the smallest input that still fails (`-minimize_crash=1`), and its input goes into `test/seeds/<target>/` when it reaches code random bytes rarely form.
