# Segmentation

Grapheme, word and sentence boundaries (UAX #29), line break opportunities (UAX #14) and script runs are iterators over caller-owned state that take text in pieces. This page explains the engine they share and the traps in it.

## One iterator type

UAX #29 defines three segmentations of the same shape: walk the text and decide, before each code point, whether a boundary falls there. Line breaking has the same shape with one more answer, a mandatory break. Three or four iterator types with their own feed and next functions would multiply the API for no difference a caller can use, so there is one public type, `muniSegmentIterator`, with an init function per kind and shared `muniFeedSegmentIterator` and `muniNextSegmentBreak`.

The public struct is opaque storage. [`src/segment.c`](../../src/segment.c) copies the internal `muniSegmenter` in and out of it with `memcpy`, which keeps every access within the C aliasing rules, and dispatches on the kind once per boundary, not per code point.

## The engine and its holds

[`src/segmenter.h`](../../src/segmenter.h) walks the text and asks a rule set about each code point: break, mandatory break, join, or hold. The engine is a `static inline` function that takes the rules as function arguments, and each rule file ([`grapheme.c`](../../src/grapheme.c), [`word.c`](../../src/word.c), [`sentence.c`](../../src/sentence.c), [`line.c`](../../src/line.c)) calls it with its own, so the compiler builds one loop per rule set with the rules inlined.

Grapheme rules only look back. Others look ahead: word rules WB6, WB7b and WB12 one code point past any Extend, Format and ZWJ; sentence rule SB8 without limit (after an abbreviation's period the sentence goes on when a lowercase letter comes before any other letter, separator or terminator); line rules LB15b, LB15c, LB19a, LB25 and LB28a one or two code points. Since text may arrive in pieces, a lookahead may cross a piece.

A rule that cannot decide answers hold. The engine remembers the boundary's offset and asks the rule set's resolve function about each following code point until it answers join (no boundary after all), break (report the held boundary, then decide about the current code point afresh) or continue (read on). The end of the text breaks a hold. A held boundary never needs buffered text: the rule state remembers what the lookahead has seen, and the offset is all the engine keeps.

Each rule set keeps a few bytes of state beyond the previous code point: whether an odd number of regional indicators precedes, where an emoji ZWJ sequence or a sentence ending stands, the last class other than a space for the "X SP* ×" line rules. The comments at the top of each rule file list what it keeps and why.

## Text in pieces

The cursor ([encoding.md](encoding.md#conversion-and-pieces)) keeps the bytes of a sequence a piece cut off and asks for more text with `muni_needMoreText`. The iterator records that it asked, and a feed is accepted only then: fed earlier, the unread rest of the current piece would be lost. The iterator never keeps a pointer to an earlier piece, so the caller may reuse its buffer.

## Complex-script runs

Thai, Lao, Khmer and Burmese (Line_Break=SA) need a dictionary to break between words. Dictionaries are large (megabytes for the four languages, hundreds of kilobytes for neural models, against tens of kilobytes for all the line breaking rules), so the library carries none and hands each maximal SA run to the caller's breaker ([`src/complex.c`](../../src/complex.c)); without one, the run follows rule LB1. A first-party dictionary could later be an optional component loaded at run time without changing the API.

A run lies within the current piece and starts at an SA code point read whole from it. The breaker is asked for the next break each time the iterator passes the previous one. Its answer is caller input and is clamped: an answer at or before the position asked about, or past the run, means no break inside the run. A break before a combining mark is ignored.

## Class overrides

Locale data such as CLDR changes segmentation in two ways. Suppressions, like the abbreviation lists that keep "Mr." from ending a sentence, only remove boundaries, so a caller filters the boundaries an iterator reports and the library needs nothing. A class change, such as Greek's semicolon as STerm, also adds boundaries, so it has to reach the rules. `muniSetBreakClassOverrides` gives a word or sentence iterator a caller-owned table of code points and classes, and the rule file's lookup consults it before the rules see the class.

The table is sorted by code point and checked once when it is set (code points, order, classes), so the lookup is a binary search and a table of any length costs a logarithm per code point, keeping segmentation linear in the text. With no table the lookup skips it. The iterator stores the pointer in its last eight bytes and the count in the word or sentence rule state, which has room for it, so the public iterator keeps its size. Only the lookup changes: rules that read another property of the code point, such as WB3c's Extended_Pictographic or the Line_Break=SA runs a word iterator hands to a complex breaker, see Unicode's values.

## CLDR root

CLDR's `common/segments/root.xml` writes the same rules as regular expressions, and locale files tailor them. Checked against CLDR 49, its word and sentence rules are UAX #29's. Two rules differ, and the library follows the current UAX in both, since its every answer comes from one Unicode version and the conformance files define them:

- Line breaking, LB12a: root writes `[^ $SP $BA $HY $HH] × $GL`, which allows a break after BA before GL. Unicode 18 keeps BA × GL (`LineBreakTest.txt` has a BA × GL case under rule 12.1).
- Grapheme clusters, GB9c: root still requires a consonant before the linker; Unicode 18 does not (see the trap below).

Elsewhere root takes every class from the Unicode properties, so a difference can only come from the data's Unicode version.

## Script runs

[`src/script_runs.c`](../../src/script_runs.c) keeps, for the current run, the set of scripts every code point in it can belong to, as a 256-bit mask over Script indexes. A code point with a real Script narrows the set to its Script or its Script_Extensions, and the run ends where that would leave no script; Common and Inherited code points leave it alone. Opening brackets remember the run's script on a stack of 32, whose oldest entry drops when it is full; a closing bracket that matches one counts as a code point of that script, so brackets around foreign text land in the script outside. The iterator shares the cursor and the piece protocol with the segmenters.

## Traps

- Word and sentence boundaries are the standard's and do not always fall on grapheme boundaries (U+0600, Prepend and numeric, takes a word boundary after it before punctuation). Do not snap them inside the library; callers that need both snap one to the other.
- GB9c as Unicode 18 defines it joins an InCB linker with the next consonant whether or not a consonant precedes the linker in the cluster; older versions, and ICU builds of them, required one. The ICU fuzz target skips such text.
- The segmentation conformance files run three ways, including fed one byte at a time, which reaches every hold across a piece boundary. A rule change that passes the whole-text run but not the bytewise one keeps state the hold does not carry.
