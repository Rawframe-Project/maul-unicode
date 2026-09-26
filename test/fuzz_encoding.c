// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the encoding functions. Every input must give
// answers that agree with each other: validation and strict conversion
// report the same error at the same offset; valid text survives a round
// trip through UTF-16 byte for byte; replacement never fails; decoding
// step by step reaches the offset validation reports.

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

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const char* text = (const char*)data;
    muniTextResult validation = muniValidateUtf8(text, size);
    CheckStepping(text, size, validation);

    uint16_t* units = malloc(sizeof(uint16_t) * (size + 1));
    size_t unitCount = 0;
    muniTextResult strict =
        muniConvertUtf8ToUtf16(text, size, units, size, muni_convertStrict, &unitCount);
    Require(strict.status == validation.status && strict.offset == validation.offset);

    if (validation.status == muni_success)
    {
        char* back = malloc(size + 1);
        size_t byteCount = 0;
        muniTextResult reverse =
            muniConvertUtf16ToUtf8(units, unitCount, back, size, muni_convertStrict, &byteCount);
        Require(reverse.status == muni_success && byteCount == size);
        Require(size == 0 || memcmp(back, text, size) == 0);
        free(back);
    }

    uint32_t* codePoints = malloc(sizeof(uint32_t) * (size + 1));
    size_t codePointCount = 0;
    muniTextResult replaced =
        muniConvertUtf8ToUtf32(text, size, codePoints, size, muni_convertReplace, &codePointCount);
    Require(replaced.status == muni_success && codePointCount <= size);
    for (size_t i = 0; i < codePointCount; i++)
    {
        Require(codePoints[i] <= 0x10FFFF && (codePoints[i] < 0xD800 || codePoints[i] > 0xDFFF));
    }
    free(codePoints);
    free(units);
    return 0;
}
