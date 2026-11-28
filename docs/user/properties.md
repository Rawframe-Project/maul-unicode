# Character properties

`properties.h` answers questions about single code points. Each lookup is a few reads of a compressed table and cannot fail; the function returns the value directly.

| Property | Function |
|---|---|
| General_Category | `muniGetGeneralCategory` |
| Grapheme_Cluster_Break, Word_Break, Sentence_Break, Indic_Conjunct_Break | `muniGetGraphemeBreak`, `muniGetWordBreak`, `muniGetSentenceBreak`, `muniGetIndicConjunctBreak` |
| Line_Break, East_Asian_Width | `muniGetLineBreak`, `muniGetEastAsianWidth` |
| Bidi_Class, Bidi_Mirroring_Glyph, Bidi_Paired_Bracket_Type | `muniGetBidiClass`, `muniGetMirroringGlyph`, `muniGetBracketType` |
| Canonical_Combining_Class | `muniGetCombiningClass` |
| Script, Script_Extensions | `muniGetScript`, `muniGetScriptExtensions` |
| Emoji properties | `muniIsExtendedPictographic`, `muniIsEmoji`, `muniIsEmojiPresentation`, `muniIsEmojiModifier`, `muniIsEmojiModifierBase`, `muniIsEmojiComponent` |
| White_Space, Default_Ignorable_Code_Point | `muniIsWhiteSpace`, `muniIsDefaultIgnorable` |
| Ideographic | `muniIsIdeographic` |
| Decimal digit value | `muniGetDecimalDigitValue` |

Identifier properties are in `identifier.h` and case properties in `case.h`; see [Identifiers and security](security.md) and [Normalization and case](normalization-and-case.md).

## Emoji sequences

What a user sees as one emoji is often several code points: a flag is two regional indicators, a keycap three code points, a family emoji people joined by U+200D. `muniGetEmojiKind` tells whether a text is exactly one emoji of the set UTS #51 recommends for general interchange (RGI), the set fonts and keyboards support, and of which kind: basic, keycap, flag, skin tone modifier, tag sequence (the flags of England, Scotland and Wales) or ZWJ sequence. Give it one grapheme cluster. To tell whether a chat message is emoji alone, for example to show it larger, check each cluster:

```c
bool onlyEmoji = length > 0;
muniSegmentIterator clusters;
(void)muniInitGraphemeIterator(&clusters, text, length, false);
size_t start = 0;
size_t end = 0;
while (onlyEmoji && muniSegmentIterator_NextBreak(&clusters, &end) == muni_success)
{
    muniEmojiKind kind = muni_emojiKindNone;
    onlyEmoji = muniGetEmojiKind(text + start, end - start, &kind) == muni_success &&
                kind != muni_emojiKindNone;
    start = end;
}
```

Only the fully qualified forms the files list count. "☺" (U+263A) is text by default and an emoji with U+FE0F after it; a watch "⌚" (U+231A) is an emoji alone and not with U+FE0F. Two regional indicators that name no region, a skin tone on an emoji that takes none, and emoji joined by U+200D into a sequence the files do not list are no RGI emoji, though a font may still draw them.

## Not obvious

- Every function accepts any `uint32_t`. Values past U+10FFFF answer as unassigned code points do, so a lookup never needs a range check first.
- The enumeration values (`muni_gcLu`, `muni_lbSa`, `muni_bcRli`) are the library's own numbering. A value that a new Unicode version adds, as Unicode 15.1 added five Line_Break classes, is appended after the last, and the existing values keep their numbers. That is part of the API, so data may store the numbers.
- Scripts are ISO 15924 tags packed into a `uint32_t` by `MUNI_MAKE_SCRIPT('L', 'a', 't', 'n')`, the same values as HarfBuzz's `hb_script_t`. Unassigned code points are `MUNI_SCRIPT_UNKNOWN`; characters many scripts share are `MUNI_SCRIPT_COMMON`; combining marks that take their base's script are `MUNI_SCRIPT_INHERITED`.
- `muniGetScriptExtensions` always writes at least one script: a code point without listed extensions has its Script alone. An array of `MUNI_MAX_SCRIPT_EXTENSIONS` entries always suffices for this Unicode version; a release with a new one may raise it, and the count the call reports is always exact.
- Bidi_Class gives unassigned code points in right-to-left blocks the UCD's defaults (R or AL), as the bidi algorithm requires.
- Ideographic is not Script=Han. Han includes the CJK radicals (U+2E80 to U+2FD5), which are not Ideographic, and leaves out Tangut, Khitan Small Script, Jurchen, Nushu and Small Seal (Script=Seal), which are. A word rule over ideographs, such as CLDR's Japanese one, wants Ideographic.
- The per-code-point emoji properties cannot tell an RGI sequence from any emoji joined to another; `muniGetEmojiKind` can.
- Decimal digit values cover Numeric_Type Decimal only (General_Category Nd). Superscripts, fractions and Roman numerals have no value here.
