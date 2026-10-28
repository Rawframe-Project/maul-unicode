# Changelog

All notable changes to this project are recorded here, grouped as
Added, Changed, Fixed, Removed and Security. The project uses
[Semantic Versioning](https://semver.org/). Before 1.0.0, any release
may change the API, the ABI and every data format.

## [0.6.0](https://github.com/Rawframe-Project/maul-unicode/compare/v0.5.0...v0.6.0) (2026-10-28)

### Changed

- **Breaking:** the word segmenter callback type `muniComplexBreakFn`
  is renamed `muniComplexBreakFunction`, the name the Maul family gives
  its callback types. Its parameters and contract and
  `muniSetComplexBreaker` are unchanged: code that names the type
  changes the name, and code that only passes a function changes
  nothing
  ([#130](https://github.com/Rawframe-Project/maul-unicode/pull/130)).

### Fixed

- **Breaking:** `muniEncodeUtf8` takes the output's capacity after
  `bytes`: `muniEncodeUtf8(codePoint, bytes, capacity, sizeOut)`.
  Before, it wrote up to four bytes without knowing the array's size,
  so a caller encoding into the rest of its own buffer that checked the
  room wrongly had bytes written past its end. Now a capacity below the
  encoding's length returns `muni_errorCapacity`, writes no byte and
  puts the length needed, 1 to 4, in `sizeOut`; a NULL `bytes` with
  capacity 0 measures. A surrogate, a value past U+10FFFF, a NULL
  `sizeOut`, or a NULL `bytes` with a nonzero capacity returns
  `muni_errorInvalid` and leaves both outputs as they were
  ([#129](https://github.com/Rawframe-Project/maul-unicode/pull/129)).

## [0.5.0](https://github.com/Rawframe-Project/maul-unicode/compare/v0.4.0...v0.5.0) (2026-10-28)

### Added

- `MUNI_MAX_NORMALIZATION_SEGMENT` (32) in `normalize.h` names the
  bound behind every `muni_errorLimit`: a normalization segment, a
  starter and the marks after it, has at most 32 code points, so 31
  marks may follow a starter. `muniNormalize`, `muniToNfkcCasefold` and
  `muniGetSkeleton` refuse a longer one, as before
  ([#108](https://github.com/Rawframe-Project/maul-unicode/pull/108)).
- `MUNI_MAX_SCRIPT_EXTENSIONS` (32) in `properties.h` and
  `MUNI_MAX_RESOLVED_SCRIPTS` (64) in `security.h`: an array of that
  size always holds what `muniGetScriptExtensions` and
  `muniGetResolvedScripts` write for this release's Unicode version. A
  later Unicode version may raise them; the calls still report the
  count they need and return `muni_errorCapacity` instead of writing
  past a smaller array
  ([#119](https://github.com/Rawframe-Project/maul-unicode/pull/119)).
- `MUNI_RESULTS_VERSION` and `muniGetResultsVersion`. The version moves
  up with every change to what `muniNormalize`, `muniToNfkcCasefold`,
  `muniConvertCase` or `muniGetSkeleton` give, or to the verdict of
  `muniCheckIdentifier`, `muniGetRestrictionLevel`,
  `muniCheckMixedNumbers` or `muniGetResolvedScripts`, a new Unicode
  version included. Store it beside normalized keys and skeletons, and
  recompute them when it changes: a fixed rule within one Unicode
  version changes results without changing `muniGetUnicodeVersion`.
  It starts at 1, and no result changed in this release
  ([#121](https://github.com/Rawframe-Project/maul-unicode/pull/121)).

### Changed

- **Breaking:** the result codes follow the Maul family's table.
  `muni_errorInvalid` (-1) and `muni_errorCapacity` (-2) keep their
  values, `muni_errorLimit` moves from -10 to -9, and the library's own
  errors move to -32 to -40: the six UTF-8 errors at -32 to -37 in
  their old order, then `muni_errorUtf16Surrogate` -38,
  `muni_errorUtf32Value` -39 and `muni_errorIdentifier` -40. Code that
  uses the names needs only a rebuild; code or data that stores the
  numbers must change them
  ([#108](https://github.com/Rawframe-Project/maul-unicode/pull/108)).
- **Breaking:** a call made at the wrong time returns the new
  `muni_errorState` (-6) instead of `muni_errorInvalid`:
  `muniSetComplexBreaker`, `muniSetBreakClassOverrides` and
  `muniSetLineOptions` on an iterator already asked for a break or
  boundary, and `muniFeedSegmentIterator` and `muniFeedScriptIterator`
  on an iterator that did not ask for more text. A wrong argument stays
  `muni_errorInvalid` in any state, and is reported first
  ([#111](https://github.com/Rawframe-Project/maul-unicode/pull/111)).
- **Breaking:** `muniDecomposePair` returns a `muniResult` and takes a
  `countOut` after `secondOut`, which receives 0 when the code point
  does not decompose, 1 for a singleton and 2 for a pair. A NULL output
  returns `muni_errorInvalid` and writes nothing. Before, it returned
  `false` both for a code point without a decomposition and for a NULL
  output, and a singleton was told from a pair only by a second code
  point of 0
  ([#117](https://github.com/Rawframe-Project/maul-unicode/pull/117)).

### Fixed

- **Breaking:** a word segmenter set with `muniSetComplexBreaker` that
  calls back into its own iterator gets the new `muni_errorBusy` (-10)
  from the three setters, `muniFeedSegmentIterator`,
  `muniNextSegmentBreak` and `muniNextLineBreak`, and the call changes
  nothing. Before, such a call worked on the iterator's state from
  before the outer `muniNextSegmentBreak` or `muniNextLineBreak`, which
  then overwrote it, so a change was lost without notice or a break
  came from stale state. The segmenter must not initialize its own
  iterator
  ([#115](https://github.com/Rawframe-Project/maul-unicode/pull/115)).

## [0.4.0](https://github.com/Rawframe-Project/maul-unicode/compare/v0.3.1...v0.4.0) (2026-10-27)

### Changed

- **Breaking:** `muniGetSkeleton` takes the workspace's size as a new
  argument after `workspace`. A workspace smaller than 2 * length
  returns `muni_errorCapacity` with offset 0 and `neededOut` 0, before
  the text is read, for all text: before, a short workspace was written
  past its end only when the text needed bidi, so the mistake passed
  tests with Latin text. A too-small output always has `neededOut`
  above its capacity, so the two errors can be told apart. `workspace`
  may be NULL only when `workspaceSize` is 0; for text that is not
  empty, that returns `muni_errorCapacity` instead of
  `muni_errorInvalid`
  ([#92](https://github.com/Rawframe-Project/maul-unicode/pull/92)).
- `muniReorderBidiLevels` and `muniReorderBidiLine` return
  `muni_errorInvalid` for a level above 126, the most rules I1 and I2
  leave, and `muniReorderBidiLine` for a paragraph level above 1, before
  writing anything. Before, a level such as 200 was ordered and
  returned `muni_success`
  ([#100](https://github.com/Rawframe-Project/maul-unicode/pull/100)).

### Fixed

- A result too long to count in a `size_t`, which a 32-bit target can
  reach through UTF-16 to UTF-8 conversion, case mapping,
  normalization, NFKC_Casefold or skeletons, returns
  `muni_errorCapacity` with `neededOut` set to `SIZE_MAX`. Before, the
  count wrapped and the call reported a size too small or returned
  `muni_success` with only part of the result written
  ([#87](https://github.com/Rawframe-Project/maul-unicode/pull/87)).
- `muniInvertBidiMap` returns `muni_errorInvalid` for a map that
  repeats an entry, such as `{2, 0, 2}`. Before, it returned
  `muni_success` with one slot of `inverseOut` never written
  ([#89](https://github.com/Rawframe-Project/maul-unicode/pull/89)).
- `muniReorderBidiLine` writes no runs when they do not fit, as
  documented. Before, it returned `muni_errorCapacity` with the runs
  that fit left in the output in logical order
  ([#99](https://github.com/Rawframe-Project/maul-unicode/pull/99)).
- `muniGetSkeleton` sets `neededOut` to 0 on every error except an
  output too small, which reports the size the skeleton needs, and
  `muni_errorInvalid`, which leaves it as it was. Before,
  `muni_errorLimit` left the bytes counted before the limit, which
  tells a caller nothing since the skeleton follows display order
  ([#101](https://github.com/Rawframe-Project/maul-unicode/pull/101)).
- `muniCheckNormalization` validates the whole text and reports a UTF-8
  error after the point where the answer became No. Before, it stopped
  there and returned `muni_success`
  ([#106](https://github.com/Rawframe-Project/maul-unicode/pull/106)).

## [0.3.1](https://github.com/Rawframe-Project/maul-unicode/compare/v0.3.0...v0.3.1) (2026-10-27)

### Fixed

- Bidi (`muniResolveBidi`, and `muniGetSkeleton` for text that needs
  bidi) resolves an FSI's direction only when the isolate takes a
  level, as UAX #9 rules X5a to X5c have it: an isolate past the depth
  limit of 125 takes no level, so its direction is never used. Every
  result is unchanged
  ([#79](https://github.com/Rawframe-Project/maul-unicode/pull/79)).

## [0.3.0](https://github.com/Rawframe-Project/maul-unicode/compare/v0.2.0...v0.3.0) (2026-10-25)

### Added

- `muniIsIdeographic` tells whether a code point has Unicode's
  Ideographic property (PropList.txt), which a word rule over ideographs
  such as CLDR's Japanese one needs. It is not Script=Han: Han includes
  the CJK radicals, which are not Ideographic, and leaves out Tangut,
  Khitan Small Script, Jurchen, Nushu and Small Seal, which are. A value
  past U+10FFFF is false. No existing result changes
  ([#63](https://github.com/Rawframe-Project/maul-unicode/pull/63)).

### Changed

- On 32-bit x86 the library is built with SSE2 (`-msse2 -mfpmath=sse`,
  from the family's compiler flags), so it needs a processor with SSE2;
  results are the same
  ([#57](https://github.com/Rawframe-Project/maul-unicode/pull/57)).

## [0.2.0](https://github.com/Rawframe-Project/maul-unicode/compare/v0.1.1...v0.2.0) (2026-10-25)

### Changed

- **Breaking:** `muniScriptIterator` is 208 bytes on every target, from
  224: its storage now fits the state it holds on 64-bit targets,
  where the state is largest, instead of keeping 16 spare bytes there.
  Programs must be rebuilt against the new headers; no
  source change is needed
  ([#54](https://github.com/Rawframe-Project/maul-unicode/pull/54)).

## [0.1.1](https://github.com/Rawframe-Project/maul-unicode/compare/v0.1.0...v0.1.1) (2026-10-25)

### Fixed

- `muniSetBreakClassOverrides` refuses a count over 0x10F800, one entry
  per scalar value, before it reads any entry, and `segment.h`
  documents that limit. The old check against `UINT32_MAX` never fired
  where `size_t` is 32 bits. GCC 14 warned about it there
  (`-Wtype-limits`), which stopped builds that treat warnings as errors
  (`MAUL_UNICODE_WERROR`). A larger count is still `muni_errorInvalid`,
  and valid tables behave the same
  ([#50](https://github.com/Rawframe-Project/maul-unicode/pull/50)).

## [0.1.0](https://github.com/Rawframe-Project/maul-unicode/compare/v0.0.1...v0.1.0) (2026-10-25)

Word, sentence and line segmentation can follow a language's
conventions where locale data such as CLDR gives a character another
class.

### Added

- Word and sentence iterators take per-code-point class overrides
  (`muniSetBreakClassOverrides`), for conventions such as Greek, which
  ends a sentence at U+003B and U+037E, and POSIX English, which keeps
  the full stop out of words but not out of numbers. The table is the
  caller's, sorted by code point and read in place.
- Line iterators take options (`muniSetLineOptions`).
  `muni_lineCjAsIdeographic` allows a line break before the small
  kana, the prolonged sound mark and the other Line_Break=CJ
  characters, as Chinese and Japanese line breaking does.

### Changed

- The shared library's SONAME carries the whole version before 1.0:
  `libmaul-unicode.so.0.1.0` instead of `.so.0.0`, so a program is
  never loaded with another 0.x release than the one it was built
  against.
- The README's quick start installs Git, CMake and GCC 14 first on
  Ubuntu 24.04, whose default GCC 13 the build refuses.

## [0.0.1](https://github.com/Rawframe-Project/maul-unicode/releases/tag/v0.0.1) (2026-10-23)

The first release: Unicode algorithms and character properties from
Unicode 18.0.0, passing every official conformance file for the
algorithms it implements.

### Added

- UTF-8, UTF-16 and UTF-32 validation that names each kind of error
  and its offset, decoding and encoding of single code points, and
  conversions between the three, strict or with U+FFFD for each
  maximal ill-formed subpart.
- Character properties for any code point: General_Category, the
  segmentation and line breaking classes, East_Asian_Width, Bidi_Class,
  mirroring and bracket pairs, combining classes, Script and
  Script_Extensions, the emoji properties, White_Space,
  Default_Ignorable_Code_Point and decimal digit values.
- Grapheme cluster, word and sentence boundaries (UAX #29) and line
  break opportunities (UAX #14), through iterators on the caller's
  stack that take text in pieces, or into arrays. A caller's word
  segmenter can break Thai, Lao, Khmer and Burmese.
- The bidirectional algorithm (UAX #9) in two bytes of caller memory
  per byte of text, with line ordering and visual maps.
- Script runs for shaping, and `maul-unicode-harfbuzz`, which gives
  HarfBuzz its Unicode data from the library.
- Normalization to NFC, NFD, NFKC and NFKD with quick checks; case
  mapping and folding with the full mappings, Final_Sigma and the
  Turkic and Lithuanian rules; NFKC_Casefold.
- Identifier properties and checks (UAX #31), confusable skeletons,
  resolved script sets, restriction levels and mixed-number detection
  (UTS #39).
- Build options to leave out normalization, case and security with
  their tables, a shared library option, a CMake package and
  pkg-config modules.
- Support for Linux, macOS, Windows with `clang-cl`, and WebAssembly.
