// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// UTF-8, UTF-16 and UTF-32. Validation is compared with an independent
// reference over every one-, two- and three-byte string and a systematic
// set of four-byte strings. The reference decodes by bit layout and then
// checks the value (overlong, surrogate, past U+10FFFF), where the
// library checks byte ranges.

#include "test_harness.h"

#include "maul-unicode/encoding.h"

#include <string.h>

// The reference: 1 when bytes[0..length) is exactly one well-formed
// sequence, and its value.
static int ReferenceOne(const uint8_t* bytes, size_t length, uint32_t* valueOut)
{
    size_t expected = bytes[0] < 0x80             ? 1
                      : (bytes[0] & 0xE0) == 0xC0 ? 2
                      : (bytes[0] & 0xF0) == 0xE0 ? 3
                      : (bytes[0] & 0xF8) == 0xF0 ? 4
                                                  : 0;
    if (expected != length)
    {
        return 0;
    }
    uint32_t value = expected == 1 ? bytes[0] : bytes[0] & (0x7Fu >> expected);
    for (size_t i = 1; i < length; i++)
    {
        if ((bytes[i] & 0xC0) != 0x80)
        {
            return 0;
        }
        value = (value << 6) | (bytes[i] & 0x3Fu);
    }
    static const uint32_t minimum[5] = {0, 0, 0x80, 0x800, 0x10000};
    if (value < minimum[length] || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
    {
        return 0;
    }
    *valueOut = value;
    return 1;
}

// 1 when bytes[0..length) is a prefix of some well-formed sequence: some
// completion with continuation bytes makes it one.
static int ReferencePrefix(const uint8_t* bytes, size_t length)
{
    uint8_t candidate[4];
    memcpy(candidate, bytes, length);
    for (size_t total = length; total <= 4; total++)
    {
        size_t missing = total - length;
        uint32_t combinations = missing == 0 ? 1 : 1u << (6 * missing);
        for (uint32_t c = 0; c < combinations; c++)
        {
            for (size_t i = 0; i < missing; i++)
            {
                candidate[length + i] = (uint8_t)(0x80 | ((c >> (6 * i)) & 0x3F));
            }
            uint32_t value;
            if (ReferenceOne(candidate, total, &value))
            {
                return 1;
            }
        }
    }
    return 0;
}

// The reference validation of a whole string: the offset of the first
// error, or length.
static size_t ReferenceFirstError(const uint8_t* bytes, size_t length)
{
    size_t offset = 0;
    while (offset < length)
    {
        size_t size = 0;
        uint32_t value;
        for (size_t k = 1; k <= 4 && offset + k <= length; k++)
        {
            if (ReferenceOne(bytes + offset, k, &value))
            {
                size = k;
                break;
            }
        }
        if (size == 0)
        {
            return offset;
        }
        offset += size;
    }
    return length;
}

static int s_mismatches = 0;

static void Compare(const uint8_t* bytes, size_t length)
{
    muniTextResult result = muniValidateUtf8((const char*)bytes, length);
    size_t expected = ReferenceFirstError(bytes, length);
    bool agree =
        (result.status == muni_success) == (expected == length) && result.offset == expected;
    if (!agree && s_mismatches < 8)
    {
        printf(
            "mismatch: length %zu, bytes %02X %02X %02X %02X, library %d at %zu, reference %zu\n",
            length, bytes[0], length > 1 ? bytes[1] : 0, length > 2 ? bytes[2] : 0,
            length > 3 ? bytes[3] : 0, result.status, result.offset, expected);
    }
    s_mismatches += agree ? 0 : 1;
}

static void TestValidationAgreesOnEveryShortString(void)
{
    uint8_t bytes[4];
    for (uint32_t a = 0; a < 256; a++)
    {
        bytes[0] = (uint8_t)a;
        Compare(bytes, 1);
        for (uint32_t b = 0; b < 256; b++)
        {
            bytes[1] = (uint8_t)b;
            Compare(bytes, 2);
            for (uint32_t c = 0; c < 256; c++)
            {
                bytes[2] = (uint8_t)c;
                Compare(bytes, 3);
            }
        }
    }
    CHECK(s_mismatches == 0, "every string of up to three bytes");
}

static void TestValidationAgreesOnFourByteStrings(void)
{
    static const uint8_t edges[] = {0x00, 0x7F, 0x80, 0x8F, 0x90, 0x9F,
                                    0xA0, 0xBF, 0xC0, 0xF4, 0xFF};
    uint8_t bytes[4];
    s_mismatches = 0;
    for (uint32_t a = 0xE0; a < 256; a++)
    {
        for (uint32_t b = 0; b < 256; b++)
        {
            for (size_t c = 0; c < sizeof(edges); c++)
            {
                for (size_t d = 0; d < sizeof(edges); d++)
                {
                    bytes[0] = (uint8_t)a;
                    bytes[1] = (uint8_t)b;
                    bytes[2] = edges[c];
                    bytes[3] = edges[d];
                    Compare(bytes, 4);
                }
            }
        }
    }
    CHECK(s_mismatches == 0, "four-byte strings over every lead and second byte");
}

static void TestMaximalSubpartsMatchTheReference(void)
{
    int mismatches = 0;
    uint8_t bytes[4] = {0};
    for (uint32_t a = 0x80; a < 256; a++)
    {
        for (uint32_t b = 0; b < 256; b++)
        {
            bytes[0] = (uint8_t)a;
            bytes[1] = (uint8_t)b;
            bytes[2] = 0x41; // an ASCII byte that ends any sequence
            uint32_t codePoint;
            size_t size;
            if (muniDecodeUtf8((const char*)bytes, 3, &codePoint, &size) == muni_success)
            {
                continue;
            }
            size_t expected = ReferencePrefix(bytes, 2) ? 2 : 1;
            mismatches += size == expected ? 0 : 1;
        }
    }
    CHECK(mismatches == 0, "maximal subpart length after each lead and second byte");
}

static void TestErrorKinds(void)
{
    CHECK(muniValidateUtf8("\x80", 1).status == muni_errorUtf8Lead, "lone continuation");
    CHECK(muniValidateUtf8("\xC0\x80", 2).status == muni_errorUtf8Overlong, "C0 80");
    CHECK(muniValidateUtf8("\xE0\x80\x80", 3).status == muni_errorUtf8Overlong, "E0 80 80");
    CHECK(muniValidateUtf8("\xED\xA0\x80", 3).status == muni_errorUtf8Surrogate, "ED A0 80");
    CHECK(muniValidateUtf8("\xF4\x90\x80\x80", 4).status == muni_errorUtf8TooLarge, "F4 90");
    CHECK(muniValidateUtf8("\xF5\x80\x80\x80", 4).status == muni_errorUtf8TooLarge, "F5");
    CHECK(muniValidateUtf8("\xFF", 1).status == muni_errorUtf8Lead, "FF");
    CHECK(muniValidateUtf8("\xE2\x82", 2).status == muni_errorUtf8Truncated, "cut euro sign");
    CHECK(muniValidateUtf8("\xE2\x41\x41", 3).status == muni_errorUtf8Continuation, "E2 41");
    muniTextResult result = muniValidateUtf8("abc\xE2\x82\xAC\xC3", 7);
    CHECK(result.status == muni_errorUtf8Truncated && result.offset == 6, "offset of the error");
    CHECK(muniValidateUtf8(nullptr, 0).status == muni_success, "empty text");
    CHECK(muniValidateUtf8(nullptr, 1).status == muni_errorInvalid, "NULL with a length");
}

static void TestReplacementFollowsTheStandard(void)
{
    // The example of U+FFFD substitution in chapter 3 of the Unicode
    // Standard: 61 F1 80 80 E1 80 C2 62 80 63 80 BF 64.
    const char text[] = "\x61\xF1\x80\x80\xE1\x80\xC2\x62\x80\x63\x80\xBF\x64";
    const uint32_t expected[] = {0x61,   0xFFFD, 0xFFFD, 0xFFFD, 0x62,
                                 0xFFFD, 0x63,   0xFFFD, 0xFFFD, 0x64};
    uint32_t output[16];
    size_t needed = 0;
    muniTextResult result =
        muniConvertUtf8ToUtf32(text, sizeof(text) - 1, output, 16, muni_convertReplace, &needed);
    CHECK(result.status == muni_success && needed == 10, "ten code points");
    CHECK(memcmp(output, expected, sizeof(expected)) == 0, "the standard's substitution");
}

static void TestConversionsRoundTrip(void)
{
    const char text[] = "a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80z"; // a, e acute, euro, emoji, z
    uint16_t units[16];
    size_t unitCount = 0;
    muniTextResult toUtf16 =
        muniConvertUtf8ToUtf16(text, sizeof(text) - 1, units, 16, muni_convertStrict, &unitCount);
    CHECK(toUtf16.status == muni_success && unitCount == 6, "six UTF-16 units");
    CHECK(units[3] == 0xD83D && units[4] == 0xDE00, "the emoji as a surrogate pair");
    char back[32];
    size_t byteCount = 0;
    muniTextResult toUtf8 =
        muniConvertUtf16ToUtf8(units, unitCount, back, 32, muni_convertStrict, &byteCount);
    CHECK(toUtf8.status == muni_success && byteCount == sizeof(text) - 1, "same length back");
    CHECK(memcmp(back, text, byteCount) == 0, "same bytes back");
}

static void TestCapacityReportsTheTotal(void)
{
    const char text[] = "\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC"; // three euro signs
    uint16_t units[2];
    size_t needed = 0;
    muniTextResult result =
        muniConvertUtf8ToUtf16(text, sizeof(text) - 1, units, 2, muni_convertStrict, &needed);
    CHECK(result.status == muni_errorCapacity && needed == 3, "capacity, three needed");
    CHECK(units[0] == 0x20AC && units[1] == 0x20AC, "the units that fit are written");
    result =
        muniConvertUtf8ToUtf16(text, sizeof(text) - 1, nullptr, 0, muni_convertStrict, &needed);
    CHECK(result.status == muni_errorCapacity && needed == 3, "measuring with a NULL buffer");
}

static void TestUtf16Surrogates(void)
{
    const uint16_t lone[] = {0x0061, 0xDC00, 0x0062};
    muniTextResult result = muniValidateUtf16(lone, 3);
    CHECK(result.status == muni_errorUtf16Surrogate && result.offset == 1, "lone low surrogate");
    char bytes[16];
    size_t needed = 0;
    result = muniConvertUtf16ToUtf8(lone, 3, bytes, 16, muni_convertReplace, &needed);
    CHECK(result.status == muni_success && needed == 5, "replaced by U+FFFD");
    CHECK(memcmp(bytes,
                 "a\xEF\xBF\xBD"
                 "b",
                 5) == 0,
          "a, U+FFFD, b");
}

static void TestEncode(void)
{
    char bytes[4];
    size_t size = 0;
    CHECK(muniEncodeUtf8(0x1F600, bytes, &size) == muni_success && size == 4, "four bytes");
    CHECK(memcmp(bytes, "\xF0\x9F\x98\x80", 4) == 0, "U+1F600");
    CHECK(muniEncodeUtf8(0xD800, bytes, &size) == muni_errorInvalid, "a surrogate is refused");
    CHECK(muniEncodeUtf8(0x110000, bytes, &size) == muni_errorInvalid, "past U+10FFFF");
}

// Prefixes of mixed text cut at every length, including inside a code
// point, then three bytes of edge values: errors and truncations land on
// either side of the validator's chunk and ASCII-run boundaries.
static void TestValidationAgreesAfterLongPrefixes(void)
{
    static const uint8_t edges[] = {0x00, 0x7F, 0x80, 0x9F, 0xA0, 0xBF, 0xC1, 0xC2,
                                    0xDF, 0xE0, 0xED, 0xEF, 0xF0, 0xF4, 0xF5, 0xFF};
    static const char unit[] = "a\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80"; // a, e acute, euro, emoji
    uint8_t bytes[80];
    size_t prefix = 0;
    while (prefix + sizeof(unit) - 1 <= 72)
    {
        memcpy(bytes + prefix, unit, sizeof(unit) - 1);
        prefix += sizeof(unit) - 1;
    }
    int before = s_mismatches;
    uint8_t tail[80 + 3];
    for (size_t length = 0; length <= prefix; length++)
    {
        memcpy(tail, bytes, length);
        for (size_t a = 0; a < sizeof(edges); a++)
        {
            for (size_t b = 0; b < sizeof(edges); b++)
            {
                for (size_t c = 0; c < sizeof(edges); c++)
                {
                    tail[length] = edges[a];
                    tail[length + 1] = edges[b];
                    tail[length + 2] = edges[c];
                    Compare(tail, length + 3);
                }
            }
        }
    }
    CHECK(s_mismatches == before, "errors after prefixes of every length");
}

int main(void)
{
    TestValidationAgreesOnEveryShortString();
    TestValidationAgreesAfterLongPrefixes();
    TestValidationAgreesOnFourByteStrings();
    TestMaximalSubpartsMatchTheReference();
    TestErrorKinds();
    TestReplacementFollowsTheStandard();
    TestConversionsRoundTrip();
    TestCapacityReportsTheTotal();
    TestUtf16Surrogates();
    TestEncode();
    return s_failures == 0 ? 0 : 1;
}
