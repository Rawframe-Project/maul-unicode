# muni-0010. Bidi in two bytes per byte of caller memory

Status: Accepted

## Context

The bidi algorithm (UAX #9) works on a whole paragraph: isolating run
sequences join text across isolates, and bracket pairs and neutral runs
look arbitrarily far ahead. It cannot stream like segmentation
(muni-0007). Implementations keep several arrays per character: ICU
keeps classes, levels and run lists; the Rust unicode-bidi crate keeps
two class arrays per byte plus vectors of sequences and pairs.

## Decision

`muniResolveBidi` resolves one paragraph of UTF-8 into one level per
byte, the output a layout engine indexes by byte offset, like HarfBuzz
clusters. Its only other memory is a workspace byte per byte. Each code
point keeps its data at its first byte: the current class in five bits,
BD16 pair marks in two, and whether it was NSM before W1 in the last.
Continuation bytes and code points X9 removes carry markers that walks
skip without decoding.

Nothing else is stored. Isolating run sequences are walked, not listed:
the matching PDI of a valid isolate is the next code point at its
initiator's level, since the isolate's contents sit higher. Bracket
pairs never cross, so a marked opening bracket's partner is the marked
closing bracket that balances it. The directional status stack (127
entries) and the BD16 stack (63) are fixed and live on the call stack.
The implicit rules run over each sequence in a few passes that keep the
explicit levels until every sequence is done; I1 and I2 run last.

A paragraph that is left to right with no R, AL, AN or explicit
formatting character resolves to level 0 without the implicit phase.

Lines are ordered by `muniReorderBidiLine`, which applies L1 and L2 and
returns runs in visual order, and `muniReorderBidiLevels` gives a visual
order for any units a caller has levels for. A CR LF pair ends one
paragraph.

## Consequences

Bidi costs two bytes per byte of text and allocates nothing. Walks
recompute what lists would store: finding a matching PDI or a bracket
partner rescans, bounded by the nesting (125 isolates, 63 brackets).
Both conformance files pass completely.
