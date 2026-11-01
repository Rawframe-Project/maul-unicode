// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Property lookups: every code point through the public API against the
// hash the generator computed from the UCD. The hash covers U+0000 to
// U+10FFFF; known values check that the public enumerations name the
// values the tables hold, and that values past U+10FFFF answer as
// unassigned code points do.

#include "tables.h"
#include "test_harness.h"

#include "maul-unicode/properties.h"

#ifdef MUNI_UCD_DATA
#include "ucd.h"
#endif

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
    CHECK(muniGetGeneralCategory(0x0378) == muni_gcCn, "U+0378 is unassigned");
    CHECK(muniGetGeneralCategory(0x1F600) == muni_gcSo, "U+1F600 is So");
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

// The emoji bits in the table's order, through the public API.
static uint8_t Emoji(uint32_t codePoint)
{
    bool (*const tests[6])(uint32_t) = {
        muniIsExtendedPictographic, muniIsEmoji,
        muniIsEmojiPresentation,    muniIsEmojiModifier,
        muniIsEmojiModifierBase,    muniIsEmojiComponent,
    };
    uint8_t bits = 0;
    for (int bit = 0; bit < 6; bit++)
    {
        bits |= (uint8_t)(tests[bit](codePoint) ? 1u << bit : 0u);
    }
    return bits;
}

static uint8_t WhiteSpace(uint32_t codePoint)
{
    return muniIsWhiteSpace(codePoint) ? 1 : 0;
}

static uint8_t DefaultIgnorable(uint32_t codePoint)
{
    return muniIsDefaultIgnorable(codePoint) ? 1 : 0;
}

static uint8_t Ideographic(uint32_t codePoint)
{
    return muniIsIdeographic(codePoint) ? 1 : 0;
}

static void TestBreakPropertiesMatchTheUcdEverywhere(void)
{
    CHECK(HashAll(GraphemeBreak) == muniGraphemeClusterBreakHash, "Grapheme_Cluster_Break hash");
    CHECK(HashAll(WordBreak) == muniWordBreakHash, "Word_Break hash");
    CHECK(HashAll(SentenceBreak) == muniSentenceBreakHash, "Sentence_Break hash");
    CHECK(HashAll(IndicConjunctBreak) == muniIndicConjunctBreakHash, "Indic_Conjunct_Break hash");
    CHECK(HashAll(Emoji) == muniEmojiHash, "emoji properties hash");
    CHECK(HashAll(WhiteSpace) == muniWhiteSpaceHash, "White_Space hash");
    CHECK(HashAll(DefaultIgnorable) == muniDefaultIgnorableHash,
          "Default_Ignorable_Code_Point hash");
    CHECK(HashAll(Ideographic) == muniIdeographicHash, "Ideographic hash");
}

