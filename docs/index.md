# Maul Unicode

Unicode algorithms and character properties in C, with no dependencies
and an MIT license: UTF-8 validation and conversion, character
properties, grapheme, word and sentence boundaries, line breaking,
bidirectional text, script itemization, normalization, case mapping,
identifiers and confusable detection, all from one Unicode version,
18.0.0. It allocates nothing, keeps no global state, and is small
enough for web builds.

- [The guide](guide.html): each part of the library and how the parts
  fit into text layout, comparison and user name checks.
- [API reference](api.html): every public function, generated from
  the headers.
- [Samples](samples.html): small complete programs, from segmenting a
  string to a user name policy.
- [The changelog](changelog.html): every release and what changed.
- [The repository](https://github.com/Rawframe-Project/maul-unicode):
  source, releases, and the test suites behind the promises.

## The promises

1. Every official Unicode conformance test for the algorithms it
   implements passes completely.
2. One Unicode version for every answer, queryable at run time.
3. Hostile input is refused with a reason and an offset; no input
   makes an algorithm allocate or work without bound.
4. The same input gives the same output on every platform.
