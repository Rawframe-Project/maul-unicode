# Samples

Each sample in [`samples/`](../samples) is a complete program that uses
only the public headers and prints something you can check. They are
built with the library (turn them off with
`-DMAUL_UNICODE_BUILD_SAMPLES=OFF`):

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/sample_segments "Some text to split"
```

## Characters, words, sentences and lines

[`samples/segments.c`](../samples/segments.c) validates its input, then
prints it split into grapheme clusters, words and sentences, and lists
where a line may or must break. An emoji family and a flag stay one
character each, and a Hindi conjunct stays whole.

## A line of mixed-direction text

[`samples/bidi.c`](../samples/bidi.c) resolves the embedding levels of
a paragraph with a Hebrew phrase and a number in it, lists its runs
from left to right, and prints the line in display order, each
right-to-left run backward by grapheme cluster with its brackets
mirrored.

## A user name policy

[`samples/names.c`](../samples/names.c) accepts or refuses candidate
user names: it puts each in NFKC, refuses mixed scripts and mixed
digit systems, derives a key by case folding that makes "ALICE" the
same account as "Alice", and compares skeletons so that a Cyrillic
"а" cannot pass for a Latin "a".

## The installed package

[`samples/minimal/`](../samples/minimal) is a separate CMake project
that finds the installed package with `find_package(maul-unicode)`,
and the HarfBuzz functions when they are installed, and nothing else.
It checks that the library it links has the Unicode version it was
compiled against. CI installs the library and builds it.
