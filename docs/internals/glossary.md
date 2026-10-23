# Glossary

Terms the code, the tests and the documentation use with one meaning.

- **Code point:** a value from U+0000 to U+10FFFF. A **scalar value** is a code point that is not a surrogate (U+D800 to U+DFFF); only scalar values are encoded.
- **Maximal ill-formed subpart:** the longest prefix of an ill-formed UTF-8 sequence that could start a well-formed one, or one byte. Each becomes one U+FFFD, the Unicode Standard's recommended practice, so every reader agrees on offsets over damaged text.
- **Piece:** one part of a text that arrives in several. Offsets are always from the start of the whole text.
- **Boundary:** a position between code points that a segmentation reports. Iterators report every boundary after the start, the end included.
- **Break opportunity:** a position where a line may end; it is **mandatory** where the line must end.
- **Hold:** a rule's answer that the boundary before a code point depends on text not yet read. The segmentation engine keeps the offset and asks the rule again about each following code point.
- **SA run:** a maximal run of Line_Break=SA text (Thai, Lao, Khmer, Burmese), handed to the caller's **breaker**, a word segmenter.
- **Grapheme cluster:** an extended grapheme cluster of UAX #29, what a user perceives as one character.
- **Embedding level:** the bidi level of a byte; even runs left to right, odd right to left. The **paragraph level** is 0 or 1.
- **Workspace:** caller memory an algorithm uses as scratch, one byte per byte of text for bidi, two for skeletons.
- **Script run:** a stretch of text a shaper shapes with one script.
- **Segment (normalization):** a starter, a code point of combining class 0, and the marks after it, at most 32 code points.
- **Record (case):** the per-code-point case data, indexes of four mapping distances and flags. Not a design record; there are none.
- **Special (case):** a code point whose full case mappings differ from its simple ones.
- **Prototype:** what `confusables.txt` maps a character to; a **skeleton** is the text of prototypes in display order, compared byte for byte.
- **Resolved script set:** the scripts every character of a string can belong to (UTS #39 section 5.1).
- **Rank index:** a sparse mapping stored as block numbers, a 64-bit map per block and entries in code point order; an entry's position is its block's rank plus the set bits below it.
- **Ceiling:** the most bytes a component's generated tables may take, in `tools/table-budget.txt`.
- **Component:** a part of the library a build can leave out: normalization, case, security.
- **Generator:** `tools/munigen`, which writes `src/generated/` from `tools/ucd/`.