static void TestBreakPropertiesKnownValues(void)
{
    CHECK(muniGetGraphemeBreak('\r') == muni_gcbCr, "CR");
    CHECK(muniGetGraphemeBreak(0x200D) == muni_gcbZwj, "U+200D is ZWJ");
    CHECK(muniGetWordBreak('\'') == muni_wbSingleQuote, "apostrophe is Single_Quote");
    CHECK(muniGetSentenceBreak('.') == muni_sbATerm, "full stop is ATerm");
    CHECK(muniGetIndicConjunctBreak(0x094D) == muni_incbLinker, "U+094D is a linker");
    CHECK(muniIsEmoji('#') && !muniIsEmojiPresentation('#') && muniIsEmojiComponent('#'),
          "# can start a keycap");
    CHECK(muniIsEmojiModifier(0x1F3FB) && muniIsEmojiModifierBase(0x1F44D),
          "a skin tone and a thumbs up");
    CHECK(muniIsIdeographic(0x3006) && muniIsIdeographic(0x3007) && muniIsIdeographic(0x4E00),
          "the closing mark, number zero and a CJK ideograph are Ideographic");
    CHECK(muniIsIdeographic(0x16FE4), "the Khitan Small Script filler, a mark, is Ideographic");
    CHECK(muniIsIdeographic(0x17000) && muniIsIdeographic(0x18E00) && muniIsIdeographic(0x1B170) &&
              muniIsIdeographic(0x3D000),
          "Tangut, Jurchen, Nushu and Small Seal are Ideographic");
    CHECK(!muniIsIdeographic(0x2E80) && !muniIsIdeographic(0x2FD5),
          "the CJK radicals, Script Han, are not Ideographic");
    CHECK(!muniIsIdeographic(0x3005) && !muniIsIdeographic(0x303B),
          "the iteration marks CLDR adds by hand are not Ideographic");
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
    muniScript scripts[MUNI_MAX_SCRIPT_EXTENSIONS];
    size_t count = 0;
    CHECK(muniGetScriptExtensions('a', scripts, MUNI_MAX_SCRIPT_EXTENSIONS, &count) ==
                  muni_success &&
              count == 1 && scripts[0] == MUNI_MAKE_SCRIPT('L', 'a', 't', 'n'),
          "a is Latin only");
    CHECK(muniGetScriptExtensions(0x060C, scripts, MUNI_MAX_SCRIPT_EXTENSIONS, &count) ==
                  muni_success &&
              scripts[0] == MUNI_MAKE_SCRIPT('A', 'r', 'a', 'b'),
          "the Arabic comma lists Arabic first");
    CHECK(muniGetMirroringGlyph('(') == ')' && muniGetMirroringGlyph(')') == '(', "( and )");
    CHECK(muniGetMirroringGlyph('a') == 'a', "a has no mirror");
    CHECK(muniGetMirroringGlyph(0x2215) == 0x29F5, "division slash, a far mirror");
    CHECK(muniGetBracketType('[') == muni_bracketOpen, "[ opens");
    CHECK(muniGetBracketType(0x300B) == muni_bracketClose, "U+300B closes");
    CHECK(muniGetLineBreak(0x0E01) == muni_lbSa, "Thai U+0E01 is SA");
    CHECK(muniGetEastAsianWidth(0xFF21) == muni_eawFullwidth, "U+FF21 is Fullwidth");
    CHECK(muniGetBidiClass(0x05FF) == muni_bcR, "unassigned U+05FF defaults to R");
    CHECK(muniGetBidiClass(0x2067) == muni_bcRli, "U+2067 is RLI");
    CHECK(muniGetCombiningClass(0x0301) == 230, "U+0301 has class 230");
    CHECK(muniGetScript(0x0627) == MUNI_MAKE_SCRIPT('A', 'r', 'a', 'b'), "U+0627 is Arabic");
    CHECK(muniGetScript(0x0301) == MUNI_SCRIPT_INHERITED, "U+0301 is Inherited");
    CHECK(muniGetScript(0x0378) == MUNI_SCRIPT_UNKNOWN, "U+0378 is Unknown");
}

// Every decimal digit has a value from 0 to 9 and its system's zero
// before it; nothing else has one.
// No code point has more Script_Extensions than an array of
// MUNI_MAX_SCRIPT_EXTENSIONS holds.
static void TestScriptExtensionsBound(void)
{
    size_t most = 0;
    for (uint32_t codePoint = 0; codePoint <= 0x10FFFF; ++codePoint)
    {
        size_t count = 0;
        muniResult status = muniGetScriptExtensions(codePoint, nullptr, 0, &count);
        if (status == muni_errorCapacity && count > most)
        {
            most = count;
        }
    }
    CHECK(most > 1 && most <= MUNI_MAX_SCRIPT_EXTENSIONS, "the bound holds for every code point");
    size_t count = 7;
    muniScript scripts[1] = {MUNI_SCRIPT_COMMON};
    CHECK(muniGetScriptExtensions('a', nullptr, 1, &count) == muni_errorInvalid && count == 7,
          "no scripts with a capacity, and no count written");
    CHECK(muniGetScriptExtensions('a', scripts, 1, nullptr) == muni_errorInvalid &&
              scripts[0] == MUNI_SCRIPT_COMMON,
          "no count, and no script written");
}

