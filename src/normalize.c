// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Normalization in one pass. Each code point is decomposed fully and its
// parts go into a segment: a starter (combining class 0) and the marks
// after it, which insertion keeps in canonical order. A new starter ends
// the segment; in the composing forms the segment is composed first, and
// when it composed into a lone starter, that starter may compose with
// the new one too, as Hangul LV + T and U+0B47 + U+0B3E do. Then the
// segment is written out and the new starter begins the next.

#include "maul-unicode/normalize.h"

#include "decompose.h"
#include "encoding.h"
#include "tables.h"

#define MAX_SEGMENT 32

typedef struct Normalizer
{
    uint32_t segment[MAX_SEGMENT];
    uint8_t classes[MAX_SEGMENT];
    size_t count;
    bool compose;
    bool compatibility;
    char* output;
    size_t capacity;
    size_t needed; // bytes the output needs so far
} Normalizer;

static void Emit(Normalizer* normalizer, uint32_t codePoint)
{
    char bytes[4];
    size_t size = 0;
    (void)muniEncodeUtf8(codePoint, bytes, &size);
    for (size_t i = 0; i < size; i++)
    {
        if (normalizer->needed + i < normalizer->capacity)
        {
            normalizer->output[normalizer->needed + i] = bytes[i];
        }
    }
    normalizer->needed += size;
}

// Canonical composition of the segment (UAX #15 D117): each mark that no
// uncomposed mark before it blocks tries to compose with the starter.
static void ComposeSegment(Normalizer* normalizer)
{
    if (normalizer->count < 2 || normalizer->classes[0] != 0)
    {
        return;
    }
    uint32_t starter = normalizer->segment[0];
    size_t kept = 1;
    uint8_t lastKept = 0; // the class of the last uncomposed mark, 0 for none
    for (size_t i = 1; i < normalizer->count; i++)
    {
        uint32_t mark = normalizer->segment[i];
        uint8_t markClass = normalizer->classes[i];
        bool blocked = lastKept != 0 && lastKept >= markClass;
        uint32_t composite = blocked ? 0 : muniComposeCanonical(starter, mark);
        if (composite != 0)
        {
            starter = composite;
            continue;
        }
        normalizer->segment[kept] = mark;
        normalizer->classes[kept] = markClass;
        kept += 1;
        lastKept = markClass;
    }
    normalizer->segment[0] = starter;
    normalizer->count = kept;
}

static void Flush(Normalizer* normalizer)
{
    if (normalizer->compose)
    {
        ComposeSegment(normalizer);
    }
    for (size_t i = 0; i < normalizer->count; i++)
    {
        Emit(normalizer, normalizer->segment[i]);
    }
    normalizer->count = 0;
}

// Adds one code point of a full decomposition; false when the segment
// would outgrow MAX_SEGMENT.
static bool Add(Normalizer* normalizer, uint32_t codePoint)
{
    uint8_t markClass = codePoint < 0x300 ? 0 : muniLookupCombiningClass(codePoint);
    if (markClass == 0)
    {
        // Nothing below U+0300 composes with what precedes it.
        if (normalizer->compose && normalizer->count > 0 && codePoint >= 0x300)
        {
            ComposeSegment(normalizer);
            uint32_t composite = normalizer->count == 1 && normalizer->classes[0] == 0
                                     ? muniComposeCanonical(normalizer->segment[0], codePoint)
                                     : 0;
            if (composite != 0)
            {
                normalizer->segment[0] = composite;
                return true;
            }
        }
        Flush(normalizer);
        normalizer->segment[0] = codePoint;
        normalizer->classes[0] = 0;
        normalizer->count = 1;
        return true;
    }
    if (normalizer->count == MAX_SEGMENT)
    {
        return false;
    }
    // Canonical ordering: after every mark of the same or a lower class.
    size_t at = normalizer->count;
    while (at > 0 && normalizer->classes[at - 1] > markClass)
    {
        normalizer->segment[at] = normalizer->segment[at - 1];
        normalizer->classes[at] = normalizer->classes[at - 1];
        at -= 1;
    }
    normalizer->segment[at] = codePoint;
    normalizer->classes[at] = markClass;
    normalizer->count += 1;
    return true;
}

