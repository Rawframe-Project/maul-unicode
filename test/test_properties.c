// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Property lookups: every code point through the public API against the
// hash the generator computed from the UCD, and known values by hand.

#include "tables.h"
#include "test_harness.h"

#include "maul-unicode/properties.h"

static uint64_t HashAll(uint8_t (*lookup)(uint32_t))
{
    uint64_t hash = 0xcbf29ce484222325ull;
    for (uint32_t c = 0; c < 0x110000u; c++)
    {
        hash ^= lookup(c);
        hash *= 0x100000001b3ull;
    }
    return hash;
}

static uint8_t GeneralCategory(uint32_t codePoint)
{
    return muniGetGeneralCategory(codePoint);
}

static void TestGeneralCategoryMatchesTheUcdEverywhere(void)
{
    CHECK(HashAll(GeneralCategory) == muniGeneralCategoryHash, "hash over every code point");
}

static void TestGeneralCategoryKnownValues(void)
{
    CHECK(muniGetGeneralCategory('A') == muni_gcLu, "A is Lu");
    CHECK(muniGetGeneralCategory('a') == muni_gcLl, "a is Ll");
    CHECK(muniGetGeneralCategory('0') == muni_gcNd, "0 is Nd");
    CHECK(muniGetGeneralCategory(' ') == muni_gcZs, "space is Zs");
    CHECK(muniGetGeneralCategory(0x0378) == muni_gcCn, "U+0378 is unassigned");
    CHECK(muniGetGeneralCategory(0xD800) == muni_gcCs, "U+D800 is a surrogate");
    CHECK(muniGetGeneralCategory(0xE000) == muni_gcCo, "U+E000 is private use");
    CHECK(muniGetGeneralCategory(0x4E00) == muni_gcLo, "U+4E00, a CJK range, is Lo");
    CHECK(muniGetGeneralCategory(0x1F600) == muni_gcSo, "U+1F600 is So");
    CHECK(muniGetGeneralCategory(0x10FFFD) == muni_gcCo, "U+10FFFD is private use");
    CHECK(muniGetGeneralCategory(0x10FFFF) == muni_gcCn, "U+10FFFF is a noncharacter");
    CHECK(muniGetGeneralCategory(0x110000) == muni_gcCn, "past U+10FFFF is unassigned");
    CHECK(muniGetGeneralCategory(UINT32_MAX) == muni_gcCn, "UINT32_MAX is unassigned");
}

static uint8_t GraphemeBreak(uint32_t codePoint)
{
    return muniGetGraphemeBreak(codePoint);
}

static uint8_t WordBreak(uint32_t codePoint)
{
    return muniGetWordBreak(codePoint);
}

static uint8_t SentenceBreak(uint32_t codePoint)
{
    return muniGetSentenceBreak(codePoint);
}

static uint8_t IndicConjunctBreak(uint32_t codePoint)
{
    return muniGetIndicConjunctBreak(codePoint);
}

static uint8_t ExtendedPictographic(uint32_t codePoint)
{
    return muniIsExtendedPictographic(codePoint) ? 1 : 0;
}

static void TestBreakPropertiesMatchTheUcdEverywhere(void)
{
    CHECK(HashAll(GraphemeBreak) == muniGraphemeClusterBreakHash, "Grapheme_Cluster_Break hash");
    CHECK(HashAll(WordBreak) == muniWordBreakHash, "Word_Break hash");
    CHECK(HashAll(SentenceBreak) == muniSentenceBreakHash, "Sentence_Break hash");
    CHECK(HashAll(IndicConjunctBreak) == muniIndicConjunctBreakHash, "Indic_Conjunct_Break hash");
    CHECK(HashAll(ExtendedPictographic) == muniExtendedPictographicHash,
          "Extended_Pictographic hash");
}

