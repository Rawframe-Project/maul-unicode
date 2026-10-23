# Documentation

## Using the library

- [Getting started](user/getting-started.md): requirements, building, installing, build options, the model the API follows, results and buffers.
- [Validating and converting text](user/text-encoding.md): UTF-8, UTF-16 and UTF-32.
- [Character properties](user/properties.md): per code point lookups.
- [Characters, words, sentences and lines](user/segmentation.md): grapheme clusters, word and sentence boundaries, line breaking, complex scripts, text in pieces.
- [Bidirectional text](user/bidi.md): embedding levels and line order.
- [Script runs and shaping](user/shaping.md): runs for a shaper and the HarfBuzz functions.
- [Normalization and case](user/normalization-and-case.md): normal forms, case operations, comparing strings.
- [Identifiers and security](user/security.md): identifier rules, confusable skeletons, restriction levels.
- [API reference](https://rawframe-project.github.io/maul-unicode/api.html), generated from the headers.

## How it works

- [Overview](internals/overview.md): the library profile and how the parts fit.
- [Glossary](internals/glossary.md).
- [Property tables](internals/tables.md), [UTF-8 handling](internals/encoding.md), [Segmentation](internals/segmentation.md), [Bidi](internals/bidi.md), [Normalization and case](internals/normalization-and-case.md), [Security](internals/security.md).
- [Maul conventions](internals/maul-conventions.md): the rules every Maul library follows.

## Maintaining it

- [Development](operations/development.md): toolchain, running tests, the generator, a new Unicode version, benchmarks, fuzz findings.
- [Checks](operations/checks.md): the local tiers, the CI jobs and their budgets.
- [Release](operations/release.md).
