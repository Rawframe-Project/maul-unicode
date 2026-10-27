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

int main(void)
{
    TestCounting();
    TestSaturation();
    return s_failures == 0 ? 0 : 1;
}
