// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// muniResolveBidi: one paragraph through the explicit and the implicit
// phases.

#include "maul-unicode/bidi.h"

#include "bidi_core.h"
#include "bidi_explicit.h"
#include "bidi_implicit.h"

#include <string.h>

muniResult muniResolveBidi(const char* text, size_t length, muniBidiDirection direction,
                           uint8_t* levelsOut, uint8_t* workspace, size_t* paragraphLengthOut,
                           uint8_t* paragraphLevelOut)
{
    bool missing = (text == nullptr || levelsOut == nullptr || workspace == nullptr) && length != 0;
    if (missing || paragraphLengthOut == nullptr || paragraphLevelOut == nullptr ||
        (uint32_t)direction > muni_bidiRightToLeft)
    {
        return muni_errorInvalid;
    }
    if (length == 0)
    {
        *paragraphLengthOut = 0;
        *paragraphLevelOut = direction == muni_bidiRightToLeft ? 1 : 0;
        return muni_success;
    }
    const uint8_t* bytes = (const uint8_t*)text;
    // The text stands for the paragraph until the explicit phase finds
    // where it ends: the search for the first strong character ends at a
    // paragraph separator too.
    muniBidiParagraph paragraph = {bytes, length, levelsOut, workspace, 0};
    if (direction == muni_bidiAuto)
    {
        paragraph.level = muniBidiFirstStrongLevel(&paragraph, 0, false, 0, nullptr);
    }
    else
    {
        paragraph.level = direction == muni_bidiRightToLeft ? 1 : 0;
    }
    if (muniBidiResolveExplicit(&paragraph))
    {
        muniBidiResolveImplicit(&paragraph);
    }
    else
    {
        memset(levelsOut, 0, paragraph.length);
    }
    *paragraphLengthOut = paragraph.length;
    *paragraphLevelOut = paragraph.level;
    return muni_success;
}

muniResult muniGetBaseDirection(const char* text, size_t length, muniBidiDirection* directionOut,
                                size_t* offsetOut)
{
    if ((text == nullptr && length != 0) || directionOut == nullptr)
    {
        return muni_errorInvalid;
    }
    // The search ends at the first paragraph separator, so the paragraph
    // need not be measured first: the whole text stands for it.
    const muniBidiParagraph paragraph = {(const uint8_t*)text, length, nullptr, nullptr, 0};
    size_t offset;
    uint8_t level = muniBidiFirstStrongLevel(&paragraph, 0, false, 2, &offset);
    *directionOut = level == 2   ? muni_bidiAuto
                    : level == 1 ? muni_bidiRightToLeft
                                 : muni_bidiLeftToRight;
    if (offsetOut != nullptr)
    {
        *offsetOut = offset;
    }
    return muni_success;
}
