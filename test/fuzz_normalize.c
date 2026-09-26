// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for normalization. For any input, with ill-formed
// bytes replaced: every form is idempotent; NFD(NFC(x)) = NFD(x),
// NFC(NFD(x)) = NFC(x) and likewise for the compatibility forms; a quick
// check that answers Yes means the text is already normalized, and No
// that it is not; and a short output buffer gets the same bytes, as many
// as fit, and the same needed size.

#include "maul-unicode/normalize.h"

#include <stdlib.h>
#include <string.h>

#define MAX_INPUT  1024
#define MAX_OUTPUT (MAX_INPUT * 18 * 3)

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

// Normalizes into out; false when the input goes past the mark limit.
static bool Normalize(const char* text, size_t length, muniNormalForm form, char* out,
                      size_t* lengthOut)
{
    muniTextResult result =
        muniNormalize(text, length, form, muni_convertReplace, out, MAX_OUTPUT, lengthOut);
    Require(result.status == muni_success || result.status == muni_errorLimit);
    return result.status == muni_success;
}

static bool Same(const char* a, size_t aLength, const char* b, size_t bLength)
{
    return aLength == bLength && memcmp(a, b, aLength) == 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size > MAX_INPUT)
    {
        return 0;
    }
    static char forms[4][MAX_OUTPUT];
    static char again[MAX_OUTPUT];
    size_t lengths[4];
    const char* text = (const char*)data;
    for (muniNormalForm form = 0; form < 4; form++)
    {
        if (!Normalize(text, size, form, forms[form], &lengths[form]))
        {
            return 0;
        }
    }
    for (muniNormalForm form = 0; form < 4; form++)
    {
        size_t length = 0;
        Require(Normalize(forms[form], lengths[form], form, again, &length));
        Require(Same(again, length, forms[form], lengths[form]));
        // The decomposed form of the composed one, and the other way.
        muniNormalForm other = (muniNormalForm)(form ^ 1);
        Require(Normalize(forms[form], lengths[form], other, again, &length));
        Require(Same(again, length, forms[other], lengths[other]));
        muniQuickCheck answer = muni_quickCheckMaybe;
        muniTextResult check = muniCheckNormalization(forms[form], lengths[form], form, &answer);
        Require(check.status == muni_success && answer != muni_quickCheckNo);
    }
    // A short buffer.
    size_t capacity = lengths[0] / 2;
    size_t needed = 0;
    muniTextResult result =
        muniNormalize(text, size, muni_nfc, muni_convertReplace, again, capacity, &needed);
    Require(needed == lengths[0]);
    Require(result.status == (capacity < needed ? muni_errorCapacity : muni_success));
    Require(memcmp(again, forms[0], capacity) == 0);
    return 0;
}
