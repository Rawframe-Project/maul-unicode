// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the encoding functions. Every input must give
// answers that agree with each other: validation and strict conversion
// report the same error at the same offset; valid text survives round
// trips through UTF-16 and UTF-32 byte for byte; replacement never
// fails; decoding step by step reaches the offset validation reports.
// The same bytes are also read as UTF-16 units and as UTF-32 values,
// which may hold any surrogate or value past U+10FFFF.

#include "maul-unicode/encoding.h"

#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static void CheckStepping(const char* text, size_t length, muniTextResult validation)
{
    size_t offset = 0;
    while (offset < length)
    {
        uint32_t codePoint;
        size_t size;
        muniResult status = muniDecodeUtf8(text + offset, length - offset, &codePoint, &size);
        Require(size >= 1 && size <= 4 && offset + size <= length);
        if (status != muni_success)
        {
            Require(validation.status == status && validation.offset == offset);
            return;
        }
        offset += size;
    }
    Require(validation.status == muni_success);
}

// A heap allocation of exactly bytes, or of one byte for none, so
// AddressSanitizer sees an access past the end.
static void* Allocate(size_t bytes)
{
    return malloc(bytes == 0 ? 1 : bytes);
}

// The input read as UTF-16 units, which may hold any surrogate: strict
// conversion fails exactly where validation does, with the bytes of the
// units before; replacement never fails, gives valid UTF-8, and gives the
// strict output for valid units, which convert back to themselves. Every
// output is a heap allocation of exactly the size measured, so a write
// past it is seen.
static void CheckUtf16(const uint8_t* data, size_t size)
{
    size_t count = size / 2;
    uint16_t* units = Allocate(sizeof(uint16_t) * count);
    memcpy(units, data, sizeof(uint16_t) * count);
    muniTextResult validation = muniValidateUtf16(units, count);
    Require(validation.status == muni_success || validation.status == muni_errorUtf16Surrogate);
    size_t length = 0;
    muniTextResult measured =
        muniConvertUtf16ToUtf8(units, count, muni_convertReplace, nullptr, 0, &length);
    Require(measured.status == (length == 0 ? muni_success : muni_errorCapacity));
    char* replaced = Allocate(length);
    size_t written = 0;
    Require(muniConvertUtf16ToUtf8(units, count, muni_convertReplace, replaced, length, &written)
                    .status == muni_success &&
            written == length);
    Require(muniValidateUtf8(replaced, length).status == muni_success);

    char* strict = Allocate(length);
    size_t strictLength = 0;
    muniTextResult converted =
        muniConvertUtf16ToUtf8(units, count, muni_convertStrict, strict, length, &strictLength);
    Require(converted.status == validation.status && converted.offset == validation.offset);
    if (validation.status == muni_success)
    {
        Require(strictLength == length && (length == 0 || memcmp(strict, replaced, length) == 0));
        uint16_t* back = Allocate(sizeof(uint16_t) * count);
        size_t backCount = 0;
        Require(
            muniConvertUtf8ToUtf16(replaced, length, muni_convertStrict, back, count, &backCount)
                    .status == muni_success &&
            backCount == count &&
            (count == 0 || memcmp(back, units, sizeof(uint16_t) * count) == 0));
        free(back);
    }
    else
    {
        size_t prefixLength = 0;
        Require(muniConvertUtf16ToUtf8(units, validation.offset, muni_convertStrict, nullptr, 0,
                                       &prefixLength)
                    .status == (prefixLength == 0 ? muni_success : muni_errorCapacity));
        Require(strictLength == prefixLength &&
                (prefixLength == 0 || memcmp(strict, replaced, prefixLength) == 0));
    }
    free(strict);
    free(replaced);
    free(units);
}