static void TestBreakPropertiesKnownValues(void)
{
    CHECK(muniGetGraphemeBreak('\r') == muni_gcbCr, "CR");
    CHECK(muniGetGraphemeBreak(0x200D) == muni_gcbZwj, "U+200D is ZWJ");
    CHECK(muniGetGraphemeBreak(0x1F1E6) == muni_gcbRegionalIndicator, "U+1F1E6 is RI");
    CHECK(muniGetGraphemeBreak(0xAC00) == muni_gcbLv, "U+AC00 is LV");
    CHECK(muniGetGraphemeBreak(0x0301) == muni_gcbExtend, "U+0301 is Extend");
    CHECK(muniGetGraphemeBreak('a') == muni_gcbOther, "a is Other");
    CHECK(muniGetWordBreak('a') == muni_wbALetter, "a is ALetter");
    CHECK(muniGetWordBreak('\'') == muni_wbSingleQuote, "apostrophe is Single_Quote");
    CHECK(muniGetWordBreak(0x05D0) == muni_wbHebrewLetter, "U+05D0 is Hebrew_Letter");
    CHECK(muniGetWordBreak(0x30A2) == muni_wbKatakana, "U+30A2 is Katakana");
    CHECK(muniGetSentenceBreak('.') == muni_sbATerm, "full stop is ATerm");
    CHECK(muniGetSentenceBreak('?') == muni_sbSTerm, "question mark is STerm");
    CHECK(muniGetSentenceBreak('A') == muni_sbUpper, "A is Upper");
    CHECK(muniGetIndicConjunctBreak(0x094D) == muni_incbLinker, "U+094D is a linker");
    CHECK(muniGetIndicConjunctBreak(0x0915) == muni_incbConsonant, "U+0915 is a consonant");
    CHECK(muniIsExtendedPictographic(0x1F600), "U+1F600 is Extended_Pictographic");
    CHECK(!muniIsExtendedPictographic('a'), "a is not Extended_Pictographic");
    CHECK(muniGetGraphemeBreak(0x110000) == muni_gcbOther, "past U+10FFFF is Other");
}

static uint8_t LineBreak(uint32_t codePoint)
{
    return muniGetLineBreak(codePoint);
}

static uint8_t EastAsianWidth(uint32_t codePoint)
{
    return muniGetEastAsianWidth(codePoint);
}

static uint8_t BidiClass(uint32_t codePoint)
{
    return muniGetBidiClass(codePoint);
}

static uint8_t MirrorIndex(uint32_t codePoint)
{
    return muniLookupBidiMirror(codePoint);
}

static uint8_t BracketType(uint32_t codePoint)
{
    return muniGetBracketType(codePoint);
}

static uint8_t ScriptExtensionsIndex(uint32_t codePoint)
{
    return muniLookupScriptExtensions(codePoint);
}

static uint8_t CombiningClass(uint32_t codePoint)
{
    return muniGetCombiningClass(codePoint);
}

static uint8_t ScriptIndex(uint32_t codePoint)
{
    return muniLookupScript(codePoint);
}

static void TestLayoutPropertiesMatchTheUcdEverywhere(void)
{
    CHECK(HashAll(LineBreak) == muniLineBreakHash, "Line_Break hash");
    CHECK(HashAll(EastAsianWidth) == muniEastAsianWidthHash, "East_Asian_Width hash");
    CHECK(HashAll(BidiClass) == muniBidiClassHash, "Bidi_Class hash");
    CHECK(HashAll(CombiningClass) == muniCombiningClassHash, "Canonical_Combining_Class hash");
    CHECK(HashAll(MirrorIndex) == muniBidiMirrorHash, "Bidi_Mirroring_Glyph hash");
    CHECK(HashAll(ScriptExtensionsIndex) == muniScriptExtensionsHash, "Script_Extensions hash");
    CHECK(HashAll(BracketType) == muniBidiBracketHash, "Bidi_Paired_Bracket_Type hash");
    CHECK(HashAll(ScriptIndex) == muniScriptHash, "Script hash");
}

