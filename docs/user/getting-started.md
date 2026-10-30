# Getting started

Maul Unicode is a C library. It builds with CMake, installs as a CMake package and a pkg-config module, and needs nothing but a C compiler and the C standard library.

## Requirements

- CMake 3.25 or newer.
- GCC 14 or Clang 19 or newer. On Windows, `clang-cl`: the Visual Studio component "C++ Clang tools for Windows". MSVC's own `cl` compiles the public headers but not the library.
- Emscripten for WebAssembly builds.

The public headers are plain C17 and C++17, so programs that include them need neither a C23 compiler nor C at all.

## Build and install

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
cmake --install build --prefix /path/to/prefix
```

Then, from CMake:

```cmake
find_package(maul-unicode REQUIRED CONFIG)
target_link_libraries(app PRIVATE maul-unicode::maul-unicode)
```

or through pkg-config, `pkg-config --cflags --libs maul-unicode`. A project can also add the source tree with `add_subdirectory`; tests, samples, benchmarks and the generator are then off by default.

[`samples/minimal/`](../../samples/minimal) is a complete project that uses only the installed package.

## Build options

| Option | Default | Effect |
|---|---|---|
| `MAUL_UNICODE_BUILD_SHARED` | `OFF` | A shared library instead of a static one. |
| `MAUL_UNICODE_NORMALIZATION` | `ON` | Normalization, with about 35 KB of tables. |
| `MAUL_UNICODE_CASE` | `ON` | Case mapping and folding, with about 10 KB of tables. |
| `MAUL_UNICODE_SECURITY` | `ON` | Confusable skeletons and restriction levels, with about 36 KB of tables. Needs normalization. |
| `MAUL_UNICODE_HARFBUZZ` | `OFF` | The separate library `maul-unicode-harfbuzz`, see [Script runs and shaping](shaping.md). |

A component left out keeps its header, so a call into it fails to link instead of failing at run time. `muniGetNfkcCasefold` needs both normalization and case.

## The model

Every answer comes from one Unicode version, 18.0.0. `muniGetUnicodeVersion` tells a program which version it linked. Stored results (skeletons, normalized and case-folded keys) depend on more than the Unicode version, since a release may also fix a rule: store `muniGetResultsVersion` beside them, and recompute them when it changes. It moves up with every change to what normalization, NFKC_Casefold, case conversion or skeletons give, or to the verdicts of `muniCheckIdentifier`, `muniGetRestrictionLevel`, `muniCheckMixedNumbers` and `muniGetResolvedScripts`, and with nothing else.

- **UTF-8 and byte offsets.** Functions take UTF-8 with an explicit length and report positions as byte offsets into it. UTF-16 and UTF-32 conversions exist for the edges of a program.
- **No allocation.** Iterators are plain structs on the caller's stack, algorithms that need scratch memory take it as an argument, and results go into caller buffers.
- **Any thread.** The tables are constant and the library has no global state, so every function is safe from any thread. An iterator, a workspace or an output buffer is used by one thread at a time.
- **Same results everywhere.** The library uses no floating point; identical input gives identical output on every platform and compiler.
- **Bounded work.** Where an algorithm could need unbounded memory, a documented limit stops it with an error instead: 125 embedding levels and 63 open brackets in bidi, a segment of `MUNI_MAX_NORMALIZATION_SEGMENT` (32) code points, a starter and the marks after it, for normalization and skeletons.

## Results and buffers

A fallible function returns a `muniResult`: `muni_success` (0), a positive outcome such as `muni_done` or `muni_needMoreText`, or a negative error. Functions over text return a `muniTextResult`: the status and, on an error, the byte offset where the offending input starts. `muniGetResultName` names any result for logs.

Functions that write text take an output buffer and its capacity, write what fits, and report through `neededOut` how many units the whole result needs. A result that does not fit returns `muni_errorCapacity`; one whose size does not fit in a `size_t`, which can happen on 32-bit targets, reports `SIZE_MAX` as its size and returns `muni_errorCapacity` too. A NULL buffer with capacity 0 measures:

```c
size_t needed = 0;
muniTextResult result =
    muniNormalize(text, length, muni_nfc, muni_convertStrict, nullptr, 0, &needed);
// result.status is muni_errorCapacity (or muni_success for empty output);
// allocate needed bytes and call again.
```

A call that fails leaves its outputs as they were, unless its header says what the failure leaves in them, such as the size an output that is too small needs, or the text converted before an error. `muni_errorInvalid` always leaves them as they were. An output must not overlap an input. The calls whose output has the type of an input refuse an overlap with `muni_errorInvalid`, so text cannot be normalized, case-mapped or folded in place: write the result to another buffer. They are `muniNormalize`, `muniConvertCase`, `muniGetNfkcCasefold`, `muniGetSkeleton` and `muniInvertBidiMap`.

Each status a function can return is listed in its header and in the [API reference](https://rawframe-project.github.io/maul-unicode/api.html).

## Samples

The samples build with the library (`-DMAUL_UNICODE_BUILD_SAMPLES=OFF` turns them off) and print something to check:

- `sample_segments "some text"` validates its input and prints it split into grapheme clusters, words, sentences and line break opportunities.
- `sample_bidi` lays out a line that mixes Hebrew, a number and brackets, run by run.
- `sample_names` accepts or refuses candidate user names with NFKC_Casefold, skeletons and restriction levels.
