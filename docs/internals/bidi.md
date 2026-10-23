# Bidi

The bidirectional algorithm (UAX #9) works on a whole paragraph: isolating run sequences join text across isolates, and bracket pairs and neutral runs look arbitrarily far ahead. It cannot stream like segmentation. Implementations usually keep several arrays per character (classes, levels, run lists, pair lists); Maul Unicode keeps two bytes per byte of text, both in caller memory, and nothing else.

## Two bytes per byte

`muniResolveBidi` writes one level per byte, the output a layout engine indexes by byte offset, like HarfBuzz clusters. Its only other memory is the workspace, also one byte per byte ([`src/bidi_core.h`](../../src/bidi_core.h)). Each code point keeps its data at its first byte: its current bidi class in the low five bits, a mark for each bracket of a pair BD16 found, and whether it was NSM before rule W1 (for the last part of rule N0). The other bytes of a code point, and code points rule X9 removes, carry markers that walks skip without decoding.

Nothing else is stored, so what a list would hold is recomputed by walking:

- Isolating run sequences are walked, not listed: the matching PDI of a valid isolate is the next code point at its initiator's level, since the isolate's contents sit higher.
- Bracket pairs never cross, so a marked opening bracket's partner is the marked closing bracket that balances it ([`src/bidi_brackets.c`](../../src/bidi_brackets.c)). N0 resolves each pair when it reaches the opening bracket, so a pair sees the directions earlier pairs gave their brackets, as the rule requires.
- The directional status stack (127 entries) and the BD16 stack (63) are fixed by the standard and live on the C stack.

Walks rescan, bounded by the nesting the standard allows (125 isolates, 63 brackets).

## Phases

[`src/bidi_explicit.c`](../../src/bidi_explicit.c) runs P1 to P3 and X1 to X9: the paragraph, its level and the explicit levels. [`src/bidi_implicit.c`](../../src/bidi_implicit.c) runs X10, W1 to W7 and N0 to N2 over each isolating run sequence in a few passes, keeping the explicit levels until every sequence is done, because X10 computes sequences and their sos and eos from them; I1 and I2 run last. A paragraph that is left to right with no R, AL, AN or explicit formatting character resolves to level 0 without the implicit phase.

[`src/bidi_line.c`](../../src/bidi_line.c) applies L1 and L2 per line. L2 reverses, from the highest level down to the lowest odd one, every maximal stretch at or above that level; over runs, a line of n runs costs at most 126 n steps and no memory. Skeletons use its rule L1 on their own copy of the levels (see [security.md](security.md)).

## Traps

- A CR LF pair ends one paragraph, not two.
- ICU 74 leaves a nonspacing mark after a bracket out of rule N0, against `BidiCharacterTest.txt`, and keeps pairing past 63 open brackets where BD16 stops; the ICU fuzz target skips such text and compares the rest as displayed, paragraph by paragraph, since ICU takes shortcuts that change levels but not the display.
- The bidi conformance test gives each case its text, levels, workspace and runs in heap allocations of exactly their size, so AddressSanitizer sees any access past them. Keep that when changing the harness.
