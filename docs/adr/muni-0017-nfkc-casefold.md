# muni-0017. NFKC_Casefold from the existing tables

Status: Accepted

## Context

UAX #31 compares identifiers without regard to case with
NFKC_Casefold, the property NFKC_CF of DerivedNormalizationProps.txt
applied to each code point and followed by NFC. Callers such as a user
name policy approximated it with case folding between two NFKC passes,
which keeps default ignorables (a soft hyphen or a zero width space
inside a name) and differs for a few characters.

ICU stores NFKC_CF as normalization data of its own. Its values follow
from data the library already has: compatibility decompositions, full
case folding and Default_Ignorable_Code_Point, repeated until nothing
changes.

## Decision

`muniToNfkcCasefold` (in `case.h`) computes each code point's value at
run time: the code point's default ignorables are dropped, the rest
folded and decomposed (NFKD), and the round repeats until it changes
nothing. The values come out decomposed, canonically equivalent to the
composed ones of the UCD, and the normalizer of `muniNormalize`,
shared through `src/normalizer.h`, orders and composes them into NFC.
The test compares every code point with NFKC_CF in the UCD, and a fuzz
target compares text with ICU's NFKC_Casefold.

Default_Ignorable_Code_Point moves from the security data into the
core, as a table of its own (357 bytes, its ceiling in muni-0008):
the Security table keeps Identifier_Status
alone and shrinks from 5,885 to 5,109 bytes, so the security
component falls to 35,513 bytes. `muniIsDefaultIgnorable` moves to
`properties.h`, which `security.h` includes.

The function is built when both the case and the normalization
components are; it needs no data of its own.

## Consequences

No NFKC_CF table: the values of its 10,671 listed code points come
from data the library carries anyway, for 6.3 KB of code (GCC 14,
x86-64, -O3) and some speed: 143 MiB/s on the benchmark text, where
case folding runs at 170 and NFC at 193.

NFKC_Casefold is not invariant under canonical equivalence where
U+0345 is involved, by its per code point definition: U+1FB3 U+0301
and its NFD get different results, as in ICU. The fuzz target's
canonical invariance check leaves such text out.