static void TestDecimalDigits(void)
{
    int failures = 0;
    for (uint32_t c = 0; c < 0x110000u; c++)
    {
        int32_t value = muniGetDecimalDigitValue(c);
        bool digit = muniGetGeneralCategory(c) == muni_gcNd;
        bool valid =
            digit ? value >= 0 && value <= 9 && muniGetDecimalDigitValue(c - (uint32_t)value) == 0
                  : value == -1;
        failures += valid ? 0 : 1;
    }
    CHECK(failures == 0, "decimal digit values over every code point");
    CHECK(muniGetDecimalDigitValue('7') == 7 && muniGetDecimalDigitValue(0x0667) == 7,
          "ASCII and Arabic-Indic seven");
    CHECK(muniGetDecimalDigitValue(0x1D7D8) == 0 && muniGetDecimalDigitValue(0x1D7E1) == 9,
          "double-struck math digits");
    CHECK(muniGetDecimalDigitValue(0x00B2) == -1 && muniGetDecimalDigitValue(0x2167) == -1 &&
              muniGetDecimalDigitValue('a') == -1,
          "superscripts, Roman numerals and letters are no decimal digits");
}

// Every value past U+10FFFF, up to UINT32_MAX, gets what properties.h
// gives it: the value of an unassigned code point outside any block with
// its own defaults, and itself as its mirror.
static void TestPastTheLastCodePoint(void)
{
    static const uint32_t values[] = {0x110000,   0x110001,   0x1FFFFF,       0x200000,
                                      0x7FFFFFFF, 0x80000000, UINT32_MAX - 1, UINT32_MAX};
    int failures = 0;
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++)
    {
        uint32_t value = values[i];
        muniScript scripts[2] = {MUNI_SCRIPT_COMMON, MUNI_SCRIPT_COMMON};
        size_t count = 0;
        bool valid = muniGetGeneralCategory(value) == muni_gcCn &&
                     muniGetGraphemeBreak(value) == muni_gcbOther &&
                     muniGetWordBreak(value) == muni_wbOther &&
                     muniGetSentenceBreak(value) == muni_sbOther &&
                     muniGetIndicConjunctBreak(value) == muni_incbNone &&
                     !muniIsExtendedPictographic(value) && !muniIsEmoji(value) &&
                     !muniIsEmojiPresentation(value) && !muniIsEmojiModifier(value) &&
                     !muniIsEmojiModifierBase(value) && !muniIsEmojiComponent(value) &&
                     !muniIsWhiteSpace(value) && !muniIsDefaultIgnorable(value) &&
                     !muniIsIdeographic(value) && muniGetDecimalDigitValue(value) == -1 &&
                     muniGetLineBreak(value) == muni_lbXx &&
                     muniGetEastAsianWidth(value) == muni_eawNeutral &&
                     muniGetBidiClass(value) == muni_bcL && muniGetMirroringGlyph(value) == value &&
                     muniGetBracketType(value) == muni_bracketNone &&
                     muniGetCombiningClass(value) == 0 &&
                     muniGetScript(value) == MUNI_SCRIPT_UNKNOWN &&
                     muniGetScriptExtensions(value, scripts, 2, &count) == muni_success &&
                     count == 1 && scripts[0] == MUNI_SCRIPT_UNKNOWN;
        failures += valid ? 0 : 1;
    }
    CHECK(failures == 0, "past U+10FFFF answers as an unassigned code point");
}

#ifdef MUNI_UCD_DATA
// Every code point through the public API against the UCD files, read
// here without the generator, so that a table the generator wrote from a
// misread file fails as well as one it wrote wrong.

static const UcdName s_generalCategories[] = {
    {"Cn", "Unassigned", muni_gcCn},
    {"Lu", "Uppercase_Letter", muni_gcLu},
    {"Ll", "Lowercase_Letter", muni_gcLl},
    {"Lt", "Titlecase_Letter", muni_gcLt},
    {"Lm", "Modifier_Letter", muni_gcLm},
    {"Lo", "Other_Letter", muni_gcLo},
    {"Mn", "Nonspacing_Mark", muni_gcMn},
    {"Mc", "Spacing_Mark", muni_gcMc},
    {"Me", "Enclosing_Mark", muni_gcMe},
    {"Nd", "Decimal_Number", muni_gcNd},
    {"Nl", "Letter_Number", muni_gcNl},
    {"No", "Other_Number", muni_gcNo},
    {"Pc", "Connector_Punctuation", muni_gcPc},
    {"Pd", "Dash_Punctuation", muni_gcPd},
    {"Ps", "Open_Punctuation", muni_gcPs},
    {"Pe", "Close_Punctuation", muni_gcPe},
    {"Pi", "Initial_Punctuation", muni_gcPi},
    {"Pf", "Final_Punctuation", muni_gcPf},
    {"Po", "Other_Punctuation", muni_gcPo},
    {"Sm", "Math_Symbol", muni_gcSm},
    {"Sc", "Currency_Symbol", muni_gcSc},
    {"Sk", "Modifier_Symbol", muni_gcSk},
    {"So", "Other_Symbol", muni_gcSo},
    {"Zs", "Space_Separator", muni_gcZs},
    {"Zl", "Line_Separator", muni_gcZl},
    {"Zp", "Paragraph_Separator", muni_gcZp},
    {"Cc", "Control", muni_gcCc},
    {"Cf", "Format", muni_gcCf},
    {"Cs", "Surrogate", muni_gcCs},
    {"Co", "Private_Use", muni_gcCo},
};

