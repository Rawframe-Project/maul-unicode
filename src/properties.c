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

muniLineBreak muniGetLineBreak(uint32_t codePoint)
{
    return muniLookupLineBreak(codePoint);
}

muniEastAsianWidth muniGetEastAsianWidth(uint32_t codePoint)
{
    return muniLookupEastAsianWidth(codePoint);
}

muniBidiClass muniGetBidiClass(uint32_t codePoint)
{
    return muniLookupBidiClass(codePoint);
}

uint32_t muniGetMirroringGlyph(uint32_t codePoint)
{
    uint8_t index = muniLookupBidiMirror(codePoint);
    return index == 0 ? codePoint
                      : (uint32_t)((int32_t)codePoint + muniBidiMirrorDeltas[index - 1]);
}

muniBracketType muniGetBracketType(uint32_t codePoint)
{
    return muniLookupBidiBracket(codePoint);
}

uint8_t muniGetCombiningClass(uint32_t codePoint)
{
    return muniLookupCombiningClass(codePoint);
}

muniScript muniGetScript(uint32_t codePoint)
{
    return muniScriptTags[muniLookupScript(codePoint)];
}
