# muni-0003. One table per property, with searched splits

Status: Accepted

## Context

Measured before the first table was written: one combined record per
code point (utf8proc) costs 352 KB and cannot leave out what a build
does not use; one fixed-split trie per property (ICU4X) costs 18 KB for
General_Category and 26 KB for Script; HarfBuzz, which searches the
best split per property and pools identical blocks, stores five
properties, decomposition included, in 38 to 43 KB.

## Decision

Each Unicode property has its own multi-level lookup table. The
generator searches, per table, for the split of levels that makes it
smallest, and all tables share pools so identical blocks are stored
once. Boolean properties use the same technique. Tables packed per
algorithm stay possible later as an internal optimization, invisible in
the API, if benchmarks of the segmentation loops call for it.

## Consequences

Property queries map one to one onto tables, a build that leaves out
an algorithm keeps every property query, and a property a later Unicode
version adds to an algorithm is a new table, not a new packing.
