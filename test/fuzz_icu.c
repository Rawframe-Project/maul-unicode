// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target that compares the library with ICU, a test-only
// oracle never linked into the library. Every input is converted to
// UTF-16 with U+FFFD for each maximal ill-formed subpart, which both
// follow the Unicode Standard on. Well-formed input whose code points
// ICU's Unicode version assigns is also normalized to the four forms,
// which the normalization stability policy keeps the same across
// versions, and lowercased, uppercased and folded, by default and
// Turkic rules, unless a mapping reaches a character added since. A
// difference prints the operation and the input, then aborts, so
// libFuzzer keeps the input.

#include "maul-unicode/case.h"
#include "maul-unicode/normalize.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unicode/uchar.h>
#include <unicode/unorm2.h>
#include <unicode/ustring.h>

#define MAX_INPUT 512
#define MAX_UNITS (MAX_INPUT * 18 * 3)
#define MAX_BYTES (MAX_UNITS * 3)

static void Differ(const char* operation, const uint8_t* data, size_t size)
{
    fprintf(stderr, "%s differs from ICU for:", operation);
    for (size_t i = 0; i < size; i++)
    {
        fprintf(stderr, " %02X", data[i]);
    }
    fprintf(stderr, "\n");
    abort();
}

// ICU's result in UTF-16 as UTF-8, by ICU; false when it failed.
static bool IcuToUtf8(const UChar* units, int32_t length, char* bytes, int32_t* lengthOut)
{
    UErrorCode error = U_ZERO_ERROR;
    u_strToUTF8(bytes, MAX_BYTES, lengthOut, units, length, &error);
    return U_SUCCESS(error);
}

static bool Same(const char* ours, size_t ourLength, const char* theirs, int32_t theirLength)
{
    return ourLength == (size_t)theirLength && memcmp(ours, theirs, ourLength) == 0;
}

static void CompareConversion(const char* text, size_t length)
{
    static uint16_t ours[MAX_UNITS];
    static UChar theirs[MAX_UNITS];
    size_t needed = 0;
    muniTextResult result =
        muniConvertUtf8ToUtf16(text, length, ours, MAX_UNITS, muni_convertReplace, &needed);
    UErrorCode error = U_ZERO_ERROR;
    int32_t theirLength = 0;
    u_strFromUTF8WithSub(theirs, MAX_UNITS, &theirLength, text, (int32_t)length, 0xFFFD, nullptr,
                         &error);
    if (result.status != muni_success || U_FAILURE(error) || needed != (size_t)theirLength ||
        memcmp(ours, theirs, needed * sizeof(uint16_t)) != 0)
    {
        Differ("UTF-8 to UTF-16 with replacement", (const uint8_t*)text, length);
    }
}

// Whether ICU's Unicode version assigns every code point of the text.
static bool IcuKnows(const UChar* units, int32_t length)
{
    for (int32_t i = 0; i < length;)
    {
        UChar32 codePoint;
        U16_NEXT(units, i, length, codePoint);
        UVersionInfo age;
        u_charAge(codePoint, age);
        if (age[0] == 0 && age[1] == 0)
        {
            return false;
        }
    }
    return true;
}

static void CompareNormalization(const char* text, size_t length, const UChar* units,
                                 int32_t unitCount)
{
    static char ours[MAX_BYTES];
    static char theirs[MAX_BYTES];
    static UChar normalized[MAX_UNITS];
    static const char* const names[4] = {"NFC", "NFD", "NFKC", "NFKD"};
    for (muniNormalForm form = 0; form < 4; form++)
    {
        size_t needed = 0;
        muniTextResult result =
            muniNormalize(text, length, form, muni_convertStrict, ours, MAX_BYTES, &needed);
        if (result.status == muni_errorLimit)
        {
            return; // more marks than a segment holds, which ICU buffers
        }
        UErrorCode error = U_ZERO_ERROR;
        const UNormalizer2* normalizer = form == muni_nfc    ? unorm2_getNFCInstance(&error)
                                         : form == muni_nfd  ? unorm2_getNFDInstance(&error)
                                         : form == muni_nfkc ? unorm2_getNFKCInstance(&error)
                                                             : unorm2_getNFKDInstance(&error);
        int32_t count =
            unorm2_normalize(normalizer, units, unitCount, normalized, MAX_UNITS, &error);
        int32_t theirLength = 0;
        if (result.status != muni_success || U_FAILURE(error) ||
            !IcuToUtf8(normalized, count, theirs, &theirLength) ||
            !Same(ours, needed, theirs, theirLength))
        {
            Differ(names[form], (const uint8_t*)text, length);
        }
    }
}

