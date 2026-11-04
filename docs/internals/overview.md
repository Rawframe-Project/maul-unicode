# Overview

Maul Unicode implements the Unicode algorithms a program that handles text needs (validation, segmentation, line breaking, bidi, script runs, normalization, case, identifiers and confusables) over property tables generated from one pinned version of the Unicode Character Database. It follows the family rules in [maul-conventions.md](maul-conventions.md); this page states what the library's domain adds and how the parts fit.

## Library profile

- **Determinism:** bit-exact. Identical input gives identical output on every platform, compiler and architecture. The library uses no floating point, and `tools/libm-allowed.txt` is empty.
- **Threads:** none. Every function runs on the caller's thread. The tables are immutable and there is no other state, so every function is safe from any thread.
- **Host code:** one callback, the word segmenter a caller sets with `muniSegmentIterator_SetComplexBreaker`. A line or word iterator calls it on the caller's thread, from inside `muniSegmentIterator_NextBreak` and `muniSegmentIterator_NextLineBreak`. It must not initialize the iterator that called it, and any other call on that iterator returns `muni_errorBusy` and changes nothing. It may call anything else in the library, since no other call reaches that iterator's state.
- **Memory:** no owner objects, and nothing is ever allocated. Iterators and workspaces are fixed-size structs or byte arrays the caller provides; results go into caller buffers, and a buffer that is too small is reported with the size needed.
- **Platform dependencies:** none beyond `<stddef.h>`, `<stdint.h>`, `<stdbool.h>` and `<string.h>`. HarfBuzz is a dependency of the separate `maul-unicode-harfbuzz` library only; ICU is a test oracle only.

## One Unicode version

