# muni-0012. Case data and language rules

Status: Accepted

## Context

Case mapping has simple mappings (one code point for one), full
mappings that change the length ("ß" to "SS"), foldings for caseless
comparison, and rules that need context: the final sigma, and language
rules for Turkish, Azerbaijani and Lithuanian (SpecialCasing.txt).
Titlecasing a string needs word boundaries. 7,357 code points carry
case data.

## Decision

Each code point has a record: the indexes of its upper, lower, title
and fold distances among the 201 distinct ones, and flags (Cased,
Case_Ignorable, special). 202 distinct records cover every code point,
so the table stores an 8-bit record index (5,729 bytes). The 105 code
points whose full mappings differ from the simple ones list all four
full mappings in a sorted table over a UTF-16 pool. Records, distances
and the special table take 3,900 bytes: 9,629 in all, which becomes the
case ceiling in muni-0008.

Final_Sigma and the Turkic rules (dotted and dotless i, the dot above
after I) are code, chosen with muni_caseTurkic. Lithuanian rules, which
keep the dot above i with accents, are not supported. Titlecasing takes
the word boundaries of UAX #29 from the word iterator.

## Consequences

Case costs under 10 KB and one table lookup per code point, and Turkish
text cases correctly. Titlecasing is not idempotent when a combining mark
titlecases to a letter (U+0345 to capital iota), which moves the word
boundaries; the standard's definition gives that, and the fuzz target
checks idempotence only for the other operations.
