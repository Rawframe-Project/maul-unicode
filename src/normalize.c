// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Normalization of text (UAX #15): each code point is decomposed fully
// and fed to the normalizer (src/normalizer.h), which orders and
// composes; and the quick check of section 9.

#include "maul-unicode/normalize.h"

#include "decompose.h"
#include "encoding.h"
#include "normalizer.h"
#include "overlap.h"
#include "tables.h"

MUNI_NODISCARD static bool IsForm(muniNormalForm form)
{
    return form == muni_nfc || form == muni_nfd || form == muni_nfkc || form == muni_nfkd;
}

// Each form's muni_quickCheck bits for No and for Maybe.
static const uint8_t s_quickCheckNo[4] = {
    [muni_nfc] = muni_quickCheckNfcNo,
    [muni_nfd] = muni_quickCheckNfdNo,
    [muni_nfkc] = muni_quickCheckNfkcNo,
    [muni_nfkd] = muni_quickCheckNfkdNo,
};
static const uint8_t s_quickCheckMaybe[4] = {
    [muni_nfc] = muni_quickCheckNfcMaybe,
    [muni_nfkc] = muni_quickCheckNfkcMaybe,
};

// The quick-check value of one code point in a form (UAX #15 section 9),
// which tools/munigen/normalization.c derives and checks against the UCD.
static muniQuickCheck QuickCheck(uint32_t codePoint, muniNormalForm form)
{
    uint8_t bits = muniLookupQuickCheck(codePoint);
    return (bits & s_quickCheckNo[form]) != 0      ? muni_quickCheckNo
           : (bits & s_quickCheckMaybe[form]) != 0 ? muni_quickCheckMaybe
                                                   : muni_quickCheckYes;
}

// Whether a code point is written as it is in the form, needing no
// segment of its own: quick check Yes and combining class 0, so it does
// not change, does not combine with what precedes it, and ends the
// segment before it. A mark after it could still combine with it, so of
// a run of them all but the last are copied.
MUNI_NODISCARD static bool IsInert(uint32_t codePoint, muniNormalForm form)
{
    // Nothing below U+00A0 decomposes, combines or has a class.
    unsigned mask = s_quickCheckNo[form] | s_quickCheckMaybe[form] | muni_quickCheckMarked;
    return codePoint < 0xA0 || (muniLookupQuickCheck(codePoint) & mask) == 0;
}

// Where the last code point starts of the run of inert code points that
// starts at offset with one of size bytes. An ill-formed sequence ends
// the run, since U+FFFD is written in its place.
static size_t LastInert(const uint8_t* bytes, size_t length, size_t offset, size_t size,
                        muniNormalForm form)
{
    size_t last = offset;
    size_t end = offset + size;
    while (end < length)
    {
        if (bytes[end] < 0x80)
        {
            last = end;
            end += 1;
            continue;
        }
        uint32_t codePoint;
        size_t next;
        if (muniStepUtf8(bytes + end, length - end, &codePoint, &next) != muni_success ||
            !IsInert(codePoint, form))
        {
            break;
        }
        last = end;
        end += next;
    }
    return last;
}

muniTextResult muniNormalize(const char* text, size_t length, muniNormalForm form,
                             muniConvertMode mode, char* textOut, size_t capacity,
                             size_t* neededOut)
{
    if ((text == nullptr && length != 0) || (textOut == nullptr && capacity != 0) ||
        neededOut == nullptr || !IsForm(form) ||
        (mode != muni_convertStrict && mode != muni_convertReplace) ||
        muniOverlaps(text, length, textOut, capacity, 1))
    {
        return (muniTextResult){muni_errorInvalid, 0};
    }
    muniNormalizer normalizer;
    muniNormalizerInit(&normalizer, form == muni_nfc || form == muni_nfkc, textOut, capacity);
    bool compatibility = form == muni_nfkc || form == muni_nfkd;
    const uint8_t* bytes = (const uint8_t*)text;
    size_t offset = 0;
    while (offset < length)
    {
        uint32_t codePoint;
        size_t size;
        muniResult status = muniStepUtf8(bytes + offset, length - offset, &codePoint, &size);
        if (status != muni_success && mode == muni_convertStrict)
        {
            muniNormalizerFlush(&normalizer);
            *neededOut = normalizer.writer.needed;
            return (muniTextResult){status, offset};
        }
        if (status == muni_success && IsInert(codePoint, form))
        {
            // The run but its last code point is written as it is; the
            // last starts the next segment, below.
            size_t last = LastInert(bytes, length, offset, size, form);
            if (last != offset)
            {
                muniNormalizerFlush(&normalizer);
                muniWriterPutBytes(&normalizer.writer, bytes + offset, last - offset);
                offset = last;
                (void)muniStepUtf8(bytes + offset, length - offset, &codePoint, &size);
            }
        }
        uint32_t parts[MUNI_MAX_DECOMPOSITION];
        size_t count = 1;
        parts[0] = codePoint;
        if (codePoint >= 0xA0) // nothing below U+00A0 decomposes
        {
            count = muniDecomposeFully(codePoint, compatibility, parts);
        }
        for (size_t i = 0; i < count; i++)
        {
            if (!muniNormalizerAdd(&normalizer, parts[i]))
            {
                *neededOut = normalizer.writer.needed;
                return (muniTextResult){muni_errorLimit, offset};
            }
        }
        offset += size;
    }
    muniNormalizerFlush(&normalizer);
    *neededOut = normalizer.writer.needed;
    return (muniTextResult){!muniWriterFits(&normalizer.writer) ? muni_errorCapacity : muni_success,
                            length};
}

