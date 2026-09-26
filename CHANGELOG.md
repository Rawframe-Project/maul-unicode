# Changelog

All notable changes to this project are recorded here. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
the project uses [Semantic Versioning](https://semver.org/). Before
1.0.0, any minor release may change the API, the ABI and every data
format.

## [Unreleased]

### Added

- The library skeleton: the build, the family rules and tools, the
  version and result API (`muniGetVersion`, `muniGetUnicodeVersion`,
  `muniResultName`) and the design records.
- The Unicode 18.0.0 data files and conformance files, with checksums.
- `tools/munigen.c`, the table generator: for each property it searches
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
- UTF-8, UTF-16 and UTF-32: `muniValidateUtf8` and `muniValidateUtf16`
  with the kind and offset of the first error, `muniDecodeUtf8`,
  `muniEncodeUtf8`, and conversions between the three in strict mode or
  with U+FFFD for each maximal ill-formed subpart, as the Unicode
  Standard recommends. Validation agrees with an independent reference
  on every string of up to three bytes; a fuzz target checks that every
  answer agrees with every other.
- Grapheme cluster boundaries (UAX #29): `muniFindGraphemeBreaks` into a
  caller array, and `muniGraphemeIterator`, a 64-byte iterator the
  caller keeps on the stack, which takes text in pieces cut anywhere,
  even inside a UTF-8 sequence, and asks for the next piece with
  `muni_needMoreText`. Passes all 853 cases of GraphemeBreakTest.txt
  whole, through the iterator and fed one byte at a time; a fuzz target
  checks that pieces give the boundaries the whole text gives.
