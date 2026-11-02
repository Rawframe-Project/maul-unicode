// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the encoding functions. Every input must give
// answers that agree with each other: validation and strict conversion
// report the same error at the same offset; valid text survives round
// trips through UTF-16 and UTF-32 byte for byte; replacement never
// fails; decoding step by step reaches the offset validation reports;
// into an output too small, each conversion stops at a code point.
// The same bytes are also read as UTF-16 units and as UTF-32 values,
// which may hold any surrogate or value past U+10FFFF.

#include "maul-unicode/encoding.h"
#include "maul-unicode/identifier.h"

#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

// Encoding a decoded code point gives its bytes back. Every capacity
// below its length is refused with the length and writes nothing; a
// capacity of exactly the length writes nothing past it.
static void CheckEncode(uint32_t codePoint, const char* bytes, size_t size)
{
    for (size_t capacity = 0; capacity <= size; capacity++)
    {
        char output[4] = {'x', 'x', 'x', 'x'};
        size_t sizeOut = 0;
        muniResult status = muniEncodeUtf8(codePoint, output, capacity, &sizeOut);
        Require(sizeOut == size);
        Require(status == (capacity < size ? muni_errorCapacity : muni_success));
        for (size_t i = 0; i < 4; i++)
        {
            Require(i < size && status == muni_success ? output[i] == bytes[i] : output[i] == 'x');
        }
    }
}

