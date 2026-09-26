# muni-0002. Pin Unicode 18.0.0

Status: Accepted

## Context

Every table in a text stack must come from one Unicode version, or
shaping, segmentation and bidi disagree about the same text. Unicode
18.0.0 was final and current when the library started, and HarfBuzz's
tables use it too.

## Decision

The library declares Unicode 18.0.0 and generates every table from
that release's data files. The version is available at compile time
(`MUNI_UNICODE_VERSION_*`) and at run time
(`muniGetUnicodeVersion`). A new Unicode version is a new library
release.

## Consequences

A consumer can assert the version it expects. Moving to a new Unicode
version is one deliberate change, not drift.
