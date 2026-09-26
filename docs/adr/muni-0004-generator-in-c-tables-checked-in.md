# muni-0004. A generator in C, generated tables checked in

Status: Accepted

## Context

The tables are derived data. They must be reproducible, reviewable,
and not require a scripting runtime to build the library.

## Decision

The generator is a C program in `tools/` that reads the pinned Unicode
data files and writes the table sources. It runs at development time,
never in a consumer's build. The generated sources are committed; CI
rebuilds the generator, regenerates the tables and fails unless the
output is byte-identical to what is committed. The generator also looks
up every code point through the generated tables and compares the
answer with the data files, so a table is exhaustively verified before
it is written.

## Consequences

Building needs only a C compiler, table changes show in review, and a
generator change that alters a table cannot go unnoticed.
