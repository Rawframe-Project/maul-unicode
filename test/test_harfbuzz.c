// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The HarfBuzz functions: compared with HarfBuzz's own built-in data
// over long-standing blocks, skipping what HarfBuzz's older Unicode data
// lacks and the few properties Unicode changed since, and used by a
// HarfBuzz buffer to guess script and direction.

#include "test_harness.h"

#include "maul-unicode/harfbuzz.h"

#include <hb.h>

// Blocks whose characters and properties Unicode has left alone for
// years: Basic Latin to Greek and Cyrillic, Hebrew and Arabic, Devanagari,
// Thai, Hangul Jamo, General Punctuation, CJK Symbols to Katakana, and a
// slice of CJK ideographs.
static const uint32_t s_ranges[][2] = {
    {0x0000, 0x04FF}, {0x0590, 0x06FF}, {0x0900, 0x097F}, {0x0E00, 0x0E7F}, {0x1100, 0x11FF},
    {0x2000, 0x206F}, {0x3000, 0x30FF}, {0x4E00, 0x4FFF}, {0xAC00, 0xAD00},
};

// Properties Unicode changed after the versions HarfBuzz releases carry.
static const uint32_t s_changed[] = {
    0x0295, // General_Category Ll to Lo in Unicode 16
};

static bool Changed(uint32_t codePoint)
{
    for (size_t i = 0; i < sizeof(s_changed) / sizeof(s_changed[0]); i++)
    {
        if (s_changed[i] == codePoint)
        {
            return true;
        }
    }
    return false;
}

static void TestAgainstHarfBuzz(void)
{
    hb_unicode_funcs_t* ours = muniCreateHarfBuzzFunctions();
    hb_unicode_funcs_t* theirs = hb_unicode_funcs_get_default();
    CHECK(hb_unicode_funcs_is_immutable(ours), "the functions are immutable");
    int mismatches = 0;
    for (size_t r = 0; r < sizeof(s_ranges) / sizeof(s_ranges[0]); r++)
    {
        for (uint32_t c = s_ranges[r][0]; c <= s_ranges[r][1]; c++)
        {
            if (Changed(c) ||
                hb_unicode_general_category(theirs, c) == HB_UNICODE_GENERAL_CATEGORY_UNASSIGNED)
            {
                continue;
            }
            hb_codepoint_t a = 0;
            hb_codepoint_t b = 0;
            hb_codepoint_t x = 0;
            hb_codepoint_t y = 0;
            bool same =
                hb_unicode_general_category(ours, c) == hb_unicode_general_category(theirs, c) &&
                hb_unicode_combining_class(ours, c) == hb_unicode_combining_class(theirs, c) &&
                hb_unicode_mirroring(ours, c) == hb_unicode_mirroring(theirs, c) &&
                hb_unicode_script(ours, c) == hb_unicode_script(theirs, c) &&
                hb_unicode_decompose(ours, c, &a, &b) == hb_unicode_decompose(theirs, c, &x, &y) &&
                a == x && b == y;
            if (!same && mismatches < 5)
            {
                printf("U+%04X differs from HarfBuzz's data\n", c);
            }
            mismatches += same ? 0 : 1;
        }
    }
    CHECK(mismatches == 0, "the same answers as HarfBuzz");
    hb_codepoint_t composite = 0;
    CHECK(hb_unicode_compose(ours, 'e', 0x0301, &composite) && composite == 0x00E9,
          "e and acute compose");
    CHECK(hb_unicode_compose(ours, 0x1100, 0x1161, &composite) && composite == 0xAC00,
          "Hangul composes");
    CHECK(!hb_unicode_compose(ours, 'a', 'b', &composite), "a and b do not");
    hb_unicode_funcs_destroy(ours);
}

static void TestBuffer(void)
{
    hb_unicode_funcs_t* ours = muniCreateHarfBuzzFunctions();
    hb_buffer_t* buffer = hb_buffer_create();
    hb_buffer_set_unicode_funcs(buffer, ours);
    // Arabic "salam".
    hb_buffer_add_utf8(buffer, "\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85", -1, 0, -1);
    hb_buffer_guess_segment_properties(buffer);
    CHECK(hb_buffer_get_script(buffer) == HB_SCRIPT_ARABIC, "the script is Arabic");
    CHECK(hb_buffer_get_direction(buffer) == HB_DIRECTION_RTL, "the direction is right to left");
    hb_buffer_destroy(buffer);
    hb_unicode_funcs_destroy(ours);
}

int main(void)
{
    TestAgainstHarfBuzz();
    TestBuffer();
    return s_failures == 0 ? 0 : 1;
}
