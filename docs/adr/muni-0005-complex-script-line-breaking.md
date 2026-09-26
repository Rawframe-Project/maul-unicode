# muni-0005. Complex-script line breaking through a hook

Status: Accepted

## Context

Thai, Lao, Khmer and Burmese write words without spaces. UAX #14 gives
their letters the class SA and says that breaking them needs
"dictionary lookup of some form"; without it, SA is treated as AL and a
sentence has no break between words. Dictionaries are large: ICU4X's
four take 1.8 MB, its neural models 310 KB, against 27 KB for all line
breaking rules.

## Decision

Line and word breaking hand each run of SA text to an optional
caller-supplied function that returns the break positions inside the
run. Without one, the run follows UAX #14 rule LB1. First-party
dictionaries may come later as an optional component whose data is
loaded at run time, never compiled in, without changing the API.

## Consequences

No build pays for dictionaries it does not use, and a consumer that
ships in these languages can plug in any segmenter today.
