# muni-0008. Size budget

Status: Accepted

## Context

The library must be small enough for web builds. Measured
implementations of the same data give honest ceilings.

## Decision

No component may be larger than the smallest measured implementation
that stores the same data:

| Component | Ceiling in bytes | Measured on |
|---|---|---|
| General_Category, Canonical_Combining_Class, mirroring, Script and decomposition | 42,627 | HarfBuzz tables |
| Line breaking data | 26,577 | ICU4X line break rules |
| Word breaking data | 18,488 | ICU4X word break rules |
| Sentence breaking data | 18,379 | ICU4X sentence break rules |
| Grapheme breaking data | 12,422 | ICU4X grapheme rules |
| Compatibility decomposition | 22,623 | Its first measured size (muni-0011) |

Components without a measured reference get their ceiling here when
their tables first exist. CI reports every component's size; the
ceilings are checked when a release is made.

## Consequences

Size stays a design constraint from the first table, measured rather
than hoped for.
