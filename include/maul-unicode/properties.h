// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Character properties from the Unicode Character Database. Every lookup
// takes a code point, cannot fail and returns the property's value; a
// value above U+10FFFF gets the value of an unassigned code point.

#ifndef MAUL_UNICODE_PROPERTIES_H
#define MAUL_UNICODE_PROPERTIES_H

#include "maul-unicode/base.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // General_Category, one value per UCD short name. The numbering is the
    // library's own and stable within a major version.
    typedef uint8_t muniGeneralCategory;

    enum
    {
        muni_gcCn = 0,  // Unassigned
        muni_gcLu = 1,  // Uppercase_Letter
        muni_gcLl = 2,  // Lowercase_Letter
        muni_gcLt = 3,  // Titlecase_Letter
        muni_gcLm = 4,  // Modifier_Letter
        muni_gcLo = 5,  // Other_Letter
        muni_gcMn = 6,  // Nonspacing_Mark
        muni_gcMc = 7,  // Spacing_Mark
        muni_gcMe = 8,  // Enclosing_Mark
        muni_gcNd = 9,  // Decimal_Number
        muni_gcNl = 10, // Letter_Number
        muni_gcNo = 11, // Other_Number
        muni_gcPc = 12, // Connector_Punctuation
        muni_gcPd = 13, // Dash_Punctuation
        muni_gcPs = 14, // Open_Punctuation
        muni_gcPe = 15, // Close_Punctuation
        muni_gcPi = 16, // Initial_Punctuation
        muni_gcPf = 17, // Final_Punctuation
        muni_gcPo = 18, // Other_Punctuation
        muni_gcSm = 19, // Math_Symbol
        muni_gcSc = 20, // Currency_Symbol
        muni_gcSk = 21, // Modifier_Symbol
        muni_gcSo = 22, // Other_Symbol
        muni_gcZs = 23, // Space_Separator
        muni_gcZl = 24, // Line_Separator
        muni_gcZp = 25, // Paragraph_Separator
        muni_gcCc = 26, // Control
        muni_gcCf = 27, // Format
        muni_gcCs = 28, // Surrogate
        muni_gcCo = 29, // Private_Use
    };

    /// Returns the General_Category of a code point.
    ///
    /// @param codePoint  Any value; one above U+10FFFF is unassigned.
    /// @return The category, one of the muni_gc values.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_API muniGeneralCategory muniGetGeneralCategory(uint32_t codePoint);

    // Grapheme_Cluster_Break (UAX #29), in the library's numbering.
    typedef uint8_t muniGraphemeBreak;

    enum
    {
        muni_gcbOther = 0,
        muni_gcbCr = 1,
        muni_gcbLf = 2,
        muni_gcbControl = 3,
        muni_gcbExtend = 4,
        muni_gcbZwj = 5,
        muni_gcbRegionalIndicator = 6,
        muni_gcbPrepend = 7,
        muni_gcbSpacingMark = 8,
        muni_gcbL = 9,
        muni_gcbV = 10,
        muni_gcbT = 11,
        muni_gcbLv = 12,
        muni_gcbLvt = 13,
    };

    // Word_Break (UAX #29), in the library's numbering.
    typedef uint8_t muniWordBreak;

    enum
    {
        muni_wbOther = 0,
        muni_wbCr = 1,
        muni_wbLf = 2,
        muni_wbNewline = 3,
        muni_wbExtend = 4,
        muni_wbZwj = 5,
        muni_wbRegionalIndicator = 6,
        muni_wbFormat = 7,
        muni_wbKatakana = 8,
        muni_wbHebrewLetter = 9,
        muni_wbALetter = 10,
        muni_wbSingleQuote = 11,
        muni_wbDoubleQuote = 12,
        muni_wbMidNumLet = 13,
        muni_wbMidLetter = 14,
        muni_wbMidNum = 15,
        muni_wbNumeric = 16,
        muni_wbExtendNumLet = 17,
        muni_wbWSegSpace = 18,
    };

    // Sentence_Break (UAX #29), in the library's numbering.
    typedef uint8_t muniSentenceBreak;

    enum
    {
        muni_sbOther = 0,
        muni_sbCr = 1,
        muni_sbLf = 2,
        muni_sbExtend = 3,
        muni_sbSep = 4,
        muni_sbFormat = 5,
        muni_sbSp = 6,
        muni_sbLower = 7,
        muni_sbUpper = 8,
        muni_sbOLetter = 9,
        muni_sbNumeric = 10,
        muni_sbATerm = 11,
        muni_sbSContinue = 12,
        muni_sbSTerm = 13,
        muni_sbClose = 14,
    };

    // Indic_Conjunct_Break, which grapheme cluster rule GB9c reads.
    typedef uint8_t muniIndicConjunctBreak;

    enum
    {
        muni_incbNone = 0,
        muni_incbLinker = 1,
        muni_incbConsonant = 2,
        muni_incbExtend = 3,
    };

    /// Returns the Grapheme_Cluster_Break of a code point.
    ///
    /// @param codePoint  Any value; one above U+10FFFF is Other.
    /// @return The value, one of the muni_gcb values.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_API muniGraphemeBreak muniGetGraphemeBreak(uint32_t codePoint);

    /// Returns the Word_Break of a code point.
    ///
    /// @param codePoint  Any value; one above U+10FFFF is Other.
    /// @return The value, one of the muni_wb values.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_API muniWordBreak muniGetWordBreak(uint32_t codePoint);

    /// Returns the Sentence_Break of a code point.
    ///
    /// @param codePoint  Any value; one above U+10FFFF is Other.
    /// @return The value, one of the muni_sb values.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_API muniSentenceBreak muniGetSentenceBreak(uint32_t codePoint);

    /// Returns the Indic_Conjunct_Break of a code point.
    ///
    /// @param codePoint  Any value; one above U+10FFFF is None.
    /// @return The value, one of the muni_incb values.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_API muniIndicConjunctBreak muniGetIndicConjunctBreak(uint32_t codePoint);

    /// Tells whether a code point has the Extended_Pictographic property
    /// (UTS #51), which keeps emoji sequences together.
    ///
    /// @param codePoint  Any value; one above U+10FFFF has it not.
    /// @return true when the code point is Extended_Pictographic.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_API bool muniIsExtendedPictographic(uint32_t codePoint);

#ifdef __cplusplus
}
#endif

#endif // MAUL_UNICODE_PROPERTIES_H
