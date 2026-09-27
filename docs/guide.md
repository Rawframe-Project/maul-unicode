# The Maul Unicode guide

Maul Unicode answers the questions a program asks about text: is it
valid, where are its characters, words and lines, which way does it
run, how does it compare, and can one name pass for another. This guide
walks through each part; [the API reference](api.md) lists every
function, and `samples/` holds small programs that use them.

## 1. The model

- **One Unicode version.** Every table comes from Unicode 18.0.0,
  generated from the Unicode Character Database by `tools/munigen` and
  checked in. `muniGetUnicodeVersion` tells a program which version it
  linked; compare it with `MUNI_UNICODE_VERSION_MAJOR` and `_MINOR` when
  stored data (skeletons, normalized keys) depends on it.
- **UTF-8 and byte offsets.** Functions take UTF-8 and report positions
  as byte offsets into it. Conversions to and from UTF-16 and UTF-32
  exist for the edges of a program.
- **Caller memory.** Nothing allocates. Iterators are plain structs on
  the caller's stack; algorithms that need scratch memory, such as bidi,
  take it as an argument; results go into caller buffers.
- **Threads.** The tables are constant and there is no global state, so
  every function is safe from any thread; an iterator is used by one
  thread at a time.
- **Named limits.** Where an algorithm could need unbounded work or
  memory, a documented limit stops it with an error instead: 125
  embedding levels and 63 open brackets in bidi, 32 combining marks in a
  row for normalization and skeletons.

## 2. Results and buffers

A fallible function returns a `muniResult`: `muni_success` (0), a
positive status such as `muni_done` or `muni_needMoreText`, or a
negative error. Functions over text return a `muniTextResult`, the
status and, on an error, the byte offset where the offending input
starts. `muniResultName` names any result for diagnostics.

Functions that write text follow one pattern: they take an output
buffer and its capacity, write what fits, and report through
`neededOut` how many units the whole result needs. A result that does
not fit returns `muni_errorCapacity`; call again with a buffer of the
needed size. A NULL buffer with capacity 0 measures.

```c
size_t needed = 0;
muniTextResult result = muniNormalize(text, length, muni_nfc, muni_convertStrict,
                                      nullptr, 0, &needed);
// result.status is muni_errorCapacity (or muni_success for empty output);
// allocate needed bytes and call again.
```

## 3. Validating and converting

Text from outside a program (network, files, the clipboard, an input
method) should be validated before anything else reads it:

```c
muniTextResult valid = muniValidateUtf8(text, length);
if (valid.status != muni_success)
{
    // valid.status says what is wrong (muni_errorUtf8Overlong, ...),
    // valid.offset where.
}
```

Validation treats input as hostile: it rejects overlong forms, encoded
surrogates, values past U+10FFFF and truncated sequences, and names each
kind of error. ASCII runs at memory speed; other text runs through a
branch-free state machine.

`muniConvertUtf8ToUtf16`, `muniConvertUtf16ToUtf8`,
`muniConvertUtf8ToUtf32` and `muniConvertUtf32ToUtf8` convert between
the encoding forms. In `muni_convertStrict` mode they stop at the first
error; in `muni_convertReplace` they substitute U+FFFD for each maximal
ill-formed subpart, as the Unicode Standard recommends, so that two
programs repairing the same bytes agree. `muniDecodeUtf8` and
`muniEncodeUtf8` handle one code point.

## 4. Character properties

`properties.h` answers per code point: General_Category, the
segmentation and line breaking classes, East_Asian_Width, Bidi_Class,
mirroring and bracket pairs, the canonical combining class, Script and
Script_Extensions, the emoji properties, White_Space and decimal digit
values. Each lookup is a few reads of a compressed table, and any
`uint32_t` is accepted: values past U+10FFFF answer as unassigned code
points do.

Scripts are ISO 15924 tags built by `MUNI_SCRIPT('L', 'a', 't', 'n')`,
the same values as HarfBuzz's `hb_script_t`.

## 5. Characters, words and sentences

A user perceives a grapheme cluster as one character: an emoji family
joined by zero width joiners, a flag of two regional indicators, a
letter with its accents, a Hindi conjunct. Caret movement, selection
and backspace must never split one. Word boundaries serve double-click
selection and word-by-word movement; sentence boundaries serve
sentence selection and text-to-speech.

An iterator reports each boundary after the start of the text, ending
with the end of the text:

