// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// See width.h.

#include "width.h"

#include "encoding.h"
#include "tables.h"

#include "maul-unicode/properties.h"

#define TEXT_PRESENTATION  0xFE0Eu
#define EMOJI_PRESENTATION 0xFE0Fu

MUNI_NODISCARD static bool IsRegionalIndicator(uint32_t codePoint)
{
    return codePoint >= 0x1F1E6 && codePoint <= 0x1F1FF;
}

size_t muniClusterWidth(const uint8_t* cluster, size_t length, bool ambiguousWide)
{
    uint32_t base;
    size_t size;
    (void)muniStepUtf8(cluster, length, &base, &size);
    if (base < 0x7F && size == length)
    {
        return base >= 0x20 ? 1 : 0;
    }
    uint32_t next = 0;
    if (size < length)
    {
        size_t nextSize;
        (void)muniStepUtf8(cluster + size, length - size, &next, &nextSize);
    }
    // Controls, separators, default ignorables and a mark with no base
    // take no column.
    uint8_t category = muniLookupGeneralCategory(base);
    if (category == muni_gcCc || category == muni_gcZl || category == muni_gcZp ||
        category == muni_gcMn || category == muni_gcMe || muniLookupDefaultIgnorable(base) != 0)
    {
        return 0;
    }
    // An emoji: U+FE0E asks for text, U+FE0F for emoji presentation, and
    // without either its default presentation holds. A regional
    // indicator is an emoji only in a pair, a flag.
    uint8_t emoji = muniLookupEmoji(base);
    if ((emoji & muni_emojiEmoji) != 0)
    {
        if (next == TEXT_PRESENTATION)
        {
            return 1;
        }
        if (IsRegionalIndicator(base))
        {
            return IsRegionalIndicator(next) ? 2 : 1;
        }
        if (next == EMOJI_PRESENTATION || (emoji & muni_emojiPresentation) != 0)
        {
            return 2;
        }
    }
    uint8_t width = muniLookupEastAsianWidth(base);
    if (width == muni_eawWide || width == muni_eawFullwidth)
    {
        return 2;
    }
    return width == muni_eawAmbiguous && ambiguousWide ? 2 : 1;
}
