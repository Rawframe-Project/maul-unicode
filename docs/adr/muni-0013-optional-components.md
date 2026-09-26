# muni-0013. Normalization and case as optional components

Status: Accepted

## Context

The requirements let builds leave out features they do not use, so they
pay nothing for them. Normalization carries 34.7 KB of tables and case
9.6 KB; a game that only lays out text needs neither.

## Decision

`MAUL_UNICODE_NORMALIZATION` and `MAUL_UNICODE_CASE`, both on by
default, take out each component's sources and tables, and its tests and
fuzz target. The headers stay: a build without a component fails to
link a call into it, rather than failing at run time. CI builds and
tests a cell with both off.

## Consequences

The static library shrinks from 282 KB to 202 KB (GCC 14) with both off.
Confusables (muni-0006) will follow the same pattern.
