# UTF-8 handling

Every module that reads text decodes it through one step, `muniStepUtf8` in [`src/encoding.h`](../../src/encoding.h). It follows the Unicode Standard's table of well-formed byte sequences, where the allowed range of the second byte depends on the lead byte (A0..BF after E0, 80..9F after ED, 90..BF after F0, 80..8F after F4). On an error it returns U+FFFD and the length of the maximal ill-formed subpart, at least one byte, so every reader makes progress and every reader agrees on where U+FFFD goes.

## Validation

`muniValidateUtf8` is the hot path at trust boundaries, and most text there is ASCII. It reads ASCII eight bytes at a time, which runs at about half the speed of a memory scan. Outside ASCII it runs a shift-based DFA over byte classes ([`src/encoding.c`](../../src/encoding.c)): a class's 64-bit row holds, at each state's bit offset, the offset of the next state, so each byte costs one table load and one shift with no branch. The DFA looks for an error only every 32 bytes; when it finds one, the exact decoder runs from the last point known to lie between code points to name the error and its offset. The tables take 352 bytes.

SIMD validators are faster on non-ASCII text, but portable C cannot express them, and with intrinsics they need runtime dispatch for SSSE3 and AVX2 (not in the x86-64 baseline), NEON and WebAssembly. One portable path that runs at memory speed on ASCII and at a few gigabytes per second elsewhere was chosen instead.

## Conversion and pieces

Conversions write through the caller's buffer and count every unit, so a too-small buffer still learns the size needed. The replacement mode substitutes per maximal subpart, exactly as validation and decoding delimit errors.

Text in pieces is read through a cursor ([`src/cursor.h`](../../src/cursor.h)) that decodes one code point at a time, keeps the bytes of a sequence a piece cuts off (at most three) until the next piece completes it, and counts offsets from the start of the whole text. The fast path decodes straight from the piece when no bytes are pending and four or more remain.

## Traps

- The test of validation compares it with an independent reference over every string of up to three bytes, systematic four-byte strings, and edge bytes after prefixes of every length. That last set exists because the DFA's 32-byte checks and the ASCII runs create boundaries an error can fall on either side of; keep it when changing either.
- A cursor may be fed only after it answered `muni_needMoreText`; the iterators refuse an earlier feed before reaching it, and the cursor asserts the precondition.