static const UcdName s_graphemeBreaks[] = {
    {"XX", "Other", muni_gcbOther},
    {"CR", "CR", muni_gcbCr},
    {"LF", "LF", muni_gcbLf},
    {"CN", "Control", muni_gcbControl},
    {"EX", "Extend", muni_gcbExtend},
    {"ZWJ", "ZWJ", muni_gcbZwj},
    {"RI", "Regional_Indicator", muni_gcbRegionalIndicator},
    {"PP", "Prepend", muni_gcbPrepend},
    {"SM", "SpacingMark", muni_gcbSpacingMark},
    {"L", "L", muni_gcbL},
    {"V", "V", muni_gcbV},
    {"T", "T", muni_gcbT},
    {"LV", "LV", muni_gcbLv},
    {"LVT", "LVT", muni_gcbLvt},
};

static const UcdName s_wordBreaks[] = {
    {"XX", "Other", muni_wbOther},
    {"CR", "CR", muni_wbCr},
    {"LF", "LF", muni_wbLf},
    {"NL", "Newline", muni_wbNewline},
    {"Extend", "Extend", muni_wbExtend},
    {"ZWJ", "ZWJ", muni_wbZwj},
    {"RI", "Regional_Indicator", muni_wbRegionalIndicator},
    {"FO", "Format", muni_wbFormat},
    {"KA", "Katakana", muni_wbKatakana},
    {"HL", "Hebrew_Letter", muni_wbHebrewLetter},
    {"LE", "ALetter", muni_wbALetter},
    {"SQ", "Single_Quote", muni_wbSingleQuote},
    {"DQ", "Double_Quote", muni_wbDoubleQuote},
    {"MB", "MidNumLet", muni_wbMidNumLet},
    {"ML", "MidLetter", muni_wbMidLetter},
    {"MN", "MidNum", muni_wbMidNum},
    {"NU", "Numeric", muni_wbNumeric},
    {"EX", "ExtendNumLet", muni_wbExtendNumLet},
    {"WSegSpace", "WSegSpace", muni_wbWSegSpace},
};

static const UcdName s_sentenceBreaks[] = {
    {"XX", "Other", muni_sbOther},
    {"CR", "CR", muni_sbCr},
    {"LF", "LF", muni_sbLf},
    {"EX", "Extend", muni_sbExtend},
    {"SE", "Sep", muni_sbSep},
    {"FO", "Format", muni_sbFormat},
    {"SP", "Sp", muni_sbSp},
    {"LO", "Lower", muni_sbLower},
    {"UP", "Upper", muni_sbUpper},
    {"LE", "OLetter", muni_sbOLetter},
    {"NU", "Numeric", muni_sbNumeric},
    {"AT", "ATerm", muni_sbATerm},
    {"SC", "SContinue", muni_sbSContinue},
    {"ST", "STerm", muni_sbSTerm},
    {"CL", "Close", muni_sbClose},
};

static const UcdName s_indicConjunctBreaks[] = {
    {"None", "None", muni_incbNone},
    {"Linker", "Linker", muni_incbLinker},
    {"Consonant", "Consonant", muni_incbConsonant},
    {"Extend", "Extend", muni_incbExtend},
};