static void TestLayoutPropertiesKnownValues(void)
{
    muniScript scripts[32];
    size_t count = 0;
    CHECK(muniGetScriptExtensions('a', scripts, 32, &count) == muni_success && count == 1 &&
              scripts[0] == MUNI_SCRIPT('L', 'a', 't', 'n'),
          "a is Latin only");
    CHECK(muniGetScriptExtensions(0x0964, scripts, 32, &count) == muni_success && count > 5,
          "the Devanagari danda serves many scripts");
    CHECK(muniGetScriptExtensions(0x0964, scripts, 1, &count) == muni_errorCapacity,
          "capacity for the danda");
    CHECK(muniGetScriptExtensions(0x060C, scripts, 32, &count) == muni_success &&
              scripts[0] == MUNI_SCRIPT('A', 'r', 'a', 'b'),
          "the Arabic comma lists Arabic first");
    CHECK(muniGetMirroringGlyph('(') == ')' && muniGetMirroringGlyph(')') == '(', "( and )");
    CHECK(muniGetMirroringGlyph(0x00AB) == 0x00BB, "guillemets mirror");
    CHECK(muniGetMirroringGlyph('a') == 'a', "a has no mirror");
    CHECK(muniGetMirroringGlyph(0x2215) == 0x29F5, "division slash, a far mirror");
    CHECK(muniGetBracketType('[') == muni_bracketOpen, "[ opens");
    CHECK(muniGetBracketType(0x300B) == muni_bracketClose, "U+300B closes");
    CHECK(muniGetBracketType('<') == muni_bracketNone, "< is no bracket");
    CHECK(muniGetMirroringGlyph(0x110000) == 0x110000, "past U+10FFFF");
    CHECK(muniGetLineBreak(' ') == muni_lbSp, "space is SP");
    CHECK(muniGetLineBreak('\n') == muni_lbLf, "LF");
    CHECK(muniGetLineBreak(0x0E01) == muni_lbSa, "Thai U+0E01 is SA");
    CHECK(muniGetLineBreak(0x4E00) == muni_lbId, "U+4E00 is ID");
    CHECK(muniGetLineBreak('(') == muni_lbOp, "( is OP");
    CHECK(muniGetEastAsianWidth('a') == muni_eawNarrow, "a is Narrow");
    CHECK(muniGetEastAsianWidth(0x4E00) == muni_eawWide, "U+4E00 is Wide");
    CHECK(muniGetEastAsianWidth(0xFF21) == muni_eawFullwidth, "U+FF21 is Fullwidth");
    CHECK(muniGetBidiClass('a') == muni_bcL, "a is L");
    CHECK(muniGetBidiClass(0x05D0) == muni_bcR, "U+05D0 is R");
    CHECK(muniGetBidiClass(0x0627) == muni_bcAl, "U+0627 is AL");
    CHECK(muniGetBidiClass('1') == muni_bcEn, "1 is EN");
    CHECK(muniGetBidiClass(0x05FF) == muni_bcR, "unassigned U+05FF defaults to R");
    CHECK(muniGetBidiClass(0x2067) == muni_bcRli, "U+2067 is RLI");
    CHECK(muniGetCombiningClass(0x0301) == 230, "U+0301 has class 230");
    CHECK(muniGetCombiningClass('a') == 0, "a has class 0");
    CHECK(muniGetScript('a') == MUNI_SCRIPT('L', 'a', 't', 'n'), "a is Latin");
    CHECK(muniGetScript(0x0627) == MUNI_SCRIPT('A', 'r', 'a', 'b'), "U+0627 is Arabic");
    CHECK(muniGetScript(' ') == MUNI_SCRIPT_COMMON, "space is Common");
    CHECK(muniGetScript(0x0301) == MUNI_SCRIPT_INHERITED, "U+0301 is Inherited");
    CHECK(muniGetScript(0x0378) == MUNI_SCRIPT_UNKNOWN, "U+0378 is Unknown");
    CHECK(muniGetScript(0x110000) == MUNI_SCRIPT_UNKNOWN, "past U+10FFFF is Unknown");
}

int main(void)
{
    TestLayoutPropertiesMatchTheUcdEverywhere();
    TestLayoutPropertiesKnownValues();
    TestGeneralCategoryMatchesTheUcdEverywhere();
    TestGeneralCategoryKnownValues();
    TestBreakPropertiesMatchTheUcdEverywhere();
    TestBreakPropertiesKnownValues();
    return s_failures == 0 ? 0 : 1;
}
