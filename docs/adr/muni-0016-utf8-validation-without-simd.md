# muni-0016. UTF-8 validation without SIMD

Status: Accepted

## Context

The requirements ask for fast validation, with SIMD where it is
portable. Validation reads ASCII eight bytes at a time; the rest went
through the decoder one code point at a time. Measured on 16 MiB with
GCC 14: ASCII 35 GB/s, about half a memory scan; mixed text 2.4 GB/s;
Cyrillic 0.86 and CJK 0.95 GB/s. SIMD validators such as simdutf's
reach 10 GB/s and more on such text, but through SSSE3, AVX2 or NEON
table lookups. SSSE3 and AVX2 are not in the x86-64 baseline, so they
need runtime dispatch; WebAssembly needs its own path again.

## Decision

Validation outside ASCII runs a shift-based DFA over byte classes: a
class's 64-bit row holds, at each state's bit offset, the offset of the
next state, so each byte costs one table load and one shift, with no
branch, and the shift count's high bits are ignored by the hardware for
free. The DFA checks for an error every 32 bytes; only then does the
exact decoder run, from the last point known to lie between code
points, to name the error and its offset. The tables take 352 bytes.

Measured the same way: mixed text 3.5 GB/s, Cyrillic 2.3 and CJK 2.6.
SIMD stays out: in plain C it is not portable, and with intrinsics it
would need dispatch code for three instruction sets to gain on text
that trust boundaries mostly see as ASCII, which already runs at memory
speed.

## Consequences

One portable code path, 2.7 times faster outside ASCII. A test places
every combination of edge bytes after prefixes of every length, so
errors fall on both sides of the checks; the fuzz target and the
exhaustive comparisons of short strings still hold.
