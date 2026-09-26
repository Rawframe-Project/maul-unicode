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
