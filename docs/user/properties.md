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

## Not obvious

- Every function accepts any `uint32_t`. Values past U+10FFFF answer as unassigned code points do, so a lookup never needs a range check first.
- The enumeration values (`muni_gcLu`, `muni_lbSa`, `muni_bcRli`) are the library's own numbering, stable within a major version. Store the UCD's short names, not the numbers, in data that outlives a version.
- Scripts are ISO 15924 tags packed into a `uint32_t` by `MUNI_SCRIPT('L', 'a', 't', 'n')`, the same values as HarfBuzz's `hb_script_t`. Unassigned code points are `MUNI_SCRIPT_UNKNOWN`; characters many scripts share are `MUNI_SCRIPT_COMMON`; combining marks that take their base's script are `MUNI_SCRIPT_INHERITED`.
- `muniGetScriptExtensions` always writes at least one script: a code point without listed extensions has its Script alone. 32 entries always suffice.
- Bidi_Class gives unassigned code points in right-to-left blocks the UCD's defaults (R or AL), as the bidi algorithm requires.
- Ideographic is not Script=Han. Han includes the CJK radicals (U+2E80 to U+2FD5), which are not Ideographic, and leaves out Tangut, Khitan Small Script, Jurchen and Nushu, which are. A word rule over ideographs, such as CLDR's Japanese one, wants Ideographic.
- Decimal digit values cover Numeric_Type Decimal only (General_Category Nd). Superscripts, fractions and Roman numerals have no value here.
