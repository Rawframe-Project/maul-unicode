// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// muniGetEmojiKind: whether a text is one RGI emoji sequence of UTS #51,
// from the emoji properties and the tables tools/munigen/emoji.c
// describes.

#include "encoding.h"
#include "tables.h"

#include "maul-unicode/properties.h"

#define MAX_CODE_POINTS 16 // longer than any RGI emoji sequence
#define MAX_ELEMENTS    8
#define ZWJ             0x200Du
#define VS16            0xFE0Fu
#define KEYCAP          0x20E3u
#define BLACK_FLAG      0x1F3F4u
#define CANCEL_TAG      0xE007Fu
#define FIRST_MODIFIER  0x1F3FBu
#define FIRST_INDICATOR 0x1F1E6u

MUNI_NODISCARD static bool Has(uint32_t codePoint, uint8_t bit)
{
    return (muniLookupEmoji(codePoint) & bit) != 0;
}

MUNI_NODISCARD static bool IsRegionalIndicator(uint32_t c)
{
    return c - FIRST_INDICATOR < 26;
}

MUNI_NODISCARD static bool IsKeycapBase(uint32_t c)
{
    return c == '#' || c == '*' || c - '0' < 10;
}

// The modifier's number, 1 to 5, or 0 for a code point that is none.
static uint8_t ModifierNumber(uint32_t c)
{
    return c - FIRST_MODIFIER < 5 ? (uint8_t)(c - FIRST_MODIFIER + 1) : 0;
}

MUNI_NODISCARD static bool TakesModifiers(uint32_t base)
{
    if (!Has(base, muni_emojiModifierBase))
    {
        return false;
    }
    for (uint8_t i = 0; i < muniEmojiUnmodifiableCount; i++)
    {
        if (muniEmojiUnmodifiable[i] == base)
        {
            return false;
        }
    }
    return true;
}

MUNI_NODISCARD static bool IsFlag(uint32_t first, uint32_t second)
{
    if (!IsRegionalIndicator(first) || !IsRegionalIndicator(second))
    {
        return false;
    }
    unsigned bit = (first - FIRST_INDICATOR) * 26 + (second - FIRST_INDICATOR);
    return (muniEmojiFlags[bit / 8] >> (bit % 8) & 1) != 0;
}

MUNI_NODISCARD static bool IsTagSequence(const uint32_t* s, size_t n)
{
    if (n < 3 || s[0] != BLACK_FLAG || s[n - 1] != CANCEL_TAG || n - 2 > 8)
    {
        return false;
    }
    for (uint8_t t = 0; t < muniEmojiTagCount; t++)
    {
        const char* tag = muniEmojiTags[t];
        size_t k = 0;
        while (k < n - 2 && tag[k] != '\0' && s[k + 1] == 0xE0000u + (uint8_t)tag[k])
        {
            k++;
        }
        if (k == n - 2 && tag[k] == '\0')
        {
            return true;
        }
    }
    return false;
}