static const UcdName s_lineBreaks[] = {
    {"XX", "Unknown", muni_lbXx},
    {"BK", "Mandatory_Break", muni_lbBk},
    {"CR", "Carriage_Return", muni_lbCr},
    {"LF", "Line_Feed", muni_lbLf},
    {"CM", "Combining_Mark", muni_lbCm},
    {"NL", "Next_Line", muni_lbNl},
    {"SG", "Surrogate", muni_lbSg},
    {"WJ", "Word_Joiner", muni_lbWj},
    {"ZW", "ZWSpace", muni_lbZw},
    {"GL", "Glue", muni_lbGl},
    {"SP", "Space", muni_lbSp},
    {"ZWJ", "ZWJ", muni_lbZwj},
    {"B2", "Break_Both", muni_lbB2},
    {"BA", "Break_After", muni_lbBa},
    {"BB", "Break_Before", muni_lbBb},
    {"HY", "Hyphen", muni_lbHy},
    {"CB", "Contingent_Break", muni_lbCb},
    {"CL", "Close_Punctuation", muni_lbCl},
    {"CP", "Close_Parenthesis", muni_lbCp},
    {"EX", "Exclamation", muni_lbEx},
    {"IN", "Inseparable", muni_lbIn},
    {"NS", "Nonstarter", muni_lbNs},
    {"OP", "Open_Punctuation", muni_lbOp},
    {"QU", "Quotation", muni_lbQu},
    {"IS", "Infix_Numeric", muni_lbIs},
    {"NU", "Numeric", muni_lbNu},
    {"PO", "Postfix_Numeric", muni_lbPo},
    {"PR", "Prefix_Numeric", muni_lbPr},
    {"SY", "Break_Symbols", muni_lbSy},
    {"AI", "Ambiguous", muni_lbAi},
    {"AL", "Alphabetic", muni_lbAl},
    {"CJ", "Conditional_Japanese_Starter", muni_lbCj},
    {"EB", "E_Base", muni_lbEb},
    {"EM", "E_Modifier", muni_lbEm},
    {"H2", "H2", muni_lbH2},
    {"H3", "H3", muni_lbH3},
    {"HL", "Hebrew_Letter", muni_lbHl},
    {"ID", "Ideographic", muni_lbId},
    {"JL", "JL", muni_lbJl},
    {"JV", "JV", muni_lbJv},
    {"JT", "JT", muni_lbJt},
    {"RI", "Regional_Indicator", muni_lbRi},
    {"SA", "Complex_Context", muni_lbSa},
    {"AK", "Aksara", muni_lbAk},
    {"AP", "Aksara_Prebase", muni_lbAp},
    {"AS", "Aksara_Start", muni_lbAs},
    {"VF", "Virama_Final", muni_lbVf},
    {"VI", "Virama", muni_lbVi},
    {"HH", "Unambiguous_Hyphen", muni_lbHh},
};

static const UcdName s_eastAsianWidths[] = {
    {"N", "Neutral", muni_eawNeutral},     {"A", "Ambiguous", muni_eawAmbiguous},
    {"H", "Halfwidth", muni_eawHalfwidth}, {"W", "Wide", muni_eawWide},
    {"F", "Fullwidth", muni_eawFullwidth}, {"Na", "Narrow", muni_eawNarrow},
};

static const UcdName s_bidiClasses[] = {
    {"L", "Left_To_Right", muni_bcL},
    {"R", "Right_To_Left", muni_bcR},
    {"AL", "Arabic_Letter", muni_bcAl},
    {"EN", "European_Number", muni_bcEn},
    {"ES", "European_Separator", muni_bcEs},
    {"ET", "European_Terminator", muni_bcEt},
    {"AN", "Arabic_Number", muni_bcAn},
    {"CS", "Common_Separator", muni_bcCs},
    {"NSM", "Nonspacing_Mark", muni_bcNsm},
    {"BN", "Boundary_Neutral", muni_bcBn},
    {"B", "Paragraph_Separator", muni_bcB},
    {"S", "Segment_Separator", muni_bcS},
    {"WS", "White_Space", muni_bcWs},
    {"ON", "Other_Neutral", muni_bcOn},
    {"LRE", "Left_To_Right_Embedding", muni_bcLre},
    {"LRO", "Left_To_Right_Override", muni_bcLro},
    {"RLE", "Right_To_Left_Embedding", muni_bcRle},
    {"RLO", "Right_To_Left_Override", muni_bcRlo},
    {"PDF", "Pop_Directional_Format", muni_bcPdf},
    {"LRI", "Left_To_Right_Isolate", muni_bcLri},
    {"RLI", "Right_To_Left_Isolate", muni_bcRli},
    {"FSI", "First_Strong_Isolate", muni_bcFsi},
    {"PDI", "Pop_Directional_Isolate", muni_bcPdi},
};

