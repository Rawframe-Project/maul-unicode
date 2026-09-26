// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the bidi algorithm. The first input byte picks
// the direction and a line split; the rest is the text. Paragraphs must
// tile the text, levels stay within 126, the runs of each line cover it
// exactly once, and the visual order from levels is a permutation.

#include "maul-unicode/bidi.h"

#include <stdlib.h>
#include <string.h>

#define MAX_TEXT 4096

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static void CheckLine(const char* line, const uint8_t* levels, size_t length, uint8_t level)
{
    static muniBidiRun runs[MAX_TEXT];
    static uint8_t covered[MAX_TEXT];
    size_t count = 0;
    Require(muniReorderBidiLine(line, levels, length, level, runs, MAX_TEXT, &count) ==
            muni_success);
    memset(covered, 0, length);
    for (size_t r = 0; r < count; r++)
    {
        Require(runs[r].length > 0 && runs[r].start + runs[r].length <= length);
        Require(runs[r].level <= 126);
        for (size_t k = runs[r].start; k < runs[r].start + runs[r].length; k++)
        {
            Require(covered[k] == 0);
            covered[k] = 1;
        }
    }
    for (size_t k = 0; k < length; k++)
    {
        Require(covered[k] == 1);
    }
}

static void CheckOrder(const uint8_t* levels, size_t length)
{
    static size_t visual[MAX_TEXT];
    static size_t logical[MAX_TEXT];
    Require(muniReorderBidiLevels(levels, length, visual) == muni_success);
    Require(muniInvertBidiMap(visual, length, logical) == muni_success);
    for (size_t k = 0; k < length; k++)
    {
        Require(visual[logical[k]] == k);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size == 0 || size > MAX_TEXT)
    {
        return 0;
    }
    static uint8_t levels[MAX_TEXT];
    static uint8_t workspace[MAX_TEXT];
    muniBidiDirection direction = (muniBidiDirection)(data[0] % 3);
    const char* text = (const char*)data + 1;
    size_t length = size - 1;
    size_t offset = 0;
    while (offset < length)
    {
        size_t paragraph = 0;
        uint8_t level = 0;
        Require(muniResolveBidi(text + offset, length - offset, direction, levels, workspace,
                                &paragraph, &level) == muni_success);
        Require(paragraph > 0 && paragraph <= length - offset && level <= 1);
        for (size_t k = 0; k < paragraph; k++)
        {
            Require(levels[k] <= 126);
        }
        CheckLine(text + offset, levels, paragraph, level);
        // Two lines, split at a byte the first input byte picks.
        size_t split = (size_t)(data[0] >> 2) % paragraph;
        CheckLine(text + offset, levels, split, level);
        CheckLine(text + offset + split, levels + split, paragraph - split, level);
        CheckOrder(levels, paragraph);
        offset += paragraph;
    }
    return 0;
}
