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
// segment holds is refused the same way in every form. Caseless
// comparison of two texts agrees with comparing their folded forms.

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
        muniGetNfkcCasefold(text, length, muni_convertReplace, out, MAX_OUTPUT, lengthOut);
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

// The folded form of text for a caseless match, through the calls that
// produce it into buffers: 1 when it is made, 0 when a segment is too
// long.
static int FoldedForm(const char* text, size_t length, muniCaselessMatch match, char* out,
                      size_t* lengthOut)
{
    static char decomposed[MAX_OUTPUT];
    static char folded[MAX_OUTPUT];
    size_t size = 0;
    if (match == muni_caselessDefault)
    {
        Require(muniConvertCase(text, length, muni_caseFold, muni_caseDefault, muni_convertReplace,
                                out, MAX_OUTPUT, lengthOut)
                    .status == muni_success);
        return 1;
    }
    if (match == muni_caselessIdentifier)
    {
        return Casefold(text, length, out, lengthOut) ? 1 : 0;
    }
    muniTextResult result =
        muniNormalize(text, length, muni_nfd, muni_convertReplace, decomposed, MAX_OUTPUT, &size);
    if (result.status == muni_errorLimit)
    {
        return 0;
    }
    Require(result.status == muni_success);
    Require(muniConvertCase(decomposed, size, muni_caseFold, muni_caseDefault, muni_convertStrict,
                            folded, MAX_OUTPUT, &size)
                .status == muni_success);
    result = muniNormalize(folded, size, muni_nfd, muni_convertStrict, out, MAX_OUTPUT, lengthOut);
    Require(result.status == muni_success || result.status == muni_errorLimit);
    return result.status == muni_success ? 1 : 0;
}

// muniCompareCaseless of the two halves of the input, split where its
// first byte says, and of each half with itself, in every match: the
// order of their folded forms in code point order. When a folded form
// cannot be made, the comparison may stop before reaching the segment
// that is too long, or refuse it.
static void CheckCaseless(const char* text, size_t length)
{
    static char firstForm[MAX_OUTPUT];
    static char secondForm[MAX_OUTPUT];
    size_t split = length == 0 ? 0 : (uint8_t)text[0] % (length + 1);
    const char* second = text + split;
    size_t secondLength = length - split;
    for (muniCaselessMatch match = 0; match <= muni_caselessIdentifier; match++)
    {
        size_t firstSize = 0;
        size_t secondSize = 0;
        bool made = FoldedForm(text, split, match, firstForm, &firstSize) == 1;
        made = FoldedForm(second, secondLength, match, secondForm, &secondSize) == 1 && made;
        int32_t order = 9;
        muniResult status = muniCompareCaseless(text, split, second, secondLength, match, &order);
        Require(status == muni_success || (status == muni_errorLimit && !made && order == 9));
        if (made)
        {
            size_t common = firstSize < secondSize ? firstSize : secondSize;
            int compared = memcmp(firstForm, secondForm, common);
            int32_t expected = compared != 0             ? (compared < 0 ? -1 : 1)
                               : firstSize == secondSize ? 0
                               : firstSize < secondSize  ? -1
                                                         : 1;
            Require(order == expected);
        }
        order = 9;
        status = muniCompareCaseless(second, secondLength, second, secondLength, match, &order);
        Require((status == muni_success && order == 0) || status == muni_errorLimit);
    }
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
    CheckCaseless(text, size);
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
        muniGetNfkcCasefold(text, size, muni_convertReplace, twice, capacity, &needed);
    Require(needed == first);
    Require(result.status == (capacity < first ? muni_errorCapacity : muni_success));
    Require(memcmp(twice, once, capacity) == 0);
    return 0;
}
