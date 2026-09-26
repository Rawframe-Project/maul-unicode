# muni-0009. One segment iterator, rules that may hold a boundary

Status: Accepted

## Context

UAX #29 defines three segmentations with the same shape: walk the text
and decide, before each code point, whether a boundary falls there.
Grapheme rules look only back. Word rules WB6, WB7b and WB12 look one
code point ahead, past any Extend, Format and ZWJ. Sentence rule SB8
looks ahead without limit: after an abbreviation's period the sentence
goes on when a lowercase letter comes before any other letter, separator
or terminator. Text may arrive in pieces (muni-0007), so a lookahead can
cross a piece.

Three iterator types with three feed and three next functions would
triple the API and the code around it for no difference a caller can
use.

## Decision

One public type, `muniSegmentIterator` (64 bytes, caller-owned), with an
init function per segmentation and shared `muniFeedSegmentIterator` and
`muniNextSegmentBreak`, plus a `muniFind...Breaks` array convenience per
segmentation.

Inside, one engine (src/segmenter.h) walks the text and asks a rule set
about each code point. A rule set answers break, join, or hold. On hold
the engine remembers the boundary's offset and asks the rule set's
resolve function about each following code point until it answers join
(no boundary after all), break (report the held boundary, then decide
about the current code point afresh) or continue (read on). The end of
the text breaks a hold. A held boundary never needs buffered text: the
rules' state holds what the lookahead has seen, and the offset is all
that is kept.

The engine is a static inline function taking the rules as function
arguments; each rule set's file calls it with its own functions, so each
gets a loop with its rules inlined. The public next function dispatches
once per boundary, not per code point.

Feeding is accepted only after the iterator returned muni_needMoreText;
earlier, the unread text would be lost, so the call fails instead.

## Consequences

Word and sentence segmentation cost one rule file each over the shared
engine, and line breaking can reuse the engine's hold for its own
lookahead. Callers learn one iterator. Word and sentence boundaries are
the standard's, which do not always fall on grapheme boundaries (a
Prepend character that is also Numeric, like U+0600, takes a word
boundary after it before punctuation); a caller wanting both snaps one
to the other.
