# Maul Unicode

Unicode algorithms and character properties in C, with no dependencies and no allocation.

[![CI](https://github.com/Rawframe-Project/maul-unicode/actions/workflows/ci.yml/badge.svg)](https://github.com/Rawframe-Project/maul-unicode/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/github/license/Rawframe-Project/maul-unicode)](LICENSE)
[![Release](https://img.shields.io/github/v/release/Rawframe-Project/maul-unicode)](https://github.com/Rawframe-Project/maul-unicode/releases)
[![OpenSSF Scorecard](https://api.scorecard.dev/projects/github.com/Rawframe-Project/maul-unicode/badge)](https://scorecard.dev/viewer/?uri=github.com/Rawframe-Project/maul-unicode)

Every program that handles text has to answer the same questions. Is this valid UTF-8, and where does it go wrong? Where does one user-perceived character end, for the caret and for backspace? Where may a line break? In which order do mixed left-to-right and right-to-left runs appear? Which script is each part in, for shaping and fonts? Are two strings the same text in different forms, or do they only look alike?

Maul Unicode answers them for games, engines and applications:

- **Validation and conversion** of UTF-8, UTF-16 and UTF-32, treating input as hostile.
- **Character properties**: categories, segmentation and line break classes, bidi classes, scripts, emoji, digits.
- **Grapheme, word and sentence boundaries** (UAX #29) and **line breaking** (UAX #14), with a hook for Thai, Lao, Khmer and Burmese dictionaries.
- **Bidirectional text** (UAX #9) and **script runs** for shaping, with Unicode functions for HarfBuzz.
- **Normalization** (UAX #15), **case mapping** with the Turkic and Lithuanian rules, and **NFKC_Casefold**.
- **Identifiers** (UAX #31) and **confusable detection** (UTS #39).

Every answer comes from one Unicode version, 18.0.0, and every official conformance test passes completely. Iterators live on the caller's stack and results go into caller buffers, every function is safe from any thread, and results are identical on every platform. Normalization, case and confusable detection can be left out of a build with their tables.

## Quick start

```sh
git clone https://github.com/Rawframe-Project/maul-unicode.git
cd maul-unicode
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
./build/sample_segments "Hello, world. 👋🏽 Next."
```

Install it with `cmake --install build --prefix <prefix>` and use it from CMake:

```cmake
find_package(maul-unicode REQUIRED CONFIG)
target_link_libraries(app PRIVATE maul-unicode::maul-unicode)
```

or through pkg-config as `maul-unicode`. [Getting started](docs/user/getting-started.md) covers the build options and the model the API follows.

## Platforms

Linux with GCC 14 or Clang 19 and newer, macOS with Apple Clang, Windows with `clang-cl`, and WebAssembly with Emscripten; CI builds and tests each. The public headers are C17 and C++17, so any C or C++ program can include them.

## Documentation

- [The documentation site](https://rawframe-project.github.io/maul-unicode/), with the API reference generated from the headers.
- [User documentation](docs/README.md): one page per part of the library.
- [How it works](docs/internals/overview.md) and [how to develop it](docs/operations/development.md).
- [Changelog](CHANGELOG.md).

Maul Unicode belongs to the Maul family of standalone C libraries, which share one set of conventions.

## Contributing

Issues are welcome. Pull requests are accepted for issues a maintainer has approved; see [CONTRIBUTING.md](https://github.com/Rawframe-Project/.github/blob/main/CONTRIBUTING.md). The project is developed with AI coding agents, directed and reviewed by its maintainers; outside contributions follow the [AI policy](https://github.com/Rawframe-Project/.github/blob/main/AI_POLICY.md).

## License

MIT; see [LICENSE](LICENSE). The Unicode data files in `tools/ucd/` are covered by the Unicode License v3.
