// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the security checks. The first input byte picks
// the paragraph direction; the rest is the text. A skeleton is the same
// through a short output buffer, as many bytes as fit, and is valid
// UTF-8; one that neither bidi nor its source reorders is its own
// skeleton. A restriction level is one of the six, ASCII only exactly
// for ASCII text in the profile, and single script or stricter exactly
// when the resolved scripts, which always fit in 64, are not empty.

#include "maul-unicode/security.h"

#include <stdlib.h>
#include <string.h>

#define MAX_INPUT  1024
#define MAX_OUTPUT (MAX_INPUT * 18 * 4)

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

// Whether text displays in logical order in a left-to-right paragraph.
static bool IsLeftToRight(const char* text, size_t length)
{
    size_t offset = 0;
    while (offset < length)
    {
        uint32_t codePoint;
        size_t size;
        (void)muniDecodeUtf8(text + offset, length - offset, &codePoint, &size);
        muniBidiClass type = muniGetBidiClass(codePoint);
        if (type == muni_bcR || type == muni_bcAl || type == muni_bcRle || type == muni_bcRlo ||
            type == muni_bcRli)
        {
            return false;
        }
        offset += size;
    }
    return true;
}

static void CheckSkeleton(const char* text, size_t length, muniBidiDirection direction)
{
    static uint8_t workspace[2 * MAX_OUTPUT];
    static char once[MAX_OUTPUT];
    static char twice[MAX_OUTPUT];
    size_t first = 0;
    muniTextResult result =
        muniGetSkeleton(text, length, direction, workspace, once, MAX_OUTPUT, &first);
    if (result.status == muni_errorLimit)
    {
        return;
    }
    Require(result.status == muni_success);
    Require(muniValidateUtf8(once, first).status == muni_success);
    size_t capacity = first / 2;
    size_t needed = 0;
    result = muniGetSkeleton(text, length, direction, workspace, twice, capacity, &needed);
    Require(needed == first && memcmp(twice, once, capacity) == 0);
    Require(result.status == (capacity < first ? muni_errorCapacity : muni_success));
    if (direction != muni_bidiRightToLeft && IsLeftToRight(text, length) &&
        IsLeftToRight(once, first))
    {
        result = muniGetSkeleton(once, first, direction, workspace, twice, MAX_OUTPUT, &needed);
        Require(result.status == muni_success && needed == first &&
                memcmp(once, twice, first) == 0);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size == 0 || size > MAX_INPUT)
    {
        return 0;
    }
    const char* text = (const char*)data + 1;
    size_t length = size - 1;
    if (muniValidateUtf8(text, length).status != muni_success)
    {
        return 0;
    }
    CheckSkeleton(text, length, (muniBidiDirection)(data[0] % 3));
    muniRestrictionLevel level = 0;
    Require(muniGetRestrictionLevel(text, length, &level).status == muni_success);
    Require(level >= muni_restrictionAsciiOnly && level <= muni_restrictionUnrestricted);
    bool ascii = true;
    for (size_t i = 0; i < length; i++)
    {
        ascii = ascii && (uint8_t)text[i] < 0x80;
    }
    Require((level == muni_restrictionAsciiOnly) ==
            (ascii && level != muni_restrictionUnrestricted));
    muniScript scripts[64];
    size_t count = 0;
    Require(muniGetResolvedScripts(text, length, scripts, 64, &count).status == muni_success);
    Require(level == muni_restrictionUnrestricted ||
            (count > 0) == (level <= muni_restrictionSingleScript));
    bool mixed = false;
    Require(muniCheckMixedNumbers(text, length, &mixed).status == muni_success);
    return 0;
}