// The index of a ZWJ element's base in the sorted base list, or -1.
static int BaseIndex(uint32_t base)
{
    int low = 0;
    int high = muniEmojiZwjBaseCount;
    while (low < high)
    {
        int middle = (low + high) / 2;
        if (muniEmojiZwjBases[middle] < base)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return low < muniEmojiZwjBaseCount && muniEmojiZwjBases[low] == base ? low : -1;
}

// Whether one skeleton, at its header, has the elements and modifiers.
MUNI_NODISCARD static bool SkeletonHas(const uint8_t* skeleton, const uint8_t* bases,
                                       const uint8_t* modifiers, size_t count)
{
    uint8_t header = skeleton[0];
    uint8_t mask = skeleton[1];
    if ((header & 15u) != count)
    {
        return false;
    }
    uint8_t seen = 0;
    bool differ = true;
    bool plain = true;
    for (size_t k = 0; k < count; k++)
    {
        bool masked = (mask >> k & 1) != 0;
        plain = plain && modifiers[k] == 0;
        if (skeleton[2 + k] != bases[k] || (modifiers[k] != 0 && !masked))
        {
            return false;
        }
        if (modifiers[k] != 0)
        {
            differ = differ && (seen >> modifiers[k] & 1) == 0;
            seen |= (uint8_t)(1u << modifiers[k]);
        }
    }
    if (plain)
    {
        return (header & 16u) != 0;
    }
    for (size_t k = 0; k < count; k++)
    {
        if ((mask >> k & 1) != 0 && modifiers[k] == 0)
        {
            return false;
        }
    }
    return (header & 32u) == 0 || differ;
}

// Whether s is an RGI ZWJ sequence: elements joined by U+200D, each a
// base with U+FE0F when it is not emoji by default and has no modifier,
// or a base and a modifier, whose bases make a stored skeleton that has
// those modifiers.
MUNI_NODISCARD static bool IsZwjSequence(const uint32_t* s, size_t n)
{
    uint8_t bases[MAX_ELEMENTS];
    uint8_t modifiers[MAX_ELEMENTS];
    size_t count = 0;
    size_t i = 0;
    while (i < n)
    {
        int base = BaseIndex(s[i]);
        if (base < 0 || count == MAX_ELEMENTS)
        {
            return false;
        }
        uint8_t modifier = i + 1 < n ? ModifierNumber(s[i + 1]) : 0;
        bool selector = i + 1 < n && s[i + 1] == VS16;
        if (selector == (Has(s[i], muni_emojiPresentation) || modifier != 0))
        {
            return false;
        }
        bases[count] = (uint8_t)base;
        modifiers[count++] = modifier;
        i += selector || modifier != 0 ? 2 : 1;
        if (i < n && (s[i] != ZWJ || i + 1 == n))
        {
            return false;
        }
        i += 1;
    }
    if (count < 2)
    {
        return false;
    }
    for (uint16_t at = muniEmojiZwjStarts[bases[0]]; at < muniEmojiZwjStarts[bases[0] + 1];
         at += (uint16_t)(2 + (muniEmojiZwjSkeletons[at] & 15u)))
    {
        if (SkeletonHas(muniEmojiZwjSkeletons + at, bases, modifiers, count))
        {
            return true;
        }
    }
    return false;
}

static muniEmojiKind KindOf(const uint32_t* s, size_t n)
{
    if (n == 1)
    {
        return Has(s[0], muni_emojiPresentation) && !IsRegionalIndicator(s[0]) ? muni_emojiKindBasic
                                                                               : muni_emojiKindNone;
    }
    if (n == 2)
    {
        if (s[1] == VS16 && Has(s[0], muni_emojiEmoji) && !Has(s[0], muni_emojiPresentation) &&
            !IsKeycapBase(s[0]))
        {
            return muni_emojiKindBasic;
        }
        if (IsFlag(s[0], s[1]))
        {
            return muni_emojiKindFlag;
        }
        if (ModifierNumber(s[1]) != 0 && TakesModifiers(s[0]))
        {
            return muni_emojiKindModifier;
        }
    }
    if (n == 3 && IsKeycapBase(s[0]) && s[1] == VS16 && s[2] == KEYCAP)
    {
        return muni_emojiKindKeycap;
    }
    if (IsTagSequence(s, n))
    {
        return muni_emojiKindTag;
    }
    return IsZwjSequence(s, n) ? muni_emojiKindZwj : muni_emojiKindNone;
}

muniResult muniGetEmojiKind(const char* text, size_t length, muniEmojiKind* kindOut)
{
    if ((text == nullptr && length != 0) || kindOut == nullptr)
    {
        return muni_errorInvalid;
    }
    // An ill-formed sequence, U+FFFD, is in no emoji sequence.
    uint32_t s[MAX_CODE_POINTS];
    size_t n = 0;
    size_t offset = 0;
    while (offset < length)
    {
        size_t size;
        if (n == MAX_CODE_POINTS || muniStepUtf8((const uint8_t*)text + offset, length - offset,
                                                 &s[n], &size) != muni_success)
        {
            *kindOut = muni_emojiKindNone;
            return muni_success;
        }
        offset += size;
        n++;
    }
    *kindOut = n == 0 ? muni_emojiKindNone : KindOf(s, n);
    return muni_success;
}
