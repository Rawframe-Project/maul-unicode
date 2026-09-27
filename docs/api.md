# Maul Unicode API reference

Generated from the public headers by `tools/gen_api.py`. The headers
are the source of truth; this file mirrors them.

## `base.h`

The base of the Maul Unicode API: the library and Unicode versions, the export and attribute macros, and the result codes every fallible function returns.

```c
muniVersion muniGetVersion(void);
```
Returns the version of the library that was linked, which may differ from the MUNI_VERSION macros a program was compiled with.  @return The library version. @par Thread safety Safe from any thread.

```c
muniVersion muniGetUnicodeVersion(void);
```
Returns the Unicode version every table in the library comes from.  @return The Unicode version, 18.0.0 for this release. @par Thread safety Safe from any thread.

```c
const char* muniResultName(muniResult result);
```
Returns the name of a result code, for diagnostics.  @param result  Any value; an unknown one is named as such. @return A static, NUL-terminated string such as "muni_errorCapacity". @par Thread safety Safe from any thread.

## `bidi.h`

The Unicode Bidirectional Algorithm (UAX #9): the display order of text that mixes left-to-right scripts, such as Latin, with right-to-left ones, such as Arabic and Hebrew. muniResolveBidi takes a paragraph of UTF-8 and gives every byte an embedding level: even levels run left to right, odd ones right to left. A layout engine breaks the paragraph into lines, and for each line muniReorderBidiLine gives the runs of equal level in the order they appear on screen, left to right. Glyphs of an odd run are laid out right to left, and characters with a muniGetMirroringGlyph are drawn mirrored there, as rule L4 asks. Nothing allocates: the caller gives one level byte and one workspace byte per byte of text, and the fixed stacks of the algorithm (125 embedding levels, 63 open brackets) live on the call stack.

```c
MUNI_NODISCARD MUNI_API muniResult muniResolveBidi(const char* text, size_t length, muniBidiDirection direction, uint8_t* levels, uint8_t* workspace, size_t* paragraphLengthOut, uint8_t* paragraphLevelOut);
```
Resolves the embedding levels of the first paragraph of a UTF-8 text (rules P1 to I2). The paragraph ends after the first paragraph separator, such as LF, CR LF or U+2029, or at the end of the text; call again on the rest for the next paragraph. Code points that rule X9 removes, such as U+202A to U+202E, take the level before them.  @param text                The text. May be NULL when length is 0. @param length              The number of bytes. @param direction           The paragraph direction, or muni_bidiAuto. @param levels              Receives one level per byte of the paragraph; holds length bytes. @param workspace           Scratch memory of length bytes. @param paragraphLengthOut  Receives the paragraph's length in bytes. @param paragraphLevelOut   Receives the paragraph embedding level, 0 or 1. @return `muni_success`, or `muni_errorInvalid` for a NULL argument or an unknown direction. @par Thread safety Safe from any thread on memory no other thread uses.

```c
MUNI_NODISCARD MUNI_API muniResult muniReorderBidiLine(const char* line, const uint8_t* levels, size_t length, uint8_t paragraphLevel, muniBidiRun* runs, size_t capacity, size_t* countOut);
```
Orders one line of a resolved paragraph for display (rules L1 and L2): writes its runs from left to right. Trailing whitespace and isolate formatting characters, and those before a tab or paragraph separator, take the paragraph level first.  @param line            The line's text within the paragraph. @param levels          The line's levels from muniResolveBidi. @param length          The line's length in bytes. @param paragraphLevel  The paragraph embedding level. @param runs            The output. May be NULL when capacity is 0. @param capacity        The number of runs the output can hold. @param countOut        Receives the number of runs. @return `muni_success`; `muni_errorCapacity` when the runs do not fit, and then none is written; `muni_errorInvalid` for a NULL argument. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniResult muniReorderBidiLevels(const uint8_t* levels, size_t count, size_t* visualToLogicalOut);
```
Computes a visual order from levels (rule L2), for any units the caller orders: code points, clusters or glyphs of one line, each with its level after rule L1.  @param levels             One level per unit, in logical order. @param count              The number of units. @param visualToLogicalOut Receives, for each position from left to right, the logical index of its unit. @return `muni_success`, or `muni_errorInvalid` for a NULL argument. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniResult muniInvertBidiMap(const size_t* map, size_t count, size_t* inverseOut);
```
Inverts a visual-to-logical map into a logical-to-visual one, or the other way round.  @param map         A permutation of 0 to count - 1. @param count       The number of entries. @param inverseOut  Receives the inverse permutation. @return `muni_success`, or `muni_errorInvalid` for a NULL argument or an entry out of range. @par Thread safety Safe from any thread.

## `case.h`

Case mapping and case folding (Unicode chapter 3.13): lowercase, uppercase and titlecase, and folding for comparing text without regard to case. Per code point, the simple mappings give one code point for one. Over text, the full mappings may change the length ("ß" uppercases to "SS"), a final sigma lowercases to "ς", and titlecasing starts each word (UAX #29) with a capital. Turkish and Azerbaijani, which pair a dotted and a dotless i, take muni_caseTurkic; Lithuanian, which keeps the dot of an i under other accents, takes muni_caseLithuanian.

```c
uint32_t muniToLower(uint32_t codePoint);
```
Returns the simple lowercase mapping of a code point.  @param codePoint  Any value. @return The lowercase code point, or codePoint when it has none. @par Thread safety Safe from any thread.

```c
uint32_t muniToUpper(uint32_t codePoint);
```
Returns the simple uppercase mapping of a code point.  @param codePoint  Any value. @return The uppercase code point, or codePoint when it has none. @par Thread safety Safe from any thread.

```c
uint32_t muniToTitle(uint32_t codePoint);
```
Returns the simple titlecase mapping of a code point, which differs from the uppercase one for digraphs such as "ǆ", which titlecases to "ǅ".  @param codePoint  Any value. @return The titlecase code point, or codePoint when it has none. @par Thread safety Safe from any thread.

```c
uint32_t muniFoldCase(uint32_t codePoint);
```
Returns the simple case folding of a code point (statuses C and S of CaseFolding.txt).  @param codePoint  Any value. @return The folded code point, or codePoint when it folds to itself. @par Thread safety Safe from any thread.

```c
bool muniIsCased(uint32_t codePoint);
```
Returns whether a code point is Cased (a letter with case, or one such as "ª" that counts as one).  @param codePoint  Any value. @return true when it is Cased. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniTextResult muniConvertCase(const char* text, size_t length, muniCaseOperation operation, muniCaseLanguage language, muniConvertMode mode, char* output, size_t capacity, size_t* neededOut);
```
Applies a full case operation to UTF-8 text.  @param text        The text. May be NULL when length is 0. @param length      The number of bytes. @param operation   muni_caseLower, muni_caseUpper, muni_caseTitle or muni_caseFold. @param language    muni_caseDefault, muni_caseTurkic or muni_caseLithuanian. @param mode        What to do with ill-formed input. @param output      The output. May be NULL when capacity is 0. @param capacity    The number of bytes output can hold. @param neededOut   Receives the number of bytes the whole result needs, which may exceed capacity. On another error, the bytes before it. @return `muni_success` and the length; `muni_errorCapacity` when the result does not fit (the bytes that fit are written); in strict mode the first UTF-8 error and its offset; `muni_errorInvalid` for a NULL argument or an unknown operation, language or mode. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniTextResult muniToNfkcCasefold(const char* text, size_t length, muniConvertMode mode, char* output, size_t capacity, size_t* neededOut);
```
Maps UTF-8 text to NFKC_Casefold: compatibility forms to their plain selves, every case to one, default ignorables removed, and the result in NFC. Two strings match without regard to case or compatibility forms when their NFKC_Casefold texts are equal; UAX #31 uses it to compare identifiers. It needs both the case and the normalization components.  @param text        The text. May be NULL when length is 0. @param length      The number of bytes. @param mode        What to do with ill-formed input. @param output      The output. May be NULL when capacity is 0. @param capacity    The number of bytes output can hold. @param neededOut   Receives the number of bytes the whole result needs, which may exceed capacity. On another error, the bytes before it. @return `muni_success` and the length; `muni_errorCapacity` when the result does not fit (the bytes that fit are written); in strict mode the first UTF-8 error and its offset; `muni_errorLimit` and the offset of the code point that makes a run of combining marks too long, as in muniNormalize; `muni_errorInvalid` for a NULL argument or an unknown mode. @par Thread safety Safe from any thread.

## `encoding.h`

UTF-8, UTF-16 and UTF-32: validation of hostile input, decoding and encoding of single code points, and conversion between the three. A sequence is well formed exactly when the Unicode Standard's table of well-formed UTF-8 byte sequences (chapter 3) says so: no overlong forms, no surrogates, nothing past U+10FFFF.

```c
MUNI_NODISCARD MUNI_API muniTextResult muniValidateUtf8(const char* bytes, size_t length);
```
Validates UTF-8 as hostile input.  @param bytes   The text. May be NULL when length is 0. @param length  The number of bytes. @return `muni_success` and the length, or the first error and the offset of its sequence: `muni_errorUtf8Lead`, `muni_errorUtf8Continuation`, `muni_errorUtf8Truncated`, `muni_errorUtf8Overlong`, `muni_errorUtf8Surrogate`, `muni_errorUtf8TooLarge`, or `muni_errorInvalid` for a NULL pointer with a nonzero length. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniTextResult muniValidateUtf16(const uint16_t* units, size_t length);
```
Validates UTF-16: every surrogate must be half of a pair.  @param units   The text. May be NULL when length is 0. @param length  The number of 16-bit code units. @return `muni_success` and the length, or `muni_errorUtf16Surrogate` and the offset of the unpaired surrogate, or `muni_errorInvalid` for a NULL pointer with a nonzero length. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniResult muniDecodeUtf8(const char* bytes, size_t length, uint32_t* codePointOut, size_t* sizeOut);
```
Decodes the code point at the start of a UTF-8 text.  @param bytes         The text. May be NULL when length is 0. @param length        The number of bytes available. @param codePointOut  Receives the code point, or U+FFFD on an error. @param sizeOut       Receives the bytes consumed: the sequence's length, or on an error the length of its maximal ill-formed subpart (at least 1). @return `muni_success`, one of the UTF-8 errors of muniValidateUtf8, or `muni_errorInvalid` when length is 0 or an out-parameter is NULL. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniResult muniEncodeUtf8(uint32_t codePoint, char* bytesOut, size_t* sizeOut);
```
Encodes one code point as UTF-8.  @param codePoint  A Unicode scalar value. @param bytesOut   Receives 1 to 4 bytes; must hold 4. @param sizeOut    Receives the number of bytes written. @return `muni_success`, or `muni_errorInvalid` for a surrogate, a value past U+10FFFF or a NULL out-parameter. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniTextResult muniConvertUtf8ToUtf16(const char* bytes, size_t length, uint16_t* units, size_t capacity, muniConvertMode mode, size_t* neededOut);
```
Converts UTF-8 to UTF-16.  @param bytes        The text. May be NULL when length is 0. @param length       The number of bytes. @param units        The output. May be NULL when capacity is 0. @param capacity     The number of 16-bit units units can hold. @param mode         What to do with ill-formed input. @param neededOut    Receives the number of units the whole conversion needs, which may exceed capacity. In strict mode, on an error, the units before it. @return `muni_success`; `muni_errorCapacity` when the output does not fit (the units that fit are written); in strict mode the first UTF-8 error and its offset; `muni_errorInvalid` for a NULL pointer with a nonzero length. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniTextResult muniConvertUtf16ToUtf8(const uint16_t* units, size_t length, char* bytes, size_t capacity, muniConvertMode mode, size_t* neededOut);
```
Converts UTF-16 to UTF-8.  @param units        The text. May be NULL when length is 0. @param length       The number of 16-bit units. @param bytes        The output. May be NULL when capacity is 0. @param capacity     The number of bytes bytes can hold. @param mode         What to do with unpaired surrogates. @param neededOut    Receives the number of bytes the whole conversion needs, which may exceed capacity. In strict mode, on an error, the bytes before it. @return `muni_success`; `muni_errorCapacity` when the output does not fit (the bytes that fit are written); in strict mode `muni_errorUtf16Surrogate` and its offset; `muni_errorInvalid` for a NULL pointer with a nonzero length. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniTextResult muniConvertUtf8ToUtf32(const char* bytes, size_t length, uint32_t* codePoints, size_t capacity, muniConvertMode mode, size_t* neededOut);
```
Converts UTF-8 to UTF-32.  @param bytes        The text. May be NULL when length is 0. @param length       The number of bytes. @param codePoints   The output. May be NULL when capacity is 0. @param capacity     The number of code points codePoints can hold. @param mode         What to do with ill-formed input. @param neededOut    Receives the number of code points the whole conversion needs, which may exceed capacity. In strict mode, on an error, the ones before it. @return `muni_success`; `muni_errorCapacity` when the output does not fit (the code points that fit are written); in strict mode the first UTF-8 error and its offset; `muni_errorInvalid` for a NULL pointer with a nonzero length. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniTextResult muniConvertUtf32ToUtf8(const uint32_t* codePoints, size_t length, char* bytes, size_t capacity, muniConvertMode mode, size_t* neededOut);
```
Converts UTF-32 to UTF-8.  @param codePoints   The text. May be NULL when length is 0. @param length       The number of code points. @param bytes        The output. May be NULL when capacity is 0. @param capacity     The number of bytes bytes can hold. @param mode         What to do with values that are no scalar value: surrogates and those past U+10FFFF. @param neededOut    Receives the number of bytes the whole conversion needs, which may exceed capacity. In strict mode, on an error, the bytes before it. @return `muni_success`; `muni_errorCapacity` when the output does not fit (the bytes that fit are written); in strict mode `muni_errorUtf32Value` and its offset; `muni_errorInvalid` for a NULL pointer with a nonzero length. @par Thread safety Safe from any thread.

## `harfbuzz.h`

Maul Unicode as HarfBuzz's source of Unicode data: general category, combining class, mirroring, script, and the composition and decomposition that HarfBuzz's normalizer uses while shaping. Built as the static library maul-unicode-harfbuzz when the build option MAUL_UNICODE_HARFBUZZ is on; it needs normalization. The header does not include <hb.h>: it repeats HarfBuzz's own declaration of hb_unicode_funcs_t, which C and C++ both allow.

```c
extern hb_unicode_funcs_t* muniCreateHarfBuzzFunctions(void);
```
Creates HarfBuzz Unicode functions backed by Maul Unicode. Pass them to hb_buffer_set_unicode_funcs; release them with hb_unicode_funcs_destroy.  @return New immutable functions. Like hb_unicode_funcs_create, it returns HarfBuzz's empty functions when HarfBuzz cannot allocate, never NULL. @par Thread safety Safe from any thread; the functions it returns are immutable and may be shared between threads.

## `identifier.h`

Identifiers and pattern syntax (UAX #31): which characters may start and continue a name in a programming or markup language, a file name or a user name, and which are syntax or white space that a pattern language may give meaning to without ever confusing it with a name.

```c
bool muniIsIdentifierStart(uint32_t codePoint);
```
Returns whether a code point may start an identifier (XID_Start).  @param codePoint  Any value. @return true for XID_Start. @par Thread safety Safe from any thread.

```c
bool muniIsIdentifierContinue(uint32_t codePoint);
```
Returns whether a code point may continue an identifier (XID_Continue, which includes XID_Start).  @param codePoint  Any value. @return true for XID_Continue. @par Thread safety Safe from any thread.

```c
bool muniIsPatternSyntax(uint32_t codePoint);
```
Returns whether a code point is Pattern_Syntax: punctuation and symbols reserved for syntax, never part of an identifier.  @param codePoint  Any value. @return true for Pattern_Syntax. @par Thread safety Safe from any thread.

```c
bool muniIsPatternWhiteSpace(uint32_t codePoint);
```
Returns whether a code point is Pattern_White_Space.  @param codePoint  Any value. @return true for Pattern_White_Space. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniTextResult muniCheckIdentifier(const char* text, size_t length);
```
Checks that UTF-8 text is a default identifier (UAX #31 R1): an XID_Start code point, then XID_Continue code points.  @param text    The text. May be NULL when length is 0. @param length  The number of bytes. @return `muni_success` and the length; `muni_errorIdentifier` and the offset of the first code point that breaks the rule (0 for empty text); the first UTF-8 error and its offset; `muni_errorInvalid` for a NULL text with a nonzero length. @par Thread safety Safe from any thread.

## `normalize.h`

Normalization (UAX #15): the four normal forms, which make text that looks the same compare the same. NFD takes characters apart ("e" and a combining acute accent for "é") and NFC puts them back together; NFKD and NFKC also fold compatibility characters, such as ligatures and full-width letters, into their plain counterparts. Normalization writes into a caller buffer and allocates nothing. It reorders at most 32 code points at once: a character followed by more combining marks than that is refused with muni_errorLimit rather than buffered without bound (real text carries a handful; UAX #15's stream-safe text carries at most 30).

```c
MUNI_NODISCARD MUNI_API muniTextResult muniNormalize(const char* text, size_t length, muniNormalForm form, muniConvertMode mode, char* output, size_t capacity, size_t* neededOut);
```
Normalizes UTF-8 text into a normal form.  @param text        The text. May be NULL when length is 0. @param length      The number of bytes. @param form        muni_nfc, muni_nfd, muni_nfkc or muni_nfkd. @param mode        What to do with ill-formed input. @param output      The output. May be NULL when capacity is 0. @param capacity    The number of bytes output can hold. @param neededOut   Receives the number of bytes the whole result needs, which may exceed capacity. On another error, the bytes before it. @return `muni_success` and the length; `muni_errorCapacity` when the result does not fit (the bytes that fit are written); in strict mode the first UTF-8 error and its offset; `muni_errorLimit` and the offset of the code point that makes a run of combining marks too long; `muni_errorInvalid` for a NULL argument or an unknown form. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniTextResult muniCheckNormalization(const char* text, size_t length, muniNormalForm form, muniQuickCheck* answerOut);
```
Tells quickly whether UTF-8 text is in a normal form, without normalizing it.  @param text       The text. May be NULL when length is 0. @param length     The number of bytes. @param form       muni_nfc, muni_nfd, muni_nfkc or muni_nfkd. @param answerOut  Receives muni_quickCheckYes, muni_quickCheckNo or muni_quickCheckMaybe. @return `muni_success` and the length; the first UTF-8 error and its offset; `muni_errorInvalid` for a NULL argument or an unknown form. @par Thread safety Safe from any thread.

```c
bool muniDecomposePair(uint32_t codePoint, uint32_t* firstOut, uint32_t* secondOut);
```
Returns the canonical decomposition of a code point one level deep, as shapers such as HarfBuzz ask for it: two code points, or one with the second 0. Hangul syllables split into LV + T or L + V.  @param codePoint  Any value. @param firstOut   Receives the first code point. @param secondOut  Receives the second code point, or 0. @return true when the code point decomposes; the outputs are left alone when it does not, or when either is NULL. @par Thread safety Safe from any thread.

```c
uint32_t muniComposePair(uint32_t first, uint32_t second);
```
Returns the primary composite of two code points (UAX #15 D114), Hangul syllables included.  @param first   The first code point. @param second  The second code point. @return The composite, or 0 when the pair does not compose. @par Thread safety Safe from any thread.

## `properties.h`

Character properties from the Unicode Character Database. Every lookup takes a code point, cannot fail and returns the property's value; a value above U+10FFFF gets the value of an unassigned code point.

```c
muniGeneralCategory muniGetGeneralCategory(uint32_t codePoint);
```
Returns the General_Category of a code point.  @param codePoint  Any value; one above U+10FFFF is unassigned. @return The category, one of the muni_gc values. @par Thread safety Safe from any thread.

```c
muniGraphemeBreak muniGetGraphemeBreak(uint32_t codePoint);
```
Returns the Grapheme_Cluster_Break of a code point.  @param codePoint  Any value; one above U+10FFFF is Other. @return The value, one of the muni_gcb values. @par Thread safety Safe from any thread.

```c
muniWordBreak muniGetWordBreak(uint32_t codePoint);
```
Returns the Word_Break of a code point.  @param codePoint  Any value; one above U+10FFFF is Other. @return The value, one of the muni_wb values. @par Thread safety Safe from any thread.

```c
muniSentenceBreak muniGetSentenceBreak(uint32_t codePoint);
```
Returns the Sentence_Break of a code point.  @param codePoint  Any value; one above U+10FFFF is Other. @return The value, one of the muni_sb values. @par Thread safety Safe from any thread.

```c
muniIndicConjunctBreak muniGetIndicConjunctBreak(uint32_t codePoint);
```
Returns the Indic_Conjunct_Break of a code point.  @param codePoint  Any value; one above U+10FFFF is None. @return The value, one of the muni_incb values. @par Thread safety Safe from any thread.

```c
bool muniIsExtendedPictographic(uint32_t codePoint);
```
Tells whether a code point has the Extended_Pictographic property (UTS #51), which keeps emoji sequences together.  @param codePoint  Any value; one above U+10FFFF has it not. @return true when the code point is Extended_Pictographic. @par Thread safety Safe from any thread.

```c
bool muniIsEmoji(uint32_t codePoint);
```
Tells whether a code point has the Emoji property (UTS #51): it can show as an emoji, which digits and '#' can too.  @param codePoint  Any value; one above U+10FFFF has it not. @return true when the code point is Emoji. @par Thread safety Safe from any thread.

```c
bool muniIsEmojiPresentation(uint32_t codePoint);
```
Tells whether a code point has the Emoji_Presentation property (UTS #51): it shows as an emoji unless a variation selector asks for text.  @param codePoint  Any value; one above U+10FFFF has it not. @return true when the code point is Emoji_Presentation. @par Thread safety Safe from any thread.

```c
bool muniIsEmojiModifier(uint32_t codePoint);
```
Tells whether a code point has the Emoji_Modifier property (UTS #51): a skin tone modifier.  @param codePoint  Any value; one above U+10FFFF has it not. @return true when the code point is Emoji_Modifier. @par Thread safety Safe from any thread.

```c
bool muniIsEmojiModifierBase(uint32_t codePoint);
```
Tells whether a code point has the Emoji_Modifier_Base property (UTS #51): a skin tone modifier after it applies to it.  @param codePoint  Any value; one above U+10FFFF has it not. @return true when the code point is Emoji_Modifier_Base. @par Thread safety Safe from any thread.

```c
bool muniIsEmojiComponent(uint32_t codePoint);
```
Tells whether a code point has the Emoji_Component property (UTS #51): it can be part of an emoji sequence, as regional indicators, keycap parts, tags and the zero width joiner are.  @param codePoint  Any value; one above U+10FFFF has it not. @return true when the code point is Emoji_Component. @par Thread safety Safe from any thread.

```c
bool muniIsWhiteSpace(uint32_t codePoint);
```
Tells whether a code point has the White_Space property: spaces, tabs and line and paragraph separators, 25 code points in all.  @param codePoint  Any value; one above U+10FFFF has it not. @return true when the code point is White_Space. @par Thread safety Safe from any thread.

```c
bool muniIsDefaultIgnorable(uint32_t codePoint);
```
Tells whether a code point is Default_Ignorable_Code_Point: a character that shows nothing when a font lacks it, such as a zero width joiner or a variation selector.  @param codePoint  Any value; one above U+10FFFF has it not. @return true for a default ignorable code point. @par Thread safety Safe from any thread.

```c
int32_t muniGetDecimalDigitValue(uint32_t codePoint);
```
Returns the value of a decimal digit: a code point of General Category Nd (Numeric_Type Decimal), such as '7' or the Devanagari seven, which parse as numbers. Superscripts, fractions and Roman numerals have numeric values but are not decimal digits.  @param codePoint  Any value. @return The digit's value, 0 to 9, or -1 when the code point is not a decimal digit. @par Thread safety Safe from any thread.

```c
muniLineBreak muniGetLineBreak(uint32_t codePoint);
```
Returns the Line_Break class of a code point.  @param codePoint  Any value; one above U+10FFFF is Unknown (XX). @return The class, one of the muni_lb values. @par Thread safety Safe from any thread.

```c
muniEastAsianWidth muniGetEastAsianWidth(uint32_t codePoint);
```
Returns the East_Asian_Width of a code point.  @param codePoint  Any value; one above U+10FFFF is Neutral. @return The width, one of the muni_eaw values. @par Thread safety Safe from any thread.

```c
muniBidiClass muniGetBidiClass(uint32_t codePoint);
```
Returns the Bidi_Class of a code point, with the UCD's defaults for unassigned code points in right-to-left blocks.  @param codePoint  Any value; one above U+10FFFF is Left_To_Right. @return The class, one of the muni_bc values. @par Thread safety Safe from any thread.

```c
uint32_t muniGetMirroringGlyph(uint32_t codePoint);
```
Returns the Bidi_Mirroring_Glyph of a code point: the character whose glyph is its mirror image, such as ')' for '(', which right- to-left text displays in its place.  @param codePoint  Any value. @return The mirror, or codePoint itself when it has none. @par Thread safety Safe from any thread.

```c
muniBracketType muniGetBracketType(uint32_t codePoint);
```
Returns the Bidi_Paired_Bracket_Type of a code point. The bracket it pairs with is its muniGetMirroringGlyph.  @param codePoint  Any value. @return One of the muni_bracket values. @par Thread safety Safe from any thread.

```c
uint8_t muniGetCombiningClass(uint32_t codePoint);
```
Returns the Canonical_Combining_Class of a code point.  @param codePoint  Any value; one above U+10FFFF has class 0. @return The class, from 0 (not reordered) to 254. @par Thread safety Safe from any thread.

```c
muniScript muniGetScript(uint32_t codePoint);
```
Returns the Script of a code point.  @param codePoint  Any value; one above U+10FFFF is Unknown. @return The script's ISO 15924 tag, as MUNI_SCRIPT builds it. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniResult muniGetScriptExtensions(uint32_t codePoint, muniScript* scripts, size_t capacity, size_t* countOut);
```
Writes the Script_Extensions of a code point (UAX #24): the scripts it is used with. A code point without listed extensions has its Script alone, so there is always at least one.  @param codePoint  Any value. @param scripts    The output. May be NULL when capacity is 0. @param capacity   The number of scripts the output can hold; 32 always suffice. @param countOut   Receives the number of scripts. @return `muni_success`, `muni_errorCapacity` when they do not all fit (the ones that fit are written), or `muni_errorInvalid` for a NULL argument. @par Thread safety Safe from any thread.

## `script.h`

Script runs: the stretches of text a shaper such as HarfBuzz shapes with one script. Characters used with many scripts, such as spaces, digits and punctuation (Script Common), and combining marks (Inherited) join the run around them; a character used with a few scripts (Script_Extensions, UAX #24) narrows the run to those it shares; a closing bracket takes the script of its opening bracket. Leading Common characters take the script of the first run. Like the segment iterators, a script iterator is a fixed-size struct the caller keeps, takes text in pieces cut anywhere, and asks for the next piece with muni_needMoreText.

```c
MUNI_NODISCARD MUNI_API muniResult muniInitScriptIterator(muniScriptIterator* iterator, const char* text, size_t length, bool moreFollows);
```
Starts an iterator over the script runs of a UTF-8 text or its first piece.  @param iterator     The iterator to initialize. @param text         The text. May be NULL when length is 0. @param length       The number of bytes. @param moreFollows  true when more pieces of the text will be fed. @return `muni_success`, or `muni_errorInvalid` for a NULL iterator or a NULL text with a nonzero length. @par Thread safety Safe from any thread; an iterator is used by one thread at a time.

```c
MUNI_NODISCARD MUNI_API muniResult muniFeedScriptIterator(muniScriptIterator* iterator, const char* text, size_t length, bool moreFollows);
```
Hands a script iterator the next piece of its text, after it returned muni_needMoreText.  @param iterator     The iterator. @param text         The next piece. May be NULL when length is 0. @param length       The number of bytes. @param moreFollows  true when still more pieces will be fed. @return `muni_success`, or `muni_errorInvalid` for a NULL argument, an iterator that did not ask for more text, or one initialized without more text to follow. @par Thread safety Safe from any thread; an iterator is used by one thread at a time.

```c
MUNI_NODISCARD MUNI_API muniResult muniNextScriptRun(muniScriptIterator* iterator, muniScriptRun* runOut);
```
Finds the next script run.  @param iterator  The iterator. @param runOut    Receives the run: where it ends, as a byte offset from the start of the whole text, and its script. A text of Common and Inherited characters only is one run of Common. @return `muni_success` with a run; `muni_done` after the last one; `muni_needMoreText` when the run's end or script depends on text not yet fed; `muni_errorInvalid` for a NULL argument. @par Thread safety Safe from any thread; an iterator is used by one thread at a time.

```c
MUNI_NODISCARD MUNI_API muniResult muniFindScriptRuns(const char* text, size_t length, muniScriptRun* runs, size_t capacity, size_t* countOut);
```
Writes the script runs of a whole UTF-8 text into a caller array.  @param text      The text. May be NULL when length is 0. @param length    The number of bytes. @param runs      The output. May be NULL when capacity is 0. @param capacity  The number of runs the output can hold. @param countOut  Receives the number of runs, which may exceed capacity; the ones that fit are written. @return `muni_success`, `muni_errorCapacity` when they do not all fit, or `muni_errorInvalid` for a NULL argument. @par Thread safety Safe from any thread.

## `security.h`

Security mechanisms (UTS #39) for names people read, such as user names and identifiers: whether two strings can pass for each other, and how far a string mixes scripts. Two strings are confusable when their skeletons are equal: "paypal" written with a Cyrillic "а" has the skeleton of the Latin one. A skeleton is not text to show; store it next to a name, compare it as bytes, and recompute it when the library's Unicode version changes. Skeletons follow the reading order bidi gives each string, so the caller supplies bidi's workspace, and nothing allocates. The restriction level grades how a string mixes scripts, from ASCII only to anything, under the General Security Profile: a string with a character the profile does not allow is unrestricted. The level does not check the string's syntax; an identifier should also pass muniCheckIdentifier.

```c
bool muniIsIdentifierAllowed(uint32_t codePoint);
```
Tells whether a code point has Identifier_Status Allowed, the General Security Profile for identifiers.  @param codePoint  Any value. @return true when the profile allows the code point. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniTextResult muniGetSkeleton(const char* text, size_t length, muniBidiDirection direction, uint8_t* workspace, char* output, size_t capacity, size_t* neededOut);
```
Computes the skeleton of UTF-8 text as displayed in a paragraph of a direction: bidiSkeleton of UTS #39 section 4. The skeleton of muni_bidiLeftToRight is the one UTS #39 calls skeleton; muni_bidiAuto takes the direction from the first strong character. Text without right-to-left characters has the same skeleton in both automatic and left-to-right paragraphs.  @param text        The text. May be NULL when length is 0. @param length      The number of bytes. @param direction   The paragraph direction. @param workspace   Scratch memory of 2 * length bytes. @param output      The output. May be NULL when capacity is 0. @param capacity    The number of bytes output can hold. @param neededOut   Receives the number of bytes the whole skeleton needs, which may exceed capacity. @return `muni_success` and the length; `muni_errorCapacity` when the skeleton does not fit (the bytes that fit are written); the first UTF-8 error and its offset; `muni_errorLimit` when a run of combining marks outgrows 32, and the offset of the code point being read then; `muni_errorInvalid` for a NULL argument or an unknown direction. @par Thread safety Safe from any thread on memory no other thread uses.

```c
MUNI_NODISCARD MUNI_API muniTextResult muniGetResolvedScripts(const char* text, size_t length, muniScript* scripts, size_t capacity, size_t* countOut);
```
Writes the resolved script set of UTF-8 text (UTS #39 section 5.1): the scripts every character can belong to, counting Han text as also Japanese (Jpan), Korean (Kore) and Han with Bopomofo (Hanb) or Latin (Hntl), Hiragana and Katakana as Japanese, Hangul as Korean, Bopomofo as Hanb and Latin as Hntl. No script means the text mixes scripts. Text of Common and Inherited characters alone, which fits any script, gives the single script Zyyy.  @param text      The text. May be NULL when length is 0. @param length    The number of bytes. @param scripts   The output, in ascending tag order. May be NULL when capacity is 0. @param capacity  The number of scripts the output can hold; 64 is always enough. @param countOut  Receives the number of scripts. @return `muni_success` and the length; `muni_errorCapacity` when the scripts do not fit, and then none is written; the first UTF-8 error and its offset; `muni_errorInvalid` for a NULL argument. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniTextResult muniGetRestrictionLevel(const char* text, size_t length, muniRestrictionLevel* levelOut);
```
Grades how UTF-8 text mixes scripts (UTS #39 section 5.2).  @param text     The text. May be NULL when length is 0. @param length   The number of bytes. @param levelOut Receives one of the muni_restriction values. @return `muni_success` and the length; the first UTF-8 error and its offset; `muni_errorInvalid` for a NULL argument. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniTextResult muniCheckMixedNumbers(const char* text, size_t length, bool* mixedOut);
```
Tells whether UTF-8 text holds digits of more than one decimal system (UTS #39 section 5.3), such as ASCII and Bengali digits, some of which look alike.  @param text     The text. May be NULL when length is 0. @param length   The number of bytes. @param mixedOut Receives true when the digits mix systems. @return `muni_success` and the length; the first UTF-8 error and its offset; `muni_errorInvalid` for a NULL argument. @par Thread safety Safe from any thread.

## `segment.h`

Text segmentation (UAX #29): the boundaries of grapheme clusters, the units a user perceives as one character, which caret movement, selection and backspace must never split; of words, for double-click selection, word movement and search; and of sentences. Line breaking (UAX #14): the positions where a line may, or must, end. An iterator walks UTF-8 text and reports each boundary after the start of the text, the last one being the end of the text; empty text has none. Offsets are in bytes from the start of the whole text. Each maximal ill-formed UTF-8 subpart counts as one U+FFFD. Word boundaries are the positions UAX #29 defines; between two of them may lie a word, a run of spaces or a punctuation mark, which the caller tells apart. Text may arrive in pieces. An iterator initialized with more text to follow stops with muni_needMoreText when the next boundary depends on text it has not seen; muniFeedSegmentIterator hands it the next piece, which may begin in the middle of a UTF-8 sequence. Some rules look ahead: a word boundary may wait for the next code point, and a sentence boundary after an abbreviation's period for the next letter.

```c
MUNI_NODISCARD MUNI_API muniResult muniInitGraphemeIterator(muniSegmentIterator* iterator, const char* text, size_t length, bool moreFollows);
```
Starts an iterator over the grapheme cluster boundaries of a UTF-8 text or its first piece.  @param iterator     The iterator to initialize. @param text         The text. May be NULL when length is 0. @param length       The number of bytes. @param moreFollows  true when more pieces of the text will be fed. @return `muni_success`, or `muni_errorInvalid` for a NULL iterator or a NULL text with a nonzero length. @par Thread safety Safe from any thread; an iterator is used by one thread at a time.

```c
MUNI_NODISCARD MUNI_API muniResult muniInitWordIterator(muniSegmentIterator* iterator, const char* text, size_t length, bool moreFollows);
```
Starts an iterator over the word boundaries of a UTF-8 text or its first piece.  @param iterator     The iterator to initialize. @param text         The text. May be NULL when length is 0. @param length       The number of bytes. @param moreFollows  true when more pieces of the text will be fed. @return `muni_success`, or `muni_errorInvalid` for a NULL iterator or a NULL text with a nonzero length. @par Thread safety Safe from any thread; an iterator is used by one thread at a time.

```c
MUNI_NODISCARD MUNI_API muniResult muniInitSentenceIterator(muniSegmentIterator* iterator, const char* text, size_t length, bool moreFollows);
```
Starts an iterator over the sentence boundaries of a UTF-8 text or its first piece.  @param iterator     The iterator to initialize. @param text         The text. May be NULL when length is 0. @param length       The number of bytes. @param moreFollows  true when more pieces of the text will be fed. @return `muni_success`, or `muni_errorInvalid` for a NULL iterator or a NULL text with a nonzero length. @par Thread safety Safe from any thread; an iterator is used by one thread at a time.

```c
MUNI_NODISCARD MUNI_API muniResult muniInitLineIterator(muniSegmentIterator* iterator, const char* text, size_t length, bool moreFollows);
```
Starts an iterator over the line break opportunities of a UTF-8 text or its first piece, with the default rules of UAX #14. Thai, Lao, Khmer and Burmese letters (class SA) are resolved as rule LB1 says without a dictionary, with no opportunities between them, unless muniSetComplexBreaker supplies a word segmenter.  @param iterator     The iterator to initialize. @param text         The text. May be NULL when length is 0. @param length       The number of bytes. @param moreFollows  true when more pieces of the text will be fed. @return `muni_success`, or `muni_errorInvalid` for a NULL iterator or a NULL text with a nonzero length. @par Thread safety Safe from any thread; an iterator is used by one thread at a time.

```c
MUNI_NODISCARD MUNI_API muniResult muniSetComplexBreaker(muniSegmentIterator* iterator, muniComplexBreakFn breaker, void* context);
```
Sets the word segmenter a line or word iterator hands runs of SA text to. Inside such a run its breaks replace the default ones: line breaking then allows a break between words, and word segmentation finds whole words instead of single letters. A break before a combining mark is ignored. A run lies within one piece of text; a piece boundary inside a run hands the run over in parts. Without a segmenter, SA text follows rule LB1 of UAX #14 and the default word rules.  @param iterator  A line or word iterator not yet asked for a break. @param breaker   The segmenter, or NULL for the default rules. @param context   Passed to the segmenter unchanged. @return `muni_success`, or `muni_errorInvalid` for a NULL iterator, another kind of iterator or one already started. @par Thread safety Safe from any thread; an iterator is used by one thread at a time, and calls the segmenter on that thread.

```c
MUNI_NODISCARD MUNI_API muniResult muniFeedSegmentIterator(muniSegmentIterator* iterator, const char* text, size_t length, bool moreFollows);
```
Hands an iterator the next piece of its text, after it returned muni_needMoreText. The iterator keeps no pointer to earlier pieces.  @param iterator     The iterator. @param text         The next piece. May be NULL when length is 0. @param length       The number of bytes. @param moreFollows  true when still more pieces will be fed. @return `muni_success`, or `muni_errorInvalid` for a NULL argument, an iterator never initialized, one initialized without more text to follow, or a piece fed before the iterator asked. @par Thread safety Safe from any thread; an iterator is used by one thread at a time.

```c
MUNI_NODISCARD MUNI_API muniResult muniNextSegmentBreak(muniSegmentIterator* iterator, size_t* offsetOut);
```
Finds the next boundary.  @param iterator   The iterator. @param offsetOut  Receives the boundary's byte offset from the start of the whole text. @return `muni_success` with a boundary; `muni_done` after the last one; `muni_needMoreText` when the next boundary depends on text not yet fed; `muni_errorInvalid` for a NULL argument. @par Thread safety Safe from any thread; an iterator is used by one thread at a time.

```c
MUNI_NODISCARD MUNI_API muniResult muniNextLineBreak(muniSegmentIterator* iterator, size_t* offsetOut, bool* mandatoryOut);
```
Finds the next line break opportunity of a line iterator, and whether the line must end there: after a mandatory break character (BK, CR, LF or NL) and at the end of the text.  @param iterator      An iterator from muniInitLineIterator. @param offsetOut     Receives the opportunity's byte offset from the start of the whole text. @param mandatoryOut  Receives true when the break is mandatory. @return As muniNextSegmentBreak; `muni_errorInvalid` also for an iterator of another kind. @par Thread safety Safe from any thread; an iterator is used by one thread at a time.

```c
MUNI_NODISCARD MUNI_API muniResult muniFindGraphemeBreaks(const char* text, size_t length, size_t* offsets, size_t capacity, size_t* countOut);
```
Writes the grapheme cluster boundaries of a whole UTF-8 text into a caller array: every boundary after the start, the end included.  @param text         The text. May be NULL when length is 0. @param length       The number of bytes. @param offsets      The output. May be NULL when capacity is 0. @param capacity     The number of offsets the output can hold. @param countOut     Receives the number of boundaries, which may exceed capacity; the ones that fit are written. @return `muni_success`, `muni_errorCapacity` when they do not all fit, or `muni_errorInvalid` for a NULL argument. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniResult muniFindWordBreaks(const char* text, size_t length, size_t* offsets, size_t capacity, size_t* countOut);
```
Writes the word boundaries of a whole UTF-8 text into a caller array: every boundary after the start, the end included.  @param text         The text. May be NULL when length is 0. @param length       The number of bytes. @param offsets      The output. May be NULL when capacity is 0. @param capacity     The number of offsets the output can hold. @param countOut     Receives the number of boundaries, which may exceed capacity; the ones that fit are written. @return `muni_success`, `muni_errorCapacity` when they do not all fit, or `muni_errorInvalid` for a NULL argument. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniResult muniFindSentenceBreaks(const char* text, size_t length, size_t* offsets, size_t capacity, size_t* countOut);
```
Writes the sentence boundaries of a whole UTF-8 text into a caller array: every boundary after the start, the end included.  @param text         The text. May be NULL when length is 0. @param length       The number of bytes. @param offsets      The output. May be NULL when capacity is 0. @param capacity     The number of offsets the output can hold. @param countOut     Receives the number of boundaries, which may exceed capacity; the ones that fit are written. @return `muni_success`, `muni_errorCapacity` when they do not all fit, or `muni_errorInvalid` for a NULL argument. @par Thread safety Safe from any thread.

```c
MUNI_NODISCARD MUNI_API muniResult muniFindLineBreaks(const char* text, size_t length, size_t* offsets, bool* mandatory, size_t capacity, size_t* countOut);
```
Writes the line break opportunities of a whole UTF-8 text into caller arrays: every opportunity after the start, the end included, and optionally whether each is mandatory.  @param text         The text. May be NULL when length is 0. @param length       The number of bytes. @param offsets      The output. May be NULL when capacity is 0. @param mandatory    NULL, or an array of capacity flags that receive true for each mandatory break. @param capacity     The number of entries the outputs can hold. @param countOut     Receives the number of opportunities, which may exceed capacity; the ones that fit are written. @return `muni_success`, `muni_errorCapacity` when they do not all fit, or `muni_errorInvalid` for a NULL argument. @par Thread safety Safe from any thread.

---

75 functions across 11 headers.
