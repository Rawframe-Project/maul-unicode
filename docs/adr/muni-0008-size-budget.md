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
| Case mapping and folding | 9,775 | Its first measured size (muni-0012) and Soft_Dotted (muni-0018) |
| Security (UTS #39) | 36,601 | Its first measured size (muni-0014) |
| Emoji properties, White_Space and decimal digits | 2,058 | Their first measured size (muni-0015) |
| Default_Ignorable_Code_Point | 357 | Its first measured size (muni-0017) |
| Bidi_Class and the bidi brackets | 5,200 | Their first measured size |
| Script_Extensions | 1,633 | Its first measured size |
| Identifier properties | 6,215 | Their first measured size |

East_Asian_Width counts in the line breaking data (rule LB30 reads it)
and Indic_Conjunct_Break in the grapheme breaking data (rule GB9c).

Components without a measured reference get their ceiling here when
their tables first exist. `tools/table-budget.txt` lists every ceiling
with the generated tables it covers, and `tools/table_budget.py`
checks the generator's report against it in CI: a component past its
ceiling, or a table no component covers, fails the build. The web job
reports each optional component's cost in the built library too.

## Consequences

Size stays a design constraint from the first table, measured rather
than hoped for.
