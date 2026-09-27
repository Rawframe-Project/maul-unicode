# muni-0018. Lithuanian case rules

Status: Accepted

## Context

muni-0012 left out the Lithuanian rules of SpecialCasing.txt, the last
language rules the file has. Lithuanian keeps the dot of an i or a j
under other accents above: lowercasing I, J and I with ogonek writes the
dot out before such an accent (More_Above), and the precomposed I with
grave, acute or tilde lowercase to an i, the dot and the accent;
uppercasing and titlecasing drop a dot above after a Soft_Dotted letter
(After_Soft_Dotted). Soft_Dotted is a property the library had no data
for: 54 code points in 36 ranges.

## Decision

`muni_caseLithuanian` selects the rules, which are code beside the
Turkic ones in `src/case.c`; folding has none. Soft_Dotted is a sorted
list of the 36 ranges, 146 bytes, searched only when a dot above
follows a letter in Lithuanian upper- or titlecasing; the generator
checks the list against PropList.txt for every code point. A flag bit
in the case records would have cost 1,088 bytes, the records splitting
where the letters sit among others.

The case ceiling in muni-0008 becomes 9,775 bytes, the old 9,629 and
the list.

## Consequences

Every language rule of SpecialCasing.txt is supported, and the
unit tests and the ICU comparison agree with ICU 74's "lt". Lithuanian
uppercasing is not idempotent: it drops one dot above after a
soft-dotted letter, so a letter that uppercasing keeps, such as U+1D422
MATHEMATICAL BOLD SMALL I, loses a second dot in a second pass; the
fuzz target leaves that operation out of its idempotence check.
