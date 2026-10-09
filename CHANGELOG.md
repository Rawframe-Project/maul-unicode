# Changelog

All notable changes to this project are recorded here. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
the project uses [Semantic Versioning](https://semver.org/). Before
1.0.0, any minor release may change the API, the ABI and every data
format.

## [Unreleased]

## [0.3.0] - 2026-10-09

One order for the converters, from an API audit after 0.2.1; the tables
are unchanged.

### Changed

- The encoding converters (`muniConvertUtf8ToUtf16`, `muniConvertUtf16ToUtf8`, `muniConvertUtf8ToUtf32`, `muniConvertUtf32ToUtf8`) take the mode before the output, as `muniNormalize` and `muniConvertCase` do: input, length, mode, output, capacity, `neededOut`. Move the mode argument from after the capacity to after the length; a call in the old order no longer compiles.

## [0.2.1] - 2026-10-08

Two build fixes for Windows; the tables and the API are unchanged.

### Fixed

- `MUNI_NODISCARD` is `[[nodiscard]]` under MSVC's C++17 compiler too,
  which keeps `__cplusplus` at 199711L without `/Zc:__cplusplus`; CI
  compiles every public header with MSVC as C17 and C++17.
- Under Visual Studio's ClangCL toolset every target is compiled as
  C23: a target made after another fetched project was compiled below
  it, the generator mapping its C standard to `stdclatest`.

## [0.2.0] - 2026-09-27

NFKC_Casefold and the Lithuanian case rules; `muniIsDefaultIgnorable`
moves into the core.

### Added

- `muniToNfkcCasefold`: NFKC_Casefold, the caseless comparison UAX #31
  gives identifiers, from the decomposition, case folding and
  Default_Ignorable data already present (muni-0017); built with both
  normalization and case. It matches NFKC_CF of the UCD for every code
  point, and a new fuzz target and the ICU comparison check it over
  text. 143 MiB/s on the benchmark text. `samples/names.c` keys names
  with it.
- `muni_caseLithuanian`: the Lithuanian rules of SpecialCasing.txt, the
  last language rules missing (muni-0018), over a 146-byte Soft_Dotted
  list; the case component takes 9,775 bytes.

### Changed

- `muniIsDefaultIgnorable` moved from `security.h` to `properties.h`
  and into the core, over a table of its own (357 bytes); `security.h`
  includes `properties.h`, so callers compile unchanged. The Security
  table keeps only Identifier_Status (5,109 bytes, from 5,885), and the
  security component takes 35,513 bytes.
- Case folding runs 1.52 times as fast, and lowercasing and
  uppercasing slightly faster, by tracking context only where a
  language rule reads it.
- The family rules for handles, threads and asynchronous requests
  (records 0016 to 0018); thread safety paragraphs open with a statement
  from a closed list.

## [0.1.0] - 2026-09-27

The first release: every algorithm and property the requirements ask
for, from Unicode 18.0.0, passing every official conformance file.

### Added

- The library skeleton: the build, the family rules and tools, the
  version and result API (`muniGetVersion`, `muniGetUnicodeVersion`,
  `muniResultName`) and the design records.
- The Unicode 18.0.0 data files and conformance files, with checksums.
- `tools/munigen`, the table generator: for each property it searches
  the multi-level layout with the fewest bytes, verifies every code
  point through the finished table, and writes a lookup with the layout
  compiled in. CI regenerates the tables and fails on any difference.
- `muniGetGeneralCategory`, over a 12,356-byte table.
- The segmentation properties: `muniGetGraphemeBreak` (3,037 bytes),
  `muniGetWordBreak` (6,253), `muniGetSentenceBreak` (7,423),
  `muniGetIndicConjunctBreak` (2,229) and `muniIsExtendedPictographic`
  (568).
- The layout properties: `muniGetLineBreak` (11,437 bytes),
  `muniGetEastAsianWidth` (2,076), `muniGetBidiClass` (4,896),
  `muniGetCombiningClass` (2,221) and `muniGetScript` (14,201), which
  returns ISO 15924 tags built by `MUNI_SCRIPT`.
- `muniGetMirroringGlyph` (Bidi_Mirroring_Glyph, 812 bytes of table and
  38 distances) and `muniGetBracketType` (Bidi_Paired_Bracket_Type, 304
  bytes); the generator checks that every paired bracket is its mirror.
- UTF-8, UTF-16 and UTF-32: `muniValidateUtf8` and `muniValidateUtf16`
  with the kind and offset of the first error, `muniDecodeUtf8`,
  `muniEncodeUtf8`, and conversions between the three in strict mode or
  with U+FFFD for each maximal ill-formed subpart, as the Unicode
  Standard recommends. Validation agrees with an independent reference
  on every string of up to three bytes; a fuzz target checks that every
  answer agrees with every other. Outside ASCII, validation runs a
  shift-based DFA (muni-0016): 2.3 to 3.5 GB/s where the decoder alone
  gave 0.9 to 2.4.
- Grapheme cluster, word and sentence boundaries (UAX #29):
  `muniFindGraphemeBreaks`, `muniFindWordBreaks` and
  `muniFindSentenceBreaks` into a caller array, and
  `muniSegmentIterator`, a 64-byte iterator the caller keeps on the
  stack, which takes text in pieces cut anywhere, even inside a UTF-8
  sequence, and asks for the next piece with `muni_needMoreText`. Rules
  that look ahead (WB6, WB7b, WB12 and SB8) hold the boundary without
  buffering text. Passes every case of GraphemeBreakTest.txt (853),
  WordBreakTest.txt (1,944) and SentenceBreakTest.txt (512) whole,
  through the iterator and fed one byte at a time; a fuzz target checks
  that pieces give the boundaries the whole text gives.
- Line breaking (UAX #14) with the default rules: `muniInitLineIterator`
  on the same iterator, `muniNextLineBreak`, which also tells whether a
  break is mandatory, and `muniFindLineBreaks`. The five rules that look
  ahead (LB15b, LB15c, LB19a, LB25 and LB28a) hold the boundary like
  the segmentation rules. Passes all 19,346 cases of LineBreakTest.txt
  whole, through the iterator and fed one byte at a time.
- `muniSetComplexBreaker`: a caller's word segmenter for Thai, Lao,
  Khmer and Burmese (muni-0005). Line and word iterators hand it each
  run of SA text in the current piece; its breaks replace the default
  ones inside the run, never before a combining mark.
- The bidirectional algorithm (UAX #9): `muniResolveBidi` gives each
  byte of a paragraph its embedding level using one workspace byte per
  byte and no allocation (muni-0010); `muniReorderBidiLine` applies
  rules L1 and L2 and returns a line's runs in visual order;
  `muniReorderBidiLevels` and `muniInvertBidiMap` give visual and
  logical orders for any units. Passes all 91,707 cases of
  BidiCharacterTest.txt and all 770,241 of BidiTest.txt; a fuzz target
  checks paragraphs, runs and orders on arbitrary input.
- `muniGetScriptExtensions` (Script_Extensions, 1,633 bytes of table
  and 120 sets).
- Script runs for shaping: `muniScriptIterator`, `muniNextScriptRun`
  and `muniFindScriptRuns`. Common and Inherited characters join the
  run around them, Script_Extensions narrow a run to the scripts its
  characters share, and a closing bracket takes its opening bracket's
  script. The iterator takes text in pieces like the segment iterators;
  a fuzz target checks pieces against the whole text.
- Normalization (UAX #15): `muniNormalize` into NFC, NFD, NFKC or NFKD
  in a caller buffer, `muniCheckNormalization` (quick check),
  `muniDecomposePair` and `muniComposePair`. Canonical data takes 12,067
  bytes and compatibility data 22,623, behind rank indexes (muni-0011).
  Passes all 20,171 cases of NormalizationTest.txt, and leaves every
  code point the file does not list unchanged; a fuzz target checks
  idempotence, round trips between forms and quick-check answers. A
  combining sequence longer than 32 code points is refused with the new
  `muni_errorLimit`.
- Case mapping and folding: `muniToLower`, `muniToUpper`, `muniToTitle`,
  `muniFoldCase` and `muniIsCased` per code point, and
  `muniConvertCase` over text with the full mappings, the final sigma,
  titlecase by word, and the Turkish and Azerbaijani rules through
  `muni_caseTurkic` (muni-0012). 9,629 bytes of tables; a fuzz target
  checks idempotence and short output buffers.
- Build options `MAUL_UNICODE_NORMALIZATION` and `MAUL_UNICODE_CASE`
  (on by default) to leave those components and their tables out
  (muni-0013); CI builds a cell without them.
- Identifiers (UAX #31): `muniIsIdentifierStart`,
  `muniIsIdentifierContinue`, `muniIsPatternSyntax`,
  `muniIsPatternWhiteSpace` over one 6,215-byte table of bits, and
  `muniCheckIdentifier` with the new `muni_errorIdentifier`.
- Generated table headers wrap at 79 columns.
- `maul-unicode-harfbuzz` (build option `MAUL_UNICODE_HARFBUZZ`):
  `muniCreateHarfBuzzFunctions` fills HarfBuzz's Unicode functions
  (general category, combining class, mirroring, script, compose,
  decompose) from Maul Unicode. Tested against HarfBuzz's built-in data
  and through a buffer's script and direction guess; CI builds it.
  Installed as the package `maul-unicode-harfbuzz` (CMake and
  pkg-config), which finds HarfBuzz the way the build did; CI installs
  it, and the shared library, and builds `samples/minimal` against
  them.
- Security mechanisms (UTS #39), build option `MAUL_UNICODE_SECURITY`
  (on by default, needs normalization): `muniGetSkeleton`, the
  confusable skeleton in display order for a paragraph direction;
  `muniGetResolvedScripts`; `muniGetRestrictionLevel`;
  `muniCheckMixedNumbers`; `muniIsIdentifierAllowed` and
  `muniIsDefaultIgnorable`. 36,601 bytes of tables (muni-0014); a fuzz
  target checks short output buffers, skeleton idempotence and the
  agreement of levels with resolved scripts.
- `muniConvertUtf32ToUtf8`, completing the conversions between the three
  encoding forms, with the new `muni_errorUtf32Value` for surrogates and
  values past U+10FFFF (U+FFFD in replace mode).
- `muniIsEmoji`, `muniIsEmojiPresentation`, `muniIsEmojiModifier`,
  `muniIsEmojiModifierBase` and `muniIsEmojiComponent` (UTS #51), which
  share one 1,653-byte table with Extended_Pictographic;
  `muniIsWhiteSpace` (95 bytes); `muniGetDecimalDigitValue` from the
  zeros of the 77 decimal digit systems (310 bytes, muni-0015).
- `bench/bench_main.c`: validation, segmentation, line breaking, bidi,
  script run, normalization, case and security throughput, through
  iterators and the array conveniences.
  `bench/baseline.txt` records reference speeds, and the benchmark
  given that file prints each speed's ratio to them.
- CI's web job reports the static library's size in total, without any
  component, and each optional component's cost.
- Samples (build option `MAUL_UNICODE_BUILD_SAMPLES`): segments, a bidi
  line and a user name policy; `samples/minimal` consumes the installed
  packages, and CI builds it after installing.
- `fuzz_icu` (build option `MAUL_UNICODE_FUZZ_ICU`): compares UTF-8 to
  UTF-16 conversion with replacement, the four normal forms, default
  and Turkic case mapping, grapheme, word, sentence and line
  boundaries, and bidi as it displays with ICU, a test-only oracle.
  It skips what changed since ICU's Unicode version, learned from the
  properties themselves, and what ICU tailors or shortcuts; the
  comments in `test/fuzz_icu.c` list each. CI runs it with the other
  fuzz targets.
