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

#ifdef __cplusplus
}
#endif

#endif // MAUL_UNICODE_PROPERTIES_H