static bool IsForm(muniNormalForm form)
{
    return form == muni_nfc || form == muni_nfd || form == muni_nfkc || form == muni_nfkd;
}

muniTextResult muniNormalize(const char* text, size_t length, muniNormalForm form,
                             muniConvertMode mode, char* output, size_t capacity, size_t* neededOut)
{
    if ((text == nullptr && length != 0) || (output == nullptr && capacity != 0) ||
        neededOut == nullptr || !IsForm(form) ||
        (mode != muni_convertStrict && mode != muni_convertReplace))
    {
        return (muniTextResult){muni_errorInvalid, 0};
    }
    Normalizer normalizer;
    normalizer.count = 0;
    normalizer.compose = form == muni_nfc || form == muni_nfkc;
    normalizer.compatibility = form == muni_nfkc || form == muni_nfkd;
    normalizer.output = output;
    normalizer.capacity = capacity;
    normalizer.needed = 0;
    const uint8_t* bytes = (const uint8_t*)text;
    size_t offset = 0;
    while (offset < length)
    {
        uint32_t codePoint;
        size_t size;
        muniResult status = muniStepUtf8(bytes + offset, length - offset, &codePoint, &size);
        if (status != muni_success && mode == muni_convertStrict)
        {
            Flush(&normalizer);
            *neededOut = normalizer.needed;
            return (muniTextResult){status, offset};
        }
        uint32_t parts[MUNI_MAX_DECOMPOSITION];
        size_t count = 1;
        parts[0] = codePoint;
        if (codePoint >= 0xA0) // nothing below U+00A0 decomposes
        {
            count = muniDecomposeFully(codePoint, normalizer.compatibility, parts);
        }
        for (size_t i = 0; i < count; i++)
        {
            if (!Add(&normalizer, parts[i]))
            {
                *neededOut = normalizer.needed;
                return (muniTextResult){muni_errorLimit, offset};
            }
        }
        offset += size;
    }
    Flush(&normalizer);
    *neededOut = normalizer.needed;
    return (muniTextResult){normalizer.needed > capacity ? muni_errorCapacity : muni_success,
                            length};
}

// The quick-check value of one code point (UAX #15 section 9 and the
// derivations tools/munigen/normalization.c checks against the UCD).
static muniQuickCheck QuickCheck(uint32_t codePoint, muniNormalForm form)
{
    uint32_t first;
    uint32_t second;
    bool canonical = muniCanonicalMapping(codePoint, &first, &second);
    switch (form)
    {
    case muni_nfd:
        return canonical ? muni_quickCheckNo : muni_quickCheckYes;
    case muni_nfkd:
        return canonical || muniUsesCompatibility(codePoint) ? muni_quickCheckNo
                                                             : muni_quickCheckYes;
    default:
        if ((canonical && muniIsCompositionExcluded(codePoint)) ||
            (form == muni_nfkc && muniUsesCompatibility(codePoint)))
        {
            return muni_quickCheckNo;
        }
        return muniCombinesBackward(codePoint) ? muni_quickCheckMaybe : muni_quickCheckYes;
    }
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
            *answerOut = muni_quickCheckNo;
            return (muniTextResult){muni_success, length};
        }
        answer = check == muni_quickCheckMaybe ? check : answer;
        lastClass = markClass;
    }
    *answerOut = answer;
    return (muniTextResult){muni_success, length};
}

bool muniDecomposePair(uint32_t codePoint, uint32_t* firstOut, uint32_t* secondOut)
{
    uint32_t first;
    uint32_t second;
    if (firstOut == nullptr || secondOut == nullptr ||
        !muniCanonicalMapping(codePoint, &first, &second))
    {
        return false;
    }
    *firstOut = first;
    *secondOut = second;
    return true;
}

uint32_t muniComposePair(uint32_t first, uint32_t second)
{
    return muniComposeCanonical(first, second);
}
