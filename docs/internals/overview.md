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
- Where an algorithm could need unbounded memory, a named limit stops it with an error: a normalization segment of 32 code points (`MUNI_MAX_NORMALIZATION_SEGMENT`, a starter and its marks), the 125 levels and 63 brackets UAX #9 itself defines.

## Hostile input and invariants

Input is hostile: every function validates its arguments and UTF-8 and refuses what is wrong with a status, in every build. Every loop advances by at least one code unit per step, so the cost of any input is proportional to its length.

Internal invariants are asserted with `MUNI_ASSERT` (`src/invariant.h`): on in Debug, sanitizer and fuzz builds, where a broken invariant traps at the fault, and compiled out of Release builds, whose hot loops (a lookup per code point) must not pay for them.

## Testing

The conformance files of every algorithm that has one (grapheme, word, sentence and line break tests, both bidi tests, the normalization test) pass completely, and the segmentation files run three ways: whole, as an array, and fed one byte at a time. Property tables are compared with the generator's hash over every code point, NFKC_Casefold and the quick checks with the UCD for every code point, and every confusables mapping with its skeleton. Fuzz targets check that the parts agree with each other and, where the Unicode versions agree, with ICU. See [checks.md](../operations/checks.md).