static const UcdName s_bracketTypes[] = {
    {"n", "None", muni_bracketNone},
    {"o", "Open", muni_bracketOpen},
    {"c", "Close", muni_bracketClose},
};

static uint8_t s_table[UCD_CODE_POINTS];
static uint32_t s_wide[UCD_CODE_POINTS];

#define COUNT(array) (sizeof(array) / sizeof((array)[0]))

static void TestEnumeratedPropertiesAgainstTheUcd(void)
{
    static const struct
    {
        const char* file;
        const char* property; // the field 0 of the lines that count, if any
        int field;
        uint8_t unlisted;
        const UcdName* names;
        size_t count;
        uint8_t (*get)(uint32_t);
    } properties[] = {
        {"DerivedGeneralCategory.txt", nullptr, 0, muni_gcCn, s_generalCategories,
         COUNT(s_generalCategories), GeneralCategory},
        {"GraphemeBreakProperty.txt", nullptr, 0, muni_gcbOther, s_graphemeBreaks,
         COUNT(s_graphemeBreaks), GraphemeBreak},
        {"WordBreakProperty.txt", nullptr, 0, muni_wbOther, s_wordBreaks, COUNT(s_wordBreaks),
         WordBreak},
        {"SentenceBreakProperty.txt", nullptr, 0, muni_sbOther, s_sentenceBreaks,
         COUNT(s_sentenceBreaks), SentenceBreak},
        {"DerivedCoreProperties.txt", "InCB", 1, muni_incbNone, s_indicConjunctBreaks,
         COUNT(s_indicConjunctBreaks), IndicConjunctBreak},
        {"LineBreak.txt", nullptr, 0, muni_lbXx, s_lineBreaks, COUNT(s_lineBreaks), LineBreak},
        {"EastAsianWidth.txt", nullptr, 0, muni_eawNeutral, s_eastAsianWidths,
         COUNT(s_eastAsianWidths), EastAsianWidth},
        {"DerivedBidiClass.txt", nullptr, 0, muni_bcL, s_bidiClasses, COUNT(s_bidiClasses),
         BidiClass},
        {"BidiBrackets.txt", nullptr, 1, muni_bracketNone, s_bracketTypes, COUNT(s_bracketTypes),
         BracketType},
    };
    for (size_t i = 0; i < COUNT(properties); i++)
    {
        memset(s_table, properties[i].unlisted, sizeof(s_table));
        bool read = UcdFill(properties[i].file, properties[i].property, properties[i].field,
                            properties[i].names, properties[i].count, s_table);
        CHECK(read && UcdCompare(properties[i].file, properties[i].get, s_table) == 0,
              properties[i].file);
    }
}

static void TestBinaryPropertiesAgainstTheUcd(void)
{
    static const struct
    {
        const char* file;
        const char* property;
        bool (*has)(uint32_t);
    } properties[] = {
        {"emoji-data.txt", "Extended_Pictographic", muniIsExtendedPictographic},
        {"emoji-data.txt", "Emoji", muniIsEmoji},
        {"emoji-data.txt", "Emoji_Presentation", muniIsEmojiPresentation},
        {"emoji-data.txt", "Emoji_Modifier", muniIsEmojiModifier},
        {"emoji-data.txt", "Emoji_Modifier_Base", muniIsEmojiModifierBase},
        {"emoji-data.txt", "Emoji_Component", muniIsEmojiComponent},
        {"PropList.txt", "White_Space", muniIsWhiteSpace},
        {"PropList.txt", "Ideographic", muniIsIdeographic},
        {"DerivedCoreProperties.txt", "Default_Ignorable_Code_Point", muniIsDefaultIgnorable},
    };
    for (size_t i = 0; i < COUNT(properties); i++)
    {
        memset(s_table, 0, sizeof(s_table));
        bool read = UcdFillBinary(properties[i].file, properties[i].property, s_table);
        int failures = 0;
        for (uint32_t c = 0; c < UCD_CODE_POINTS; c++)
        {
            if ((properties[i].has(c) ? 1 : 0) != s_table[c] && failures++ < 5)
            {
                printf("%s of U+%04X differs\n", properties[i].property, (unsigned)c);
            }
        }
        CHECK(read && failures == 0, properties[i].property);
    }
}

