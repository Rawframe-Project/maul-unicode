# Maul Unicode

Unicode algorithms and character properties for games, engines and
applications. Written in C23 with public headers any C17 or C++17
program can include, no dependencies and an MIT license.

It answers the questions every program that handles text has to ask:

- Is this text valid UTF-8, and where does it go wrong?
- Where does one user-perceived character end (for the caret, for
  backspace, for an emoji family of five code points)?
- Where may a line break, and where must it?
- In which order do mixed left-to-right and right-to-left runs appear?
- Which script is each part of the text in, for shaping and fonts?
- Are two strings the same text in different forms, or do they only
  look alike?

Every answer comes from one declared Unicode version, **18.0.0**. The
library allocates nothing: iterators live on the caller's stack and
results go into caller buffers. Every function is safe from any thread,
and results are identical on every platform.

## Status

Early development. The algorithms arrive in this order; the first five
are done:

1. The table generator and character properties.
2. UTF-8 validation and conversion.
3. Grapheme, word and sentence segmentation (UAX #29).
4. Line breaking (UAX #14), with a hook for Thai, Lao, Khmer and
   Burmese.
5. Bidirectional text (UAX #9).
6. Script itemization, and Unicode callbacks for HarfBuzz.
7. Normalization (UAX #15) and case mapping.
8. Identifiers (UAX #31) and confusable detection (UTS #39).

Each one is accepted when it passes the official Unicode conformance
files completely. Nothing is released before 0.1.0, and until 1.0.0 any
minor release may change the API.

## Building

Requirements: CMake 3.25 and GCC 14 or Clang 19 or newer; on Windows,
`clang-cl` (the Visual Studio component "C++ Clang tools for Windows").

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Use the package from CMake with `find_package(maul-unicode)` and link
`maul-unicode::maul-unicode`, or through pkg-config.

## The Maul family

Maul Unicode belongs to the Maul family of standalone libraries, with
[Maul2D](https://github.com/Rawframe-Project/maul2d) and
[Maul3D](https://github.com/Rawframe-Project/maul3d) among them. The
libraries share one rulebook,
[docs/conventions.md](docs/conventions.md), and one set of design
records, [docs/adr/](docs/adr/README.md). The
records specific to this library are listed in
[docs/adr/muni.md](docs/adr/muni.md).

## License

MIT; see [LICENSE](LICENSE). The Unicode Character Database files the
generator reads are covered by the Unicode License v3.
