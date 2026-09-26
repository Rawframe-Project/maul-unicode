// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Property lookups over the generated tables.

#include "maul-unicode/properties.h"

#include "tables.h"

muniGeneralCategory muniGetGeneralCategory(uint32_t codePoint)
{
    return muniLookupGeneralCategory(codePoint);
}

muniGraphemeBreak muniGetGraphemeBreak(uint32_t codePoint)
{
    return muniLookupGraphemeClusterBreak(codePoint);
}

muniWordBreak muniGetWordBreak(uint32_t codePoint)
{
    return muniLookupWordBreak(codePoint);
}

muniSentenceBreak muniGetSentenceBreak(uint32_t codePoint)
{
    return muniLookupSentenceBreak(codePoint);
}

muniIndicConjunctBreak muniGetIndicConjunctBreak(uint32_t codePoint)
{
    return muniLookupIndicConjunctBreak(codePoint);
}

bool muniIsExtendedPictographic(uint32_t codePoint)
{
    return muniLookupExtendedPictographic(codePoint) != 0;
}
