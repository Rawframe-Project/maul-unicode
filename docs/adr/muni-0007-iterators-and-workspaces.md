# muni-0007. Caller-owned iterators and workspaces

Status: Accepted

## Context

The algorithms must not allocate on hot paths, must process text that
arrives in pieces, and must report byte offsets. Library-owned iterator
objects (ICU4C) allocate; filled arrays per byte (libunibreak) need the
whole text and one byte of output per byte of input.

## Decision

Segmentation, line breaking and script itemization are iterators:
fixed-size structs, declared in the public headers and documented as
opaque, that the caller keeps on its stack. Initializing one allocates
nothing and there is nothing to close. An iterator that cannot decide a
boundary before the end of the current piece returns a status that asks
for the next piece and continues from there. Each iterator has an
array convenience that writes boundary offsets into a caller buffer.
Bidi uses a caller-provided fixed-size workspace, possible because
UAX #9 bounds its own state (a depth of 125 and a bracket stack of 63).
Each algorithm has one entry point per encoding, UTF-8 first. Property
lookups cannot fail and return their value directly.

## Consequences

No function allocates, streaming text works, and the common case is one
call.
