// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The UTF-8 writer behind case mapping, normalization and skeletons. A
// count past SIZE_MAX needs more input than a test can allocate, so this
// white-box suite starts a writer near it instead.

#include "test_harness.h"
#include "writer.h"

#include <stdint.h>
#include <string.h>

static void TestCounting(void)
{
    char output[4] = {0};
    muniWriter writer = {output, sizeof(output), 0};
    muniWriterPut(&writer, 0x41);
    muniWriterPut(&writer, 0x20AC);
    CHECK(writer.needed == 4 && memcmp(output, "A\xE2\x82\xAC", 4) == 0 && muniWriterFits(&writer),
          "the bytes that fit");
    muniWriterPut(&writer, 0x10348);
    CHECK(writer.needed == 8 && !muniWriterFits(&writer), "a code point past the capacity");
}

static void TestSaturation(void)
{
    char output[4] = {0};
    muniWriter writer = {output, sizeof(output), SIZE_MAX - 5};
    muniWriterPut(&writer, 0x20AC);
    CHECK(writer.needed == SIZE_MAX - 2 && !muniWriterFits(&writer), "a count near SIZE_MAX");
    muniWriterPut(&writer, 0x10348);
    CHECK(writer.needed == SIZE_MAX && !muniWriterFits(&writer), "a count past SIZE_MAX");
    muniWriterPut(&writer, 0x41);
    CHECK(writer.needed == SIZE_MAX, "a count that stays at SIZE_MAX");
    CHECK(memcmp(output, "\0\0\0\0", 4) == 0, "nothing written past the capacity");
    muniWriter whole = {output, SIZE_MAX, SIZE_MAX - 1};
    muniWriterPut(&whole, 0x41);
    CHECK(whole.needed == SIZE_MAX && !muniWriterFits(&whole), "SIZE_MAX never fits");
}

// Bytes copied as they are: the part that fits, the whole count, and the
// same saturation as a code point.
static void TestBytes(void)
{
    char output[4] = {0};
    muniWriter writer = {output, sizeof(output), 0};
    muniWriterPutBytes(&writer, (const uint8_t*)"abc", 3);
    muniWriterPutBytes(&writer, (const uint8_t*)"de", 2);
    CHECK(writer.needed == 5 && memcmp(output, "abcd", 4) == 0 && !muniWriterFits(&writer),
          "the bytes that fit, and the count of all");
    muniWriterPutBytes(&writer, (const uint8_t*)"f", 1);
    CHECK(writer.needed == 6 && memcmp(output, "abcd", 4) == 0, "nothing past the capacity");
    muniWriter none = {nullptr, 0, 0};
    muniWriterPutBytes(&none, (const uint8_t*)"", 0);
    CHECK(none.needed == 0 && muniWriterFits(&none), "nothing written, nothing counted");
    muniWriter high = {output, sizeof(output), SIZE_MAX - 4};
    muniWriterPutBytes(&high, (const uint8_t*)"abc", 3);
    CHECK(high.needed == SIZE_MAX - 1, "a count just below SIZE_MAX");
    muniWriterPutBytes(&high, (const uint8_t*)"a", 1);
    CHECK(high.needed == SIZE_MAX && !muniWriterFits(&high), "a count that reaches SIZE_MAX");
    muniWriterPutBytes(&high, (const uint8_t*)"", 0);
    CHECK(high.needed == SIZE_MAX, "a count that stays at SIZE_MAX");
}

int main(void)
{
    TestCounting();
    TestSaturation();
    TestBytes();
    return s_failures == 0 ? 0 : 1;
}
