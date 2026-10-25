# Changelog

All notable changes to this project are recorded here, grouped as
Added, Changed, Fixed, Removed and Security. The project uses
[Semantic Versioning](https://semver.org/). Before 1.0.0, any release
may change the API, the ABI and every data format.

## [0.1.0](https://github.com/Rawframe-Project/maul-unicode/compare/v0.0.1...v0.1.0) (2026-10-25)


### Added

* **line:** resolve CJ as ID on request ([#38](https://github.com/Rawframe-Project/maul-unicode/issues/38)) ([56d3e85](https://github.com/Rawframe-Project/maul-unicode/commit/56d3e85651eaac3721e0deae0eb52132726e5ec2)), closes [#16](https://github.com/Rawframe-Project/maul-unicode/issues/16)
* **segment:** override break classes on word and sentence iterators ([#36](https://github.com/Rawframe-Project/maul-unicode/issues/36)) ([be0d183](https://github.com/Rawframe-Project/maul-unicode/commit/be0d183a954900bc013d1a54214e5939975bbebf)), closes [#15](https://github.com/Rawframe-Project/maul-unicode/issues/15)

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
