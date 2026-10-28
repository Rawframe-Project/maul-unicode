# Validating and converting text

Text from outside a program (files, the network, the clipboard, an input method) is validated before anything else reads it:

```c
muniTextResult valid = muniValidateUtf8(text, length);
if (valid.status != muni_success)
{
    // valid.status says what is wrong (muni_errorUtf8Overlong, ...),
    // valid.offset where.
}
```

Validation treats its input as hostile. It accepts exactly the well-formed sequences of the Unicode Standard and names each kind of error: a byte that cannot start a sequence, a missing continuation byte, a truncated sequence, an overlong form, an encoded surrogate, a value past U+10FFFF. ASCII runs at memory speed; other text runs at a few gigabytes per second.

## Conversion

`muniConvertUtf8ToUtf16`, `muniConvertUtf16ToUtf8`, `muniConvertUtf8ToUtf32` and `muniConvertUtf32ToUtf8` convert between the encoding forms, into caller buffers as described in [Results and buffers](getting-started.md#results-and-buffers). The mode says what happens to ill-formed input:

- `muni_convertStrict` stops at the first error and reports it with its offset; `neededOut` then counts the units before it.
- `muni_convertReplace` writes U+FFFD for each maximal ill-formed subpart and goes on. This is the practice the Unicode Standard recommends, so two programs that repair the same bytes agree.

`muniValidateUtf16` checks that every surrogate is half of a pair. `muniDecodeUtf8` and `muniEncodeUtf8` handle one code point; on an error the decoder reports the length of the maximal subpart, at least one byte, so a loop that skips it always makes progress. The encoder takes the room left in its output and writes nothing when the code point does not fit, reporting the 1 to 4 bytes it needs, so a loop that encodes into the rest of a buffer cannot write past its end.

## Not obvious

- Every other function that reads UTF-8 (segmentation, bidi, script runs) treats each maximal ill-formed subpart as one U+FFFD rather than failing, so offsets stay meaningful over damaged text. Functions that produce text (normalization, case, skeletons) take a mode, or fail with the UTF-8 error and its offset.
- `muni_errorUtf8Truncated` at the end of a piece is not necessarily an error when text arrives in pieces; the iterators keep the cut sequence and complete it from the next piece.
