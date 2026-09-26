// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for case operations. The first input byte picks the
// language; the rest is the text, with ill-formed bytes replaced.
// Lowercasing, uppercasing and folding twice give what they give once;
// titlecasing need not, since it can turn a combining mark into a letter
// (U+0345 into capital iota), which moves the word boundaries it works
// on. For every operation, a short output buffer gets the same bytes, as
// many as fit, and the same needed size.

#include "maul-unicode/case.h"

#include <stdlib.h>
#include <string.h>

#define MAX_INPUT  1024
#define MAX_OUTPUT (MAX_INPUT * 9)

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static size_t Convert(const char* text, size_t length, muniCaseOperation operation,
                      muniCaseLanguage language, char* out)
{
    size_t needed = 0;
    muniTextResult result = muniConvertCase(text, length, operation, language, muni_convertReplace,
                                            out, MAX_OUTPUT, &needed);
    Require(result.status == muni_success);
    return needed;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size == 0 || size > MAX_INPUT)
    {
        return 0;
    }
    static char once[MAX_OUTPUT];
    static char twice[MAX_OUTPUT];
    muniCaseLanguage language = (muniCaseLanguage)(data[0] & 1);
    const char* text = (const char*)data + 1;
    size_t length = size - 1;
    for (muniCaseOperation operation = 0; operation <= muni_caseFold; operation++)
    {
        size_t first = Convert(text, length, operation, language, once);
        if (operation != muni_caseTitle)
        {
            size_t second = Convert(once, first, operation, language, twice);
            Require(first == second && memcmp(once, twice, first) == 0);
        }
        size_t capacity = first / 2;
        size_t needed = 0;
        muniTextResult result = muniConvertCase(text, length, operation, language,
                                                muni_convertReplace, twice, capacity, &needed);
        Require(needed == first);
        Require(result.status == (capacity < first ? muni_errorCapacity : muni_success));
        Require(memcmp(twice, once, capacity) == 0);
    }
    return 0;
}