```c
muniSegmentIterator iterator;
if (muniInitGraphemeIterator(&iterator, text, length, false) == muni_success)
{
    size_t start = 0;
    size_t end = 0;
    while (muniNextSegmentBreak(&iterator, &end) == muni_success)
    {
        // text[start, end) is one grapheme cluster
        start = end;
    }
}
```

`muniInitWordIterator` and `muniInitSentenceIterator` work the same
way. Between two word boundaries lies a word, a run of spaces or a
punctuation mark; the caller tells them apart, for example by the
General_Category of the first code point. `muniFindGraphemeBreaks`,
`muniFindWordBreaks` and `muniFindSentenceBreaks` write all boundaries
into an array instead, which is faster when a whole text is at hand.

**Text in pieces.** An iterator started with `moreFollows` set stops
with `muni_needMoreText` when the next boundary depends on text it has
not seen; `muniFeedSegmentIterator` then hands it the next piece, which
may start in the middle of a UTF-8 sequence. The iterator keeps no
pointer to earlier pieces, so a program can segment a stream through a
small buffer. Offsets stay relative to the start of the whole text.

## 6. Line breaking

A line iterator reports each position where a line may end, and
whether it must (after a line feed, a paragraph separator and the
like, and at the end of the text):

```c
muniSegmentIterator lines;
(void)muniInitLineIterator(&lines, text, length, false);
size_t end = 0;
bool mandatory = false;
while (muniNextLineBreak(&lines, &end, &mandatory) == muni_success)
{
    // a line may end at end; it must when mandatory is true
}
```

A layout engine measures the text between opportunities and ends each
line at the last opportunity that fits, or at the first mandatory one.
Spaces before an opportunity belong to the line before it and are not
drawn at its end.

Thai, Lao, Khmer and Burmese write words without spaces (line breaking
class SA), so finding breaks inside them needs a dictionary, which the
library does not carry. By default, as rule LB1 of UAX #14 allows, a
run of SA text has no break inside it. A program with a word segmenter
hands it to the iterator:

```c
static size_t BreakThai(void* context, const char* run, size_t length, size_t from)
{
    // Return the byte offset of the first word break in run after from,
    // or length when there is none.
}

(void)muniSetComplexBreaker(&lines, BreakThai, dictionary);
```

The iterator then asks the segmenter about each maximal run of SA text
and never breaks before a combining mark. The same hook serves word
iterators, which then report whole words in these scripts.

## 7. Bidirectional text