static uint8_t DecimalDigit(uint32_t codePoint)
{
    return (uint8_t)(muniGetDecimalDigitValue(codePoint) + 1);
}

// The Canonical_Combining_Class and the decimal digit value of
// UnicodeData.txt, whose ranges are a First and a Last line.
static void TestUnicodeDataAgainstTheUcd(void)
{
    static uint8_t digits[UCD_CODE_POINTS];
    memset(s_table, 0, sizeof(s_table));
    memset(digits, 0, sizeof(digits));
    FILE* file = UcdOpen("UnicodeData.txt");
    CHECK(file != nullptr, "UnicodeData.txt opens");
    if (file == nullptr)
    {
        return;
    }
    static char buffer[4096];
    UcdLine line;
    uint32_t first = 0;
    while (UcdNext(file, buffer, sizeof(buffer), &line))
    {
        if (UcdEndsWith(line.fields[0], ", First>"))
        {
            first = line.first;
            continue;
        }
        bool range = UcdEndsWith(line.fields[0], ", Last>");
        for (uint32_t c = range ? first : line.first; c <= line.last; c++)
        {
            s_table[c] = (uint8_t)atoi(line.fields[2]);
            digits[c] = line.fields[5][0] != '\0' ? (uint8_t)(atoi(line.fields[5]) + 1) : 0;
        }
    }
    fclose(file);
    CHECK(UcdCompare("Canonical_Combining_Class", CombiningClass, s_table) == 0,
          "Canonical_Combining_Class of every code point");
    CHECK(UcdCompare("the decimal digit value", DecimalDigit, digits) == 0,
          "decimal digit values of every code point");
}

static void TestMirroringAgainstTheUcd(void)
{
    for (uint32_t c = 0; c < UCD_CODE_POINTS; c++)
    {
        s_wide[c] = c;
    }
    FILE* file = UcdOpen("BidiMirroring.txt");
    CHECK(file != nullptr, "BidiMirroring.txt opens");
    if (file == nullptr)
    {
        return;
    }
    static char buffer[4096];
    UcdLine line;
    while (UcdNext(file, buffer, sizeof(buffer), &line))
    {
        if (!line.missing && line.count == 1)
        {
            s_wide[line.first] = (uint32_t)strtoul(line.fields[0], nullptr, 16);
        }
    }
    fclose(file);
    int failures = 0;
    for (uint32_t c = 0; c < UCD_CODE_POINTS; c++)
    {
        if (muniGetMirroringGlyph(c) != s_wide[c] && failures++ < 5)
        {
            printf("Bidi_Mirroring_Glyph of U+%04X differs\n", (unsigned)c);
        }
    }
    CHECK(failures == 0, "Bidi_Mirroring_Glyph of every code point");
}

// The scripts of PropertyValueAliases.txt: their short and long names.
typedef struct ScriptName
{
    char shortName[8];
    char longName[64];
} ScriptName;

static ScriptName s_scriptNames[512];
static size_t s_scriptCount;

static bool LoadScriptNames(void)
{
    FILE* file = UcdOpen("PropertyValueAliases.txt");
    if (file == nullptr)
    {
        return false;
    }
    static char buffer[4096];
    s_scriptCount = 0;
    while (fgets(buffer, sizeof(buffer), file) != nullptr && s_scriptCount < COUNT(s_scriptNames))
    {
        char shortName[8];
        char longName[64];
        if (sscanf(buffer, "sc ; %7s ; %63s", shortName, longName) == 2)
        {
            strcpy(s_scriptNames[s_scriptCount].shortName, shortName);
            strcpy(s_scriptNames[s_scriptCount].longName, longName);
            s_scriptCount += 1;
        }
    }
    fclose(file);
    return s_scriptCount > 100;
}

// The tag of a script named either way, or 0.
static muniScript ScriptTag(const char* name)
{
    for (size_t i = 0; i < s_scriptCount; i++)
    {
        const char* tag = s_scriptNames[i].shortName;
        if (strcmp(tag, name) == 0 || strcmp(s_scriptNames[i].longName, name) == 0)
        {
            return MUNI_MAKE_SCRIPT(tag[0], tag[1], tag[2], tag[3]);
        }
    }
    printf("no script named %s\n", name);
    return 0;
}