// The input read as UTF-32 values, which may be surrogates or past
// U+10FFFF: strict conversion fails at the first such value; replacement
// never fails, and reading its output back gives each value, or U+FFFD in
// place of each bad one.
static void CheckUtf32(const uint8_t* data, size_t size)
{
    size_t count = size / 4;
    uint32_t* values = Allocate(sizeof(uint32_t) * count);
    memcpy(values, data, sizeof(uint32_t) * count);
    size_t firstBad = count;
    for (size_t i = 0; i < count && firstBad == count; i++)
    {
        if (values[i] > 0x10FFFF || (values[i] >= 0xD800 && values[i] <= 0xDFFF))
        {
            firstBad = i;
        }
    }
    size_t length = 0;
    muniTextResult measured =
        muniConvertUtf32ToUtf8(values, count, muni_convertReplace, nullptr, 0, &length);
    Require(measured.status == (length == 0 ? muni_success : muni_errorCapacity));
    char* replaced = Allocate(length);
    size_t written = 0;
    Require(muniConvertUtf32ToUtf8(values, count, muni_convertReplace, replaced, length, &written)
                    .status == muni_success &&
            written == length);
    size_t strictLength = 0;
    muniTextResult strict =
        muniConvertUtf32ToUtf8(values, count, muni_convertStrict, nullptr, 0, &strictLength);
    if (firstBad == count)
    {
        Require(strict.status == (length == 0 ? muni_success : muni_errorCapacity) &&
                strictLength == length);
    }
    else
    {
        Require(strict.status == muni_errorUtf32Value && strict.offset == firstBad);
    }
    uint32_t* back = Allocate(sizeof(uint32_t) * count);
    size_t backCount = 0;
    Require(muniConvertUtf8ToUtf32(replaced, length, muni_convertStrict, back, count, &backCount)
                    .status == muni_success &&
            backCount == count);
    for (size_t i = 0; i < count; i++)
    {
        bool bad = values[i] > 0x10FFFF || (values[i] >= 0xD800 && values[i] <= 0xDFFF);
        Require(back[i] == (bad ? 0xFFFDu : values[i]));
    }
    free(back);
    free(replaced);
    free(values);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const char* text = (const char*)data;
    muniTextResult validation = muniValidateUtf8(text, size);
    CheckStepping(text, size, validation);

    uint16_t* units = malloc(sizeof(uint16_t) * (size + 1));
    size_t unitCount = 0;
    muniTextResult strict =
        muniConvertUtf8ToUtf16(text, size, muni_convertStrict, units, size, &unitCount);
    Require(strict.status == validation.status && strict.offset == validation.offset);

    if (validation.status == muni_success)
    {
        char* back = malloc(size + 1);
        size_t byteCount = 0;
        muniTextResult reverse =
            muniConvertUtf16ToUtf8(units, unitCount, muni_convertStrict, back, size, &byteCount);
        Require(reverse.status == muni_success && byteCount == size);
        Require(size == 0 || memcmp(back, text, size) == 0);
        free(back);
    }

    uint32_t* codePoints = malloc(sizeof(uint32_t) * (size + 1));
    size_t codePointCount = 0;
    muniTextResult replaced =
        muniConvertUtf8ToUtf32(text, size, muni_convertReplace, codePoints, size, &codePointCount);
    Require(replaced.status == muni_success && codePointCount <= size);
    for (size_t i = 0; i < codePointCount; i++)
    {
        Require(codePoints[i] <= 0x10FFFF && (codePoints[i] < 0xD800 || codePoints[i] > 0xDFFF));
    }
    // The replaced code points back in UTF-8 are the text itself when it
    // is valid, and at most three bytes per input byte otherwise.
    char* again = malloc(3 * size + 1);
    size_t againCount = 0;
    muniTextResult back = muniConvertUtf32ToUtf8(codePoints, codePointCount, muni_convertStrict,
                                                 again, 3 * size, &againCount);
    Require(back.status == muni_success && againCount <= 3 * size);
    Require(validation.status != muni_success ||
            (againCount == size && (size == 0 || memcmp(again, text, size) == 0)));
    free(again);
    free(codePoints);
    free(units);
    CheckUtf16(data, size);
    CheckUtf32(data, size);
    return 0;
}