muniTextResult muniCheckNormalization(const char* text, size_t length, muniNormalForm form,
                                      muniQuickCheck* answerOut)
{
    if ((text == nullptr && length != 0) || answerOut == nullptr || !IsForm(form))
    {
        return (muniTextResult){muni_errorInvalid, 0};
    }
    const uint8_t* bytes = (const uint8_t*)text;
    muniQuickCheck answer = muni_quickCheckYes;
    uint8_t lastClass = 0;
    size_t offset = 0;
    while (offset < length)
    {
        uint32_t codePoint;
        size_t size;
        muniResult status = muniStepUtf8(bytes + offset, length - offset, &codePoint, &size);
        if (status != muni_success)
        {
            return (muniTextResult){status, offset};
        }
        offset += size;
        if (codePoint < 0xA0)
        {
            lastClass = 0;
            continue;
        }
        uint8_t markClass = muniLookupCombiningClass(codePoint);
        muniQuickCheck check = QuickCheck(codePoint, form);
        if ((markClass != 0 && lastClass > markClass) || check == muni_quickCheckNo)
        {
            // The answer is settled, but the rest must still be valid.
            muniTextResult rest = muniValidateUtf8(text + offset, length - offset);
            if (rest.status != muni_success)
            {
                return (muniTextResult){rest.status, offset + rest.offset};
            }
            *answerOut = muni_quickCheckNo;
            return (muniTextResult){muni_success, length};
        }
        answer = check == muni_quickCheckMaybe ? check : answer;
        lastClass = markClass;
    }
    *answerOut = answer;
    return (muniTextResult){muni_success, length};
}

muniResult muniGetDecomposition(uint32_t codePoint, muniNormalForm form, uint32_t* codePointsOut,
                                size_t capacity, size_t* countOut)
{
    if ((codePointsOut == nullptr && capacity != 0) || countOut == nullptr ||
        (form != muni_nfd && form != muni_nfkd))
    {
        return muni_errorInvalid;
    }
    uint32_t parts[MUNI_MAX_DECOMPOSITION];
    size_t count = muniDecomposeFully(codePoint, form == muni_nfkd, parts);
    for (size_t i = 0; i < count && i < capacity; i++)
    {
        codePointsOut[i] = parts[i];
    }
    *countOut = count;
    return count <= capacity ? muni_success : muni_errorCapacity;
}

muniResult muniDecomposePair(uint32_t codePoint, uint32_t* firstOut, uint32_t* secondOut,
                             size_t* countOut)
{
    if (firstOut == nullptr || secondOut == nullptr || countOut == nullptr)
    {
        return muni_errorInvalid;
    }
    uint32_t first = 0;
    uint32_t second = 0;
    size_t count = 0;
    if (muniCanonicalMapping(codePoint, &first, &second))
    {
        count = second == 0 ? 1 : 2;
    }
    else
    {
        first = 0;
        second = 0;
    }
    *firstOut = first;
    *secondOut = second;
    *countOut = count;
    return muni_success;
}

uint32_t muniComposePair(uint32_t first, uint32_t second)
{
    return muniComposeCanonical(first, second);
}
