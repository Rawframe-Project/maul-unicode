# muni-0001. Library profile

Status: Accepted

## Context

Every Maul library states in one record what its domain adds to the
family rulebook (family record 0005).

## Decision

- **Determinism:** bit-exact. Identical input gives identical output on
  every platform, compiler and architecture. The library uses no
  floating point.
- **Threads:** none. Every function runs on the caller's thread; the
  tables are immutable and every function is safe from any thread.
- **Memory:** there are no owner objects and nothing is allocated.
  Iterators and bidi workspaces are fixed-size structs the caller
  declares; results go into caller buffers, and a buffer that is too
  small is reported with the size needed.
- **Platform dependencies:** none beyond the C standard library's
  `<stddef.h>`, `<stdint.h>` and `<string.h>`.
- **Commit areas:** `api`, `bench`, `bidi`, `build`, `case`, `ci`,
  `confusables`, `docs`, `generator`, `identifiers`, `linebreak`,
  `normalize`, `properties`, `script`, `segment`, `tables`, `tests`,
  `tools`, `utf8`.

## Consequences

Consumers can call any function from any thread with no setup and no
cleanup, and results do not depend on where the program runs.
