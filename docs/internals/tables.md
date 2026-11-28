# Property tables

Every property the library knows lives in a table that `tools/munigen` generates from the Unicode Character Database in `tools/ucd/` and writes into `src/generated/`. The generated sources are committed: building the library needs only a C compiler, never the generator or a scripting runtime, and a table change shows in review.

## One table per property

Each property has its own multi-level lookup table, and boolean properties use the same technique. For each table the generator builds the full array of values, one per code point, and searches the split into levels that stores it in the fewest bytes; identical blocks are stored once. The search result is compiled into the lookup function the generated file defines, so a lookup is a fixed sequence of shifts and loads with no branches on the layout. The generator prints each table's size and layout when it runs. Word_Break, Sentence_Break and Line_Break, which the segmentation rules read for every code point, also store their 128 ASCII values in an array the lookup reads before the trie: 128 bytes each, counted in their ceilings, for a lookup of ASCII without the trie.

One table per property maps queries one to one onto tables, lets a build that leaves out an algorithm keep every property query, and turns a property a later Unicode version adds into a new table rather than a new packing. A single combined record per code point would cost several times as much and could not leave out what a build does not use; a trie with one fixed split for every property costs more per table than a searched split.

Where one loop reads several properties of every code point, they share a table, a trade of size for speed stated in the pull request that made it. QuickCheck holds the normalization quick checks. GraphemeProperties holds Grapheme_Cluster_Break, Indic_Conjunct_Break and Extended_Pictographic in one byte, so the grapheme rules look a code point up once instead of three times, for 83 bytes more than the two tables it replaced; Extended_Pictographic stays in the emoji table too, for the queries and rules outside grapheme breaking.

Some data does not suit a per-code-point table:

- **Rank indexes** store sparse mappings (decompositions, compatibility mappings, confusable prototypes). A small table gives each 64-code-point block that holds any entry a block number; per block, a 64-bit map marks the code points that have one, and the block's rank plus the count of marked bits below a code point is the position of its entry. Entries sit in code point order. A lookup costs a table walk, a bit test and a population count (`muniRank` in `src/bits.h`). Per-code-point indexes into such data would never repeat, so no block of their table could be shared.
- **Sorted lists** hold what is short: the zeros of the 77 decimal digit systems (a digit's value is its distance from the greatest zero at or below it), the Soft_Dotted ranges, the special case mappings.
- **Rules checked by the generator** stand in for most of the RGI emoji sequences ([`tools/munigen/emoji.c`](../../tools/munigen/emoji.c)): the basic emoji, keycaps and skin tone sequences follow from the emoji properties, and the generator fails if the files stop agreeing. Flags are a bitmap over pairs of regional indicators, the three tag sequences their tag letters, and the 1,614 ZWJ sequences 261 skeletons of bases, each saying which elements take a skin tone, whether the sequence also comes without, and whether the tones must differ, grouped by first base. The whole set takes about 2 KB.
- **Indexes into side arrays** keep tables narrow: the Script table stores an index into the script tags, Script_Extensions an index into a list of script sets, the mirroring table an index into distinct mirror distances.

## Verification

The generator stops at the first problem. It checks every code point through each finished table against the array it built, so a table is verified before it is written. It derives the four normalization quick-check properties from the decomposition tables and fails unless they match `DerivedNormalizationProps.txt` for every code point; the QuickCheck table it then writes holds the derived values. It checks Soft_Dotted against `PropList.txt`, every decimal digit's derived value, and that Identifier_Status follows from Identifier_Type.

Each generated file also carries the hash of its array (FNV-1a 64 over one byte per code point), and the property, case, identifier and security tests hash the library's own lookups over every code point against it. That checks the compiled lookup functions on every platform CI builds, where the generator's check covered only its own reading of the tables.

CI rebuilds the generator, regenerates the tables and fails on any byte of difference, so a generator change that alters a table cannot go unnoticed.

## Size budget

The library must stay small enough for web builds. Each component's tables have a ceiling, listed in `tools/table-budget.txt` with the generated tables it covers, and `tools/table_budget.py` checks the generator's report against it in CI: a component past its ceiling, or a table no component covers, fails.

The ceilings come from the smallest measured implementation that stores the same data. General_Category, Canonical_Combining_Class, mirroring, Script and canonical decomposition together stay within HarfBuzz's total for the same data; line, word, sentence and grapheme breaking data stay within ICU4X's rule data for each. A component without such a reference takes its first measured size as its ceiling. East_Asian_Width counts toward line breaking (rule LB30 reads it) and the grapheme properties, with Indic_Conjunct_Break (rule GB9c) and Extended_Pictographic (GB11), toward grapheme breaking.

A ceiling only moves with a reason: a new Unicode version that adds data, or a deliberate trade of size for speed, stated in the pull request. CI's web build also reports the built library's size in total and the cost of each optional component.

## Traps

- Never edit `src/generated/` by hand; change the generator and regenerate.
- The hash in a generated file is over the array the generator built, not over the table bytes: a layout change that keeps every value keeps the hash.
- A table whose values the library combines with a public enumeration (General_Category, Line_Break, Bidi_Class and the others) depends on the generator writing values in the header's order. Both sides of a hash come from the generator, so a hash cannot catch that, nor a file the generator misread. The suites that read `tools/ucd/` themselves catch both: each compares what the public API gives for every code point with the file, through a table of the value names the file uses.
