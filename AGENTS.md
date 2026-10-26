# Maul Unicode

Maul Unicode is a C library of Unicode algorithms and character properties: UTF-8 validation and conversion, segmentation, line breaking, bidi, script runs, normalization, case, identifiers and confusables. It is part of the Maul family; the family rules are in [docs/internals/maul-conventions.md](docs/internals/maul-conventions.md), and [docs/internals/overview.md](docs/internals/overview.md) explains how the parts fit.

It never compromises on:

1. **Conformance.** Every official Unicode conformance file for an algorithm it implements passes completely, and every answer comes from one Unicode version, 18.0.0.
2. **Hostile input.** Every argument and every byte of text is validated and refused with a status and an offset, in every build. No input makes it allocate, overrun a buffer or work longer than linearly in its length.
3. **Caller memory.** Nothing allocates and there is no global state. Iterators and workspaces are caller-owned; results go into caller buffers that report the size they need.
4. **Same bits everywhere.** No floating point; identical output on every platform and compiler.
5. **Size.** Each component's tables stay under their ceiling in `tools/table-budget.txt`.

## Commands

```sh
tools/check.sh changed          # before every push: rules, format, Debug build, affected tests (< 2 min)
tools/check.sh pr               # what the pull request check runs (< 10 min)
tools/check.sh full             # adds TSan, shared, minimal and Release builds, longer fuzzing (< 20 min)
cmake --preset linux-clang-20-asan && cmake --build --preset linux-clang-20-asan   # one CI cell
ctest --preset linux-clang-20-asan -L segment   # one suite, by label
cmake --build out/linux-gcc-14 --target munigen && ./out/linux-gcc-14/munigen tools/ucd src/generated   # regenerate tables
```

Run the `pr` and `full` tiers through the workspace's heavy-job wrapper on the shared machine. Every CI cell is a preset in `CMakePresets.json` (configure, build and test of one name, built in `out/<preset>`); add a cell there, never as commands in a workflow. Each suite is `test/test_<label>.c`; the labels are the suite names (`base`, `encoding`, `properties`, `segment`, `bidi`, `script`, `identifier`, `normalize`, `case`, `security`, `harfbuzz`, `guide`, `guide_harfbuzz`).

## Ways to hurt yourself

- **Killing by pattern.** Never `pkill -f`, `pgrep | kill` or `kill` a PID found by matching a name or path: other sessions build and fuzz on the same machine with the same binary names. Kill only a PID you started.
- **Endless fuzzing.** A libFuzzer target runs until it finds something. Always pass `-max_total_time`, and stop what you started.
- **Editing generated or external files.** `src/generated/` is written by `tools/munigen`; `tools/ucd/` holds the Unicode files exactly as published, with their SHA-256 in `tools/ucd/README.md`, which `tools/check_ucd.py` checks. Change the generator, or replace the data as [development.md](docs/operations/development.md#a-new-unicode-version) describes.
- **Editing shared family files.** These are synced from the canonical copies in the organization's `.github` repository and fail CI when they differ: `docs/internals/maul-conventions.md`; `.clang-format`, `.clang-tidy`, `.editorconfig`, `.gitattributes`, `.gitignore`; `cmake/CompilerFlags.cmake`, `cmake/Install.cmake`, `cmake/Library.cmake`, `cmake/ReadVersion.cmake`, `cmake/Testing.cmake`, `cmake/Config.cmake.in`, `cmake/pkg-config.pc.in`, `cmake/HeaderCheck/CMakeLists.txt`; `bench/CMakeLists.txt`; `test/test_harness.h`; and `tools/build_docs.py`, `check_docs.py`, `check_exports.py`, `check_guide.py`, `check_lengths.py`, `check_modules.py`, `check_source.py`, `check_test_hygiene.py`, `gen_api.py`, `fuzz_target.sh`, and the fixtures in `tools/test_hygiene/`. Change the canonical copy first.

## Invariants

- **Generated sources:** `src/generated/*.c`, from `tools/ucd/` by `tools/munigen`. CI regenerates them and fails on any byte of difference. `src/tables.h` declares their lookups and hashes.
- **Unicode version:** `MUNI_UNICODE_VERSION_*` in `include/maul-unicode/base.h` must name the version of `tools/ucd/`. A new version is a minor release.
- **Library version:** `MUNI_VERSION_*` between the release-please markers in `base.h`; CMake reads it from there. Only release-please changes it.
- **Public headers** are the common subset of C17 and C++17 and declare only the API; every public function has a `///` block with `@param`, `@return` and a thread safety paragraph from the list in `tools/check_docs.py`. Iterator structs are opaque `uint64_t` arrays whose size is ABI; a `static_assert` beside each internal state keeps it within them.
- **Modules:** `tools/modules.txt` lists which module may include which. A new include needs a line there.
- **Components:** normalization, case and security are optional (`CMakeLists.txt` source lists); security needs normalization, NFKC_Casefold needs both. Code in the core must not call into a component.
- **Assertions:** `MUNI_ASSERT` (`src/invariant.h`) checks internal invariants only. It traps in builds without `NDEBUG` (Debug, sanitizers, fuzzing) and compiles out of Release, whose per-code-point loops must not pay for it. Never assert on caller input or text; validate and return a status. Library code never calls `assert` or `abort`.
- **Workflows:** every action is pinned by commit SHA with its version in a comment, and each workflow and job declares the least `permissions` it needs; Dependabot moves the pins.
- **Limits:** 32 combining marks per normalization segment (`muni_errorLimit`), and the 125 levels and 63 brackets of UAX #9. A limit is part of the API.

## Testing

Follow the [repository standard's Tests section](https://github.com/Rawframe-Project/.github/blob/main/docs/repository-standard.md#tests); in this repository:

- Conformance comes from the Unicode files, not hand-written cases. A rule change is proven by the conformance suite, and the segmentation files also run fed one byte at a time.
- Test through the public headers. The white-box suites include `src/tables.h` only for the generator's hashes; do not test internal state.
- Inputs that could be read past their end go in a heap allocation of exactly their length, so AddressSanitizer sees an overread; the bidi, case and security suites show how.
- Every C snippet of `docs/user/` appears unchanged in `test/test_guide.c` or `test/test_guide_harfbuzz.c`; `tools/check_guide.py` enforces it.
- A fuzz finding becomes a test in its module's suite with the minimized input.
- Data suites read `tools/ucd/` through `MUNI_UCD_DATA`. Tests that call `fopen` need `_CRT_SECURE_NO_WARNINGS` on Windows, which `CMakeLists.txt` sets for the data suites.

## Documentation

Follow the [repository standard's Documentation section](https://github.com/Rawframe-Project/.github/blob/main/docs/repository-standard.md#documentation). A change in what a caller must do or can rely on updates the page of `docs/user/` for that part and, if it is in a snippet, the matching code in `test/test_guide*.c`. The API reference is generated from the headers into the documentation site; never commit it.

## Pull requests

Titles are conventional commits. Scopes: `encoding`, `properties`, `segment`, `line`, `bidi`, `script`, `normalize`, `case`, `identifier`, `security`, `harfbuzz`, `generator`, `tables`, `build`, `bench`, `family`. Example: `fix(line): keep a quotation mark with the word after it`.

A change to `src/generated/` names the generator change or data that caused it; a change of results (a table, a rule) also names who sees it, since callers may store skeletons and keys.