// Whether ICU's version knows every code point of UTF-8 output: case
// mappings of old characters to new ones, such as U+019B to U+A7DC in
// Unicode 16, are changes since that version, not differences.
static bool OutputKnown(const char* bytes, size_t length)
{
    static UChar units[MAX_UNITS];
    UErrorCode error = U_ZERO_ERROR;
    int32_t count = 0;
    u_strFromUTF8(units, MAX_UNITS, &count, bytes, (int32_t)length, &error);
    return U_SUCCESS(error) && IcuKnows(units, count);
}

typedef int32_t IcuCaseFn(UChar* dest, int32_t capacity, const UChar* source, int32_t length,
                          const char* locale, UErrorCode* error);

static int32_t FoldDefault(UChar* dest, int32_t capacity, const UChar* source, int32_t length,
                           const char* locale, UErrorCode* error)
{
    (void)locale;
    return u_strFoldCase(dest, capacity, source, length, U_FOLD_CASE_DEFAULT, error);
}

static int32_t FoldTurkic(UChar* dest, int32_t capacity, const UChar* source, int32_t length,
                          const char* locale, UErrorCode* error)
{
    (void)locale;
    return u_strFoldCase(dest, capacity, source, length, U_FOLD_CASE_EXCLUDE_SPECIAL_I, error);
}

static void CompareCase(const char* text, size_t length, const UChar* units, int32_t unitCount)
{
    static char ours[MAX_BYTES];
    static char theirs[MAX_BYTES];
    static UChar mapped[MAX_UNITS];
    static const struct
    {
        const char* name;
        muniCaseOperation operation;
        muniCaseLanguage language;
        IcuCaseFn* icu;
        const char* locale;
    } s_cases[] = {
        {"lowercase", muni_caseLower, muni_caseDefault, u_strToLower, ""},
        {"uppercase", muni_caseUpper, muni_caseDefault, u_strToUpper, ""},
        {"fold", muni_caseFold, muni_caseDefault, FoldDefault, ""},
        {"Turkic lowercase", muni_caseLower, muni_caseTurkic, u_strToLower, "tr"},
        {"Turkic uppercase", muni_caseUpper, muni_caseTurkic, u_strToUpper, "tr"},
        {"Turkic fold", muni_caseFold, muni_caseTurkic, FoldTurkic, "tr"},
    };
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); i++)
    {
        size_t needed = 0;
        muniTextResult result =
            muniConvertCase(text, length, s_cases[i].operation, s_cases[i].language,
                            muni_convertStrict, ours, MAX_BYTES, &needed);
        UErrorCode error = U_ZERO_ERROR;
        int32_t count =
            s_cases[i].icu(mapped, MAX_UNITS, units, unitCount, s_cases[i].locale, &error);
        int32_t theirLength = 0;
        if (result.status == muni_success && !OutputKnown(ours, needed))
        {
            continue; // a mapping added since ICU's version
        }
        if (result.status != muni_success || U_FAILURE(error) ||
            !IcuToUtf8(mapped, count, theirs, &theirLength) ||
            !Same(ours, needed, theirs, theirLength))
        {
            Differ(s_cases[i].name, (const uint8_t*)text, length);
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size > MAX_INPUT)
    {
        return 0;
    }
    const char* text = (const char*)data;
    CompareConversion(text, size);
    static UChar units[MAX_UNITS];
    UErrorCode error = U_ZERO_ERROR;
    int32_t unitCount = 0;
    u_strFromUTF8(units, MAX_UNITS, &unitCount, text, (int32_t)size, &error);
    if (U_FAILURE(error) || !IcuKnows(units, unitCount))
    {
        return 0;
    }
    CompareNormalization(text, size, units, unitCount);
    CompareCase(text, size, units, unitCount);
    return 0;
}