// muniCheckIdentifier reports the first problem in the text, a UTF-8
// error or a code point that may not stand where it is, as a walk with
// muniDecodeUtf8 and the identifier properties finds it.
static void CheckIdentifier(const char* text, size_t length)
{
    muniTextResult expected = {muni_errorIdentifier, 0};
    if (length > 0)
    {
        expected = (muniTextResult){muni_success, length};
    }
    size_t offset = 0;
    while (offset < length && expected.status == muni_success)
    {
        uint32_t codePoint;
        size_t size;
        muniResult status = muniDecodeUtf8(text + offset, length - offset, &codePoint, &size);
        if (status != muni_success)
        {
            expected = (muniTextResult){status, offset};
        }
        else if (offset == 0 ? !muniIsIdentifierStart(codePoint)
                             : !muniIsIdentifierContinue(codePoint))
        {
            expected = (muniTextResult){muni_errorIdentifier, offset};
        }
        offset += size;
    }
    muniTextResult result = muniCheckIdentifier(text, length);
    Require(result.status == expected.status && result.offset == expected.offset);
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
        CheckEncode(codePoint, text + offset, size);
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

// A conversion in replace mode, with the size of its input and output
// units and the units the code point at the start of its input takes.
typedef struct
{
    muniTextResult (*convert)(const void* input, size_t length, void* output, size_t capacity,
                              size_t* neededOut);
    size_t inputSize;
    size_t outputSize;
    size_t (*step)(const void* input, size_t length);
} Conversion;

static muniTextResult Utf8ToUtf16(const void* input, size_t length, void* output, size_t capacity,
                                  size_t* neededOut)
{
    return muniConvertUtf8ToUtf16(input, length, muni_convertReplace, output, capacity, neededOut);
}

static muniTextResult Utf8ToUtf32(const void* input, size_t length, void* output, size_t capacity,
                                  size_t* neededOut)
{
    return muniConvertUtf8ToUtf32(input, length, muni_convertReplace, output, capacity, neededOut);
}

static muniTextResult Utf16ToUtf8(const void* input, size_t length, void* output, size_t capacity,
                                  size_t* neededOut)
{
    return muniConvertUtf16ToUtf8(input, length, muni_convertReplace, output, capacity, neededOut);
}

static muniTextResult Utf32ToUtf8(const void* input, size_t length, void* output, size_t capacity,
                                  size_t* neededOut)
{
    return muniConvertUtf32ToUtf8(input, length, muni_convertReplace, output, capacity, neededOut);
}

static size_t StepUtf8(const void* input, size_t length)
{
    uint32_t codePoint;
    size_t size = 0;
    (void)muniDecodeUtf8(input, length, &codePoint, &size);
    return size;
}

static size_t StepUtf16(const void* input, size_t length)
{
    uint32_t codePoint;
    size_t size = 0;
    (void)muniDecodeUtf16(input, length, &codePoint, &size);
    return size;
}

static size_t StepUtf32(const void* input, size_t length)
{
    (void)input;
    (void)length;
    return 1;
}

static const Conversion s_utf8ToUtf16 = {Utf8ToUtf16, 1, sizeof(uint16_t), StepUtf8};
static const Conversion s_utf8ToUtf32 = {Utf8ToUtf32, 1, sizeof(uint32_t), StepUtf8};
static const Conversion s_utf16ToUtf8 = {Utf16ToUtf8, sizeof(uint16_t), 1, StepUtf16};
static const Conversion s_utf32ToUtf8 = {Utf32ToUtf8, sizeof(uint32_t), 1, StepUtf32};

// Into an output too small, a conversion stops at a code point: it
// reports the whole size and the offset of the first code point not
// written, writes the conversion of the input before that offset and
// nothing after it, and the code point at the offset would not have fit.
// Each output is a heap allocation of exactly its capacity.
static void CheckStops(const Conversion* conversion, const void* input, size_t length, uint8_t salt)
{
    size_t needed = 0;
    (void)conversion->convert(input, length, nullptr, 0, &needed);
    uint8_t* whole = Allocate(conversion->outputSize * needed);
    size_t written = 0;
    Require(conversion->convert(input, length, whole, needed, &written).status == muni_success &&
            written == needed);
    for (size_t k = 0; k < 3; k++)
    {
        size_t capacity = (salt * 31u + k * (needed / 3 + 1)) % (needed + 1);
        size_t bytes = conversion->outputSize * capacity;
        uint8_t* output = Allocate(bytes);
        memset(output, 0xA5, bytes);
        size_t total = 0;
        muniTextResult result = conversion->convert(input, length, output, capacity, &total);
        Require(result.status == (capacity < needed ? muni_errorCapacity : muni_success) &&
                total == needed && result.offset <= length);
        size_t prefix = 0;
        (void)conversion->convert(input, result.offset, nullptr, 0, &prefix);
        size_t prefixBytes = conversion->outputSize * prefix;
        Require(prefix <= capacity && memcmp(output, whole, prefixBytes) == 0);
        for (size_t i = prefixBytes; i < bytes; i++)
        {
            Require(output[i] == 0xA5);
        }
        if (result.offset < length)
        {
            const uint8_t* rest = (const uint8_t*)input + conversion->inputSize * result.offset;
            size_t next = result.offset + conversion->step(rest, length - result.offset);
            size_t more = 0;
            (void)conversion->convert(input, next, nullptr, 0, &more);
            Require(more > capacity);
        }
        free(output);
    }
    free(whole);
}

// Stepping through UTF-16 units with muniDecodeUtf16 meets its first
// unpaired surrogate where validation stops, and gives, code point by
// code point, what replacing conversion writes.
static void CheckUtf16Stepping(const uint16_t* units, size_t count, muniTextResult validation,
                               const char* replaced, size_t length)
{
    size_t offset = 0;
    size_t written = 0;
    bool refused = false;
    while (offset < count)
    {
        uint32_t codePoint;
        size_t size;
        muniResult status = muniDecodeUtf16(units + offset, count - offset, &codePoint, &size);
        Require(size >= 1 && size <= 2 && offset + size <= count);
        Require(status == muni_success || status == muni_errorUtf16Surrogate);
        if (status != muni_success && !refused)
        {
            Require(validation.status == status && validation.offset == offset);
            refused = true;
        }
        char encoded[4];
        size_t encodedSize = 0;
        Require(muniEncodeUtf8(codePoint, encoded, sizeof(encoded), &encodedSize) == muni_success);
        Require(written + encodedSize <= length &&
                memcmp(replaced + written, encoded, encodedSize) == 0);
        written += encodedSize;
        offset += size;
    }
    Require(written == length && (refused || validation.status == muni_success));
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
    CheckUtf16Stepping(units, count, validation, replaced, length);
    CheckStops(&s_utf16ToUtf8, units, count, size == 0 ? 0 : data[0]);

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
    CheckStops(&s_utf32ToUtf8, values, count, size == 0 ? 0 : data[0]);
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
    CheckIdentifier(text, size);
    uint8_t salt = size == 0 ? 0 : data[0];
    CheckStops(&s_utf8ToUtf16, text, size, salt);
    CheckStops(&s_utf8ToUtf32, text, size, salt);

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