// Whether the extensions of a code point are the scripts given, in any
// order.
static bool ExtensionsAre(uint32_t codePoint, const muniScript* scripts, size_t count)
{
    muniScript have[MUNI_MAX_SCRIPT_EXTENSIONS];
    size_t haveCount = 0;
    if (muniGetScriptExtensions(codePoint, have, COUNT(have), &haveCount) != muni_success ||
        haveCount != count)
    {
        return false;
    }
    for (size_t i = 0; i < count; i++)
    {
        bool found = false;
        for (size_t j = 0; j < haveCount; j++)
        {
            found = found || have[j] == scripts[i];
        }
        if (!found)
        {
            return false;
        }
    }
    return true;
}

static void TestScriptsAgainstTheUcd(void)
{
    CHECK(LoadScriptNames(), "PropertyValueAliases.txt names the scripts");
    FILE* file = UcdOpen("Scripts.txt");
    CHECK(file != nullptr, "Scripts.txt opens");
    if (file == nullptr)
    {
        return;
    }
    static char buffer[4096];
    UcdLine line;
    for (uint32_t c = 0; c < UCD_CODE_POINTS; c++)
    {
        s_wide[c] = MUNI_SCRIPT_UNKNOWN;
    }
    while (UcdNext(file, buffer, sizeof(buffer), &line))
    {
        muniScript tag = ScriptTag(line.fields[0]);
        for (uint32_t c = line.first; c <= line.last && !line.missing; c++)
        {
            s_wide[c] = tag;
        }
    }
    fclose(file);
    int failures = 0;
    for (uint32_t c = 0; c < UCD_CODE_POINTS; c++)
    {
        if (muniGetScript(c) != s_wide[c] && failures++ < 5)
        {
            printf("Script of U+%04X differs\n", (unsigned)c);
        }
    }
    CHECK(failures == 0, "Script of every code point");

    // A code point ScriptExtensions.txt does not list has its Script alone.
    file = UcdOpen("ScriptExtensions.txt");
    CHECK(file != nullptr, "ScriptExtensions.txt opens");
    if (file == nullptr)
    {
        return;
    }
    memset(s_table, 0, sizeof(s_table));
    failures = 0;
    while (UcdNext(file, buffer, sizeof(buffer), &line))
    {
        if (line.missing)
        {
            continue;
        }
        muniScript scripts[MUNI_MAX_SCRIPT_EXTENSIONS];
        size_t count = 0;
        for (char* name = strtok(line.fields[0], " "); name != nullptr && count < COUNT(scripts);
             name = strtok(nullptr, " "))
        {
            scripts[count++] = ScriptTag(name);
        }
        for (uint32_t c = line.first; c <= line.last; c++)
        {
            s_table[c] = 1;
            if (!ExtensionsAre(c, scripts, count) && failures++ < 5)
            {
                printf("Script_Extensions of U+%04X differ\n", (unsigned)c);
            }
        }
    }
    fclose(file);
    for (uint32_t c = 0; c < UCD_CODE_POINTS; c++)
    {
        if (s_table[c] == 0 && !ExtensionsAre(c, &s_wide[c], 1) && failures++ < 5)
        {
            printf("Script_Extensions of U+%04X are not its Script\n", (unsigned)c);
        }
    }
    CHECK(failures == 0, "Script_Extensions of every code point");
}
#endif

int main(void)
{
    TestLayoutPropertiesMatchTheUcdEverywhere();
    TestLayoutPropertiesKnownValues();
    TestGeneralCategoryMatchesTheUcdEverywhere();
    TestGeneralCategoryKnownValues();
    TestBreakPropertiesMatchTheUcdEverywhere();
    TestBreakPropertiesKnownValues();
    TestScriptExtensionsBound();
    TestDecimalDigits();
    TestPastTheLastCodePoint();
#ifdef MUNI_UCD_DATA
    TestEnumeratedPropertiesAgainstTheUcd();
    TestBinaryPropertiesAgainstTheUcd();
    TestUnicodeDataAgainstTheUcd();
    TestMirroringAgainstTheUcd();
    TestScriptsAgainstTheUcd();
#endif
    return s_failures == 0 ? 0 : 1;
}