Every table comes from Unicode 18.0.0, and a new Unicode version is a new library release. Shaping, segmentation and bidi that read different versions disagree about the same text, so a text stack must answer from one version; `MUNI_UNICODE_VERSION_*` and `muniGetUnicodeVersion` let a program check which one it has. The generator, the tables and the conformance files all live in the repository, so moving to a new version is one deliberate change: see [development.md](../operations/development.md#a-new-unicode-version).

## How the parts fit

```text
tools/ucd/ ──munigen──> src/generated/*.c ──> tables.h lookups
                                                   │
       properties.c, identifier.c (public property queries)
                                                   │
   encoding.h (UTF-8 step) ── cursor (text in pieces) ── writer (caller buffers)
        │                          │                          │
        │      segmenter.h engine: grapheme.c word.c sentence.c line.c (+ complex.c)
        │      script_runs.c
        │
   bidi_core.h: bidi_explicit.c bidi_implicit.c bidi_brackets.c bidi_line.c
   decompose.c + normalizer.h: normalize.c, nfkc_casefold.c
   case_map.c: case.c
   security.c, skeleton.c (skeletons reuse bidi_line and the normalizer's ordering)
```

`tools/modules.txt` declares which module may include which, and `tools/check_modules.py` enforces it. The parts and their reasoning:

- [tables.md](tables.md): the generator, the table layout, verification and the size budget.
- [encoding.md](encoding.md): UTF-8 validation and the decoding step every reader shares.
- [segmentation.md](segmentation.md): the iterators, text in pieces, the segmentation engine and its holds, complex-script runs, script runs.
- [bidi.md](bidi.md): the bidirectional algorithm in two bytes per byte of caller memory.
- [normalization-and-case.md](normalization-and-case.md): decomposition data, the streaming normalizer, case data and language rules, NFKC_Casefold.
- [security.md](security.md): confusable prototypes, skeletons in display order, script sets.

## Components

Normalization, case and security are optional, each switched by a CMake option that removes its sources and tables (`CMakeLists.txt`); security needs normalization, and NFKC_Casefold is built only when both normalization and case are. The headers stay in every build, so a call into a missing component fails to link instead of failing at run time. A game that only lays out text pays for neither normalization nor case, about 45 KB of tables. CI builds and tests a cell with all three off.

The HarfBuzz functions are a separate static library, `maul-unicode-harfbuzz` (`cmake/HarfBuzz.cmake`), so only programs that shape with HarfBuzz depend on HarfBuzz.

## Caller memory

Nothing allocates, which is why every algorithm is an iterator or a call over caller buffers:

- Segmentation, line breaking and script runs are iterators: structs declared in the public headers as opaque `uint64_t` arrays, which the caller keeps on its stack. Initializing one allocates nothing and there is nothing to close. A `static_assert` beside each internal state struct keeps it within the public size.
- Bidi cannot stream (it resolves a whole paragraph), so it takes one level byte and one workspace byte per byte of text.
- Output that may not fit is written through `src/writer.h`, which writes what fits and counts everything, so the caller learns the size the whole result needs from one pass.
- Where an algorithm could need unbounded memory, a fixed bound holds it. A normalization segment longer than 32 code points (`MUNI_MAX_NORMALIZATION_SEGMENT`, a starter and its marks) is refused with `muni_errorLimit`. Bidi keeps the 125 levels and 63 open brackets UAX #9 itself defines, and resolves text past them as the algorithm says instead of refusing it.

## Hostile input and invariants

Input is hostile: every function validates its arguments and UTF-8 and refuses what is wrong with a status, in every build. Every loop advances by at least one code unit per step, so the cost of any input is proportional to its length.

Internal invariants are asserted with `MUNI_ASSERT` (`src/invariant.h`): on in Debug, sanitizer and fuzz builds, where a broken invariant traps at the fault, and compiled out of Release builds, whose hot loops (a lookup per code point) must not pay for them.

## What 1.0.0 promises

From 1.0.0 the library keeps the family's rules for 1.x ([maul-conventions.md](maul-conventions.md#compatibility)). Here they cover:

- **The API.** The eleven headers in `include/maul-unicode/`, `harfbuzz.h` included, with every function, type, enum value, field and macro they declare. Within 1.x none is removed or renamed, and new ones may be added. Every enum value keeps its number, and a property value that a new Unicode version adds goes after the last one.
- **What the headers say.** What each call gives on success and leaves on each refusal. The one limit refused with `muni_errorLimit`, `MUNI_MAX_NORMALIZATION_SEGMENT`. UAX #9's 125 levels and 63 open brackets are no refusal: text past them resolves with `muni_success` through the algorithm's own overflow rules (X5a to X5c for levels, BD16 for brackets), and what the bidi calls give there is part of their results like any other. The library profile above: no allocation, no global state, no threads, a cost proportional to the length of any input, and the same bits everywhere.
- **The binary interface, within a minor version.** The shared library's `SOVERSION` is major.minor, so a program built against 1.y loads only a 1.y library and is built again for a later minor release, which may grow the iterator structs. The CMake packages accept any later 1.y.
- **Stored results, by their version.** `MUNI_RESULTS_VERSION` moves up with every change to what the calls `base.h` lists give, a new Unicode version included. A caller that stores normalized keys, skeletons or verdicts stores it beside them, and recomputes them when `muniGetResultsVersion` differs.

Outside the promise:

- Exact results across Unicode versions and rule fixes. The results version announces each change, and a new Unicode version is a minor release.
- `MUNI_MAX_SCRIPT_EXTENSIONS`, `MUNI_MAX_RESOLVED_SCRIPTS` and `MUNI_MAX_DECOMPOSITION` hold for one Unicode version and may rise in a minor release; the calls always report the count they need.
- The size of the iterator structs across minor releases.
- Everything outside `include/`: `src/`, the generated tables and their sizes, speed, the tools, tests, samples and benchmarks, and the HarfBuzz versions the adapter builds with.
- What a call does when its caller breaks a header's contract, such as a workspace that overlaps its text, or an iterator whose bytes were changed by hand while keeping the cookie its init call wrote. An iterator no init call set up is refused with `muni_errorInvalid`, which is promised.

The library reads and writes no asset or state format, so it promises none, and its headers name no other Maul library that would have to reach 1.0.0 first.

## Testing

The conformance files of every algorithm that has one (grapheme, word, sentence and line break tests, both bidi tests, the normalization test) pass completely, and the segmentation files run three ways: whole, as an array, and fed one byte at a time. What the public API gives for every code point is compared with the UCD files, which the tests read themselves (`test/ucd.h`): the properties, the simple and full case mappings, the identifier properties and Identifier_Status, the decomposition and composition pairs, the quick checks, NFKC_Casefold and the skeleton of each code point alone. The tables are also compared with the generator's hash, and every confusables mapping with its skeleton. Fuzz targets check that the parts agree with each other and, where the Unicode versions agree, with ICU. See [checks.md](../operations/checks.md).
