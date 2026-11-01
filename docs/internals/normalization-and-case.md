# Normalization and case

Normalization, case mapping and NFKC_Casefold share data and code: case folding feeds NFKC_Casefold, which reuses the normalizer, and skeletons decompose through the same data. This page explains the data layout, the streaming normalizer and the context rules.

## Decomposition data

The canonical and compatibility mappings sit behind rank indexes ([tables.md](tables.md#one-table-per-property)), in code point order. Their layout ([`tools/munigen/normalization.c`](../../tools/munigen/normalization.c)) is chosen for size, because the canonical data shares a ceiling with General_Category, combining classes, mirroring and Script:

- A canonical pair is 32 bits: the first code point and the index of the second among the few distinct seconds ("marks").
- A singleton is its low 16 bits, with a bit map of those whose target lies in plane 2.
- Composition searches the list of pairs that compose, sorted by first code point and mark; the composite is not stored but recovered by selecting the set bit at the pair's position ([`src/decompose.c`](../../src/decompose.c)).
- Compatibility mappings are sequences of UTF-16 units in a pool, each led by its length, found from the block's first by walking.
- Hangul syllables are computed, as UAX #15 section 3.12 describes.

Mappings are one level deep, as the UCD gives them, and applied recursively with an explicit stack. The quick-check answers are derived from the same tables; the generator proves the derivation against the UCD for every code point. Two derivation rules are easy to miss: since Unicode 16 a composing pair whose first code point combines backward combines backward too (the Tulu-Tigalari vowel signs), and a canonical decomposition that reaches a compatibility mapping makes NFKC_QC No.

## The streaming normalizer

[`src/normalizer.h`](../../src/normalizer.h) normalizes in one pass. The caller decomposes each code point fully and feeds the parts; they collect in a segment, a starter and the marks after it, kept in canonical order by insertion. A new starter ends the segment. In the composing forms the segment is composed first, and when it composed into a lone starter, that starter may compose with the new one too (Hangul LV + T, U+0B47 + U+0B3E). Then the segment is written out.

A segment holds at most `MUNI_MAX_NORMALIZATION_SEGMENT` (32) code points, a starter and the marks after it. A longer run of marks is refused with `muni_errorLimit` instead of being buffered without bound; UAX #15's stream-safe format allows 30 marks, and real text carries a handful.

## Case data

Each code point has a record ([`tools/munigen/case.c`](../../tools/munigen/case.c)): the indexes of its upper, lower, title and fold distances among the distinct distances, and flags (Cased, Case_Ignorable, special). About two hundred distinct records cover every code point, so the table stores an 8-bit record index. The code points whose full mappings differ from the simple ones list all four full mappings in a sorted table over a UTF-16 pool.

The conditional mappings of SpecialCasing.txt are code in [`src/case.c`](../../src/case.c), not data: Final_Sigma, the Turkic rules for I, dotted I and the dot above, and the Lithuanian rules that keep the dot of an i or a j under other accents. Two language rules come from ICU and CLDR rather than SpecialCasing.txt: Greek uppercasing (ICU's, which CLDR's `el-Upper` transform describes) and the Armenian ligature U+0587 (ICU-13416). Folding has no language rules. Context is tracked only where a selected rule reads it, which keeps the default operations fast. Soft_Dotted, which only the Lithuanian rules read, is a sorted list of ranges searched only when a dot above follows a letter in Lithuanian upper- or titlecasing; a flag bit in the case records would have split the records and cost several times more.

Titlecasing takes its word boundaries from the word iterator.

## NFKC_Casefold

[`src/nfkc_casefold.c`](../../src/nfkc_casefold.c) computes each code point's NFKC_CF value at run time instead of storing it: default ignorables are dropped, the rest case folded and decomposed (NFKD), and the round repeats until it changes nothing. The values come out decomposed, canonically equivalent to the composed values of the UCD, and the shared normalizer composes them into NFC. Every value follows from data the library carries anyway, so NFKC_Casefold costs code but no table. The case test compares every code point with NFKC_CF in `DerivedNormalizationProps.txt`.

Approximating it by case folding between two NFKC passes, as callers often do, keeps default ignorables (a soft hyphen or a zero width space inside a name) and differs for a few characters.

## Traps

- Greek uppercasing asks of each Greek letter the uppercase of its base and the accents, dialytika and iota subscript its canonical decomposition puts on it. The case component cannot use the normalization component's tables, so `tools/munigen` derives this from UnicodeData.txt and Scripts.txt into `muniGreekLetters`: an index for each code point of U+0370..U+03FF, U+1F00..U+1FFF and U+2126 into 64 distinct values, 529 bytes. Combining marks after a letter go with it. The disjunctive eta asks whether it stands alone with the Final_Sigma context, so it reads a character both cased and case-ignorable (U+0345 standing alone, U+037A) as cased, as Final_Sigma here does; ICU reads it as case-ignorable, the one place the two differ, and `fuzz_icu` skips such text for the eta as it does for the sigma.
- Titlecasing is not idempotent where a combining mark titlecases to a letter (U+0345 to capital iota), because the word boundaries move; Lithuanian uppercasing drops one dot above per pass. The case fuzz target checks idempotence only for the other operations.
- NFKC_Casefold is not invariant under canonical equivalence where U+0345 is involved, by its per-code-point definition; the casefold fuzz target leaves such text out of its canonical invariance check.
- The ICU fuzz target compares case only where no mapping reaches a character added since ICU's Unicode version, and not where Final_Sigma meets a character that is both cased and case-ignorable.
