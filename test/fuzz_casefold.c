// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for NFKC_Casefold, over text with ill-formed bytes
// replaced. Its result is in NFC and is its own NFKC_Casefold, and text
// gets the same result as its NFD and its NFKC: NFKC_Casefold keys a
// string by what canonical and compatibility equivalence leave. The one
// exception is U+0345 COMBINING GREEK YPOGEGRAMMENI, which folds to an
// iota, a starter: in "U+1FB3 U+0301" the acute follows the iota and
// composes with it, while the NFD of the same text puts the acute before
// U+0345, on the alpha. Its per code point definition gives the two
// different keys (ICU agrees), so text whose NFKD has U+0345 is left out
// of that check. A short output buffer gets the same bytes, as many as fit, and
// the same needed size. Text with more combining marks in a row than a
// segment holds is refused the same way in every form.

#include "maul-unicode/case.h"
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

// NFKC_Casefold of text into out; false when a run of marks is too long.
static bool Casefold(const char* text, size_t length, char* out, size_t* lengthOut)
{
    muniTextResult result =
        muniToNfkcCasefold(text, length, muni_convertReplace, out, MAX_OUTPUT, lengthOut);
    Require(result.status == muni_success || result.status == muni_errorLimit);
    return result.status == muni_success;
}

// Requires that a form of text has the same NFKC_Casefold as key.
static void RequireSameKey(const char* text, size_t length, muniNormalForm form, const char* key,
                           size_t keyLength)
{
    static char normalized[MAX_OUTPUT];
    static char again[MAX_OUTPUT];
    size_t normalizedLength = 0;
    size_t againLength = 0;
    if (muniNormalize(text, length, form, muni_convertReplace, normalized, MAX_OUTPUT,
                      &normalizedLength)
                .status != muni_success ||
        !Casefold(normalized, normalizedLength, again, &againLength))
    {
        return;
    }
    Require(againLength == keyLength && memcmp(again, key, keyLength) == 0);
}

// Whether the NFKD of text has U+0345, which U+037A GREEK
// YPOGEGRAMMENI brings in too.
static bool HasYpogegrammeni(const char* text, size_t length)
{
    static char decomposed[MAX_OUTPUT];
    size_t decomposedLength = 0;
    if (muniNormalize(text, length, muni_nfkd, muni_convertReplace, decomposed, MAX_OUTPUT,
                      &decomposedLength)
            .status != muni_success)
    {
        return true;
    }
    for (size_t i = 0; i + 1 < decomposedLength; i++)
    {
        if ((uint8_t)decomposed[i] == 0xCD && (uint8_t)decomposed[i + 1] == 0x85)
        {
            return true;
        }
    }
    return false;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size > MAX_INPUT)
    {
        return 0;
    }
    static char once[MAX_OUTPUT];
    static char twice[MAX_OUTPUT];
    const char* text = (const char*)data;
    size_t first = 0;
    if (!Casefold(text, size, once, &first))
    {
        return 0;
    }
    size_t second = 0;
    Require(Casefold(once, first, twice, &second));
    Require(first == second && memcmp(once, twice, first) == 0);
    Require(muniNormalize(once, first, muni_nfc, muni_convertStrict, twice, MAX_OUTPUT, &second)
                    .status == muni_success &&
            first == second && memcmp(once, twice, first) == 0);
    if (!HasYpogegrammeni(text, size))
    {
        RequireSameKey(text, size, muni_nfd, once, first);
        RequireSameKey(text, size, muni_nfkc, once, first);
    }
    size_t capacity = first / 2;
    size_t needed = 0;
    muniTextResult result =
        muniToNfkcCasefold(text, size, muni_convertReplace, twice, capacity, &needed);
    Require(needed == first);
    Require(result.status == (capacity < first ? muni_errorCapacity : muni_success));
    Require(memcmp(twice, once, capacity) == 0);
    return 0;
}