Hebrew, Arabic and other right-to-left scripts mix with left-to-right
text and numbers; the Unicode Bidirectional Algorithm (UAX #9) decides
the display order. `muniResolveBidi` resolves one paragraph, giving
every byte an embedding level: even levels run left to right, odd ones
right to left. It needs one level byte and one scratch byte per byte of
text, both from the caller:

```c
size_t paragraphLength = 0;
uint8_t paragraphLevel = 0;
muniResult status = muniResolveBidi(text, length, muni_bidiAuto, levels, workspace,
                                    &paragraphLength, &paragraphLevel);
```

A paragraph ends after its first paragraph separator; call again on the
rest of the text for the next one. `muni_bidiAuto` takes the direction
from the first strong character; a program that knows the direction,
such as a right-to-left user interface, passes it.

After line breaking, `muniReorderBidiLine` orders each line: it writes
the line's runs of equal level from left to right, applying rule L1
(trailing whitespace returns to the paragraph level). A renderer lays
out each run in order; the glyphs of an odd run go right to left, and
characters with a `muniGetMirroringGlyph`, such as parentheses, are
drawn mirrored there. `muniReorderBidiLevels` computes a visual order
for any units a caller has levels for, clusters or glyphs, and
`muniInvertBidiMap` turns it into a logical-to-visual map for hit
testing. `samples/bidi.c` shows the whole path.

## 8. Script runs and shaping

A shaper such as HarfBuzz shapes one script at a time. A script
iterator splits text into runs of one script, resolving Common and
Inherited characters (punctuation, digits, combining marks) into the
run around them and keeping brackets in the script of their contents:

```c
muniScriptIterator scripts;
(void)muniInitScriptIterator(&scripts, text, length, false);
muniScriptRun run;
while (muniNextScriptRun(&scripts, &run) == muni_success)
{
    // the text up to run.end has script run.script
}
```

A text layout pipeline itemizes by paragraph (bidi), then by script and
bidi run, then by font, and shapes each item.

With `-DMAUL_UNICODE_HARFBUZZ=ON`, `muniCreateHarfBuzzFunctions` (in
the separate library `maul-unicode-harfbuzz`) gives HarfBuzz its Unicode
data from Maul Unicode: general category, combining class, mirroring,
script, composition and decomposition. The whole text stack then
answers from one Unicode version:

```c
hb_unicode_funcs_t* functions = muniCreateHarfBuzzFunctions();
hb_buffer_set_unicode_funcs(buffer, functions);
```

## 9. Normalization and case

The same text can be encoded more than one way: "é" is one code point
or "e" and a combining accent. `muniNormalize` puts text into one of
the four normal forms of UAX #15: NFC and NFD for canonical
equivalence, NFKC and NFKD also folding compatibility characters such
as ligatures and full-width letters. Store and compare text in NFC.
`muniCheckNormalization` answers quickly whether text is already in a
form, and usually is, which saves the copy.

`muniConvertCase` lowercases, uppercases, titlecases (by the word
boundaries of UAX #29) and case folds, with the full mappings that
change the length ("ß" uppercases to "SS") and the context rules of
final sigma. `muni_caseTurkic` applies the Turkish and Azerbaijani
rules for dotted and dotless i, and `muni_caseLithuanian` the
Lithuanian rules that keep the dot of an i under other accents. The simple mappings of one code point
are `muniToLower`, `muniToUpper`, `muniToTitle` and `muniFoldCase`.

To compare strings without regard to case, fold both and compare the
results. To compare them as users see them, use `muniToNfkcCasefold`,
the key UAX #31 gives identifiers: it folds case, turns compatibility
forms into their plain selves, drops default ignorables such as a soft
hyphen or a zero width space, and puts the result in NFC. It needs both
components.

```c
size_t keyLength = 0;
muniTextResult result = muniToNfkcCasefold(name, length, muni_convertStrict, key,
                                           sizeof(key), &keyLength);
// "Alice", "ALICE", a full-width "ALICE" and "Al" + soft hyphen + "ice"
// all get the key "alice".
```

## 10. Identifiers and security

`identifier.h` implements the identifier rules of UAX #31:
`muniIsIdentifierStart`, `muniIsIdentifierContinue`, the pattern
syntax and whitespace properties, and `muniCheckIdentifier`, which
reports where a string stops being a default identifier. A scripting
language or a configuration format uses these for its names.

`security.h` (UTS #39) protects names that people read, such as user
names in chat:

- `muniGetSkeleton` maps a string to its skeleton; two strings are
  confusable when their skeletons are equal. "paypal" with a Cyrillic
  "а" has the skeleton of the Latin one. Skeletons follow the display
  order bidi gives the string, so they take a direction and bidi's
  workspace. Store a skeleton next to each name and compare bytes; a
  skeleton is not for display and changes with the Unicode version.
- `muniGetRestrictionLevel` grades how a string mixes scripts, from
  ASCII only to unrestricted, under the General Security Profile.
  Moderately restrictive (Latin with one other recommended script but
  Cyrillic or Greek) is a common limit for user names.
- `muniGetResolvedScripts` lists the scripts every character of a
  string can belong to; none means the string mixes scripts.
- `muniCheckMixedNumbers` finds digits from more than one decimal
  system, some of which look alike.

`samples/names.c` combines these into a user name policy.

## 11. Leaving parts out

Three components can be left out of a build with their tables:
`-DMAUL_UNICODE_NORMALIZATION=OFF`, `-DMAUL_UNICODE_CASE=OFF` and
`-DMAUL_UNICODE_SECURITY=OFF` (security needs normalization;
`muniToNfkcCasefold` needs normalization and case). Their
headers stay, so a call into a missing component fails to link rather
than at run time. CI's web build reports what each costs in a static
library.

## 12. Conformance and testing

Every algorithm with an official Unicode test file passes all of it:
the grapheme, word, sentence and line break tests, both bidi tests, and
the normalization test with its part 1 check of every code point.
NFKC_Casefold is checked against the UCD's NFKC_CF for every code
point.
Fuzz targets check that the answers of each area agree with each other,
and one compares with ICU where the two Unicode versions agree. The
generator's tables are verified against the UCD for every code point
when they are built, and CI regenerates them and fails on any
difference.
