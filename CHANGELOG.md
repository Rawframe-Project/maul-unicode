# Changelog

All notable changes to this project are recorded here, grouped as
Added, Changed, Fixed, Removed and Security. The project uses
[Semantic Versioning](https://semver.org/). Before 1.0.0, any release
may change the API, the ABI and every data format.

## [0.3.0](https://github.com/Rawframe-Project/maul-unicode/compare/v0.2.0...v0.3.0) (2026-10-25)


### Added

* **properties:** the Ideographic property ([#63](https://github.com/Rawframe-Project/maul-unicode/issues/63)) ([78f6ad3](https://github.com/Rawframe-Project/maul-unicode/commit/78f6ad3a909417185c51ddf3065879668916fb6f)), closes [#62](https://github.com/Rawframe-Project/maul-unicode/issues/62)

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
