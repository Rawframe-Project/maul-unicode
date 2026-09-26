# muni-0011. Normalization data behind rank indexes

Status: Accepted

## Context

muni-0008 caps General_Category, Canonical_Combining_Class, mirroring,
Script and canonical decomposition together at 42,627 bytes, HarfBuzz's
total for the same data. The first four took 29,742, leaving 12,885
for decomposition and composition. A per-code-point table of 16-bit
indexes to the 2,081 canonical decompositions took 7,408 bytes alone:
every entry is distinct, so no block of the table repeats.

## Decision

Decompositions are found through rank indexes. A table gives each
64-code-point block that holds a decomposition a block number (85
canonical blocks, 114 compatibility ones); per block, a 64-bit map marks
the code points with one, and the block's rank plus the count of marked
bits below a code point is its entry's position. Entries sit in code
point order.

Canonical pairs are 32 bits: the first code point and the index of the
second among the 94 distinct seconds. Singletons are 16 bits with a bit
map of plane-2 targets. Composition searches the composing pairs,
listed by first code point and second; the composite, not stored, is
recovered by selecting the set bit at the pair's position. Hangul is
computed. Compatibility mappings are UTF-16 sequences in a pool, each
led by its length, found from the block's first by walking.

Mappings are one level deep and applied recursively with an explicit
stack. The quick-check properties are derived from these tables, and
the generator fails unless the derivation matches
DerivedNormalizationProps.txt for every code point; since Unicode 16 a
composing pair whose first code point combines backward combines
backward too (the Tulu-Tigalari vowel signs), and a canonical
decomposition that reaches a compatibility mapping makes NFKC_QC No.

Normalization reorders at most 32 code points at once and refuses more
with muni_errorLimit.

## Consequences

Canonical data is 12,067 bytes (1,511 of block table, 10,556 of data),
so the muni-0008 group totals 41,809 of 42,627. Compatibility data is
22,623 bytes, which becomes its ceiling. A lookup costs a table walk, a
bit test and a population count; composition adds two binary searches
and a bit selection. All 20,171 NormalizationTest.txt cases pass, and
every code point the file does not list stays unchanged in all forms.
