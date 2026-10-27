// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the bidi algorithm on long text, which the other
// targets, fed at most 256 bytes, never reach. The input is a pattern
// the target repeats into a text of 4 KiB to 512 KiB; the first byte
// picks the direction and the length. Every paragraph must resolve and
// tile the text, and a run must finish within libFuzzer's -timeout, so
// work that grows faster than the text is a finding.

#include "maul-unicode/bidi.h"

#include <stdlib.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 2)
    {
        return 0;
    }
    muniBidiDirection direction = (muniBidiDirection)(data[0] % 3);
    size_t length = (size_t)4096 << (data[0] >> 2) % 8;
    const uint8_t* pattern = data + 1;
    size_t patternLength = size - 1;
    // Exactly the text's length, so AddressSanitizer sees an overread.
    char* text = malloc(length);
    uint8_t* levels = malloc(length);
    uint8_t* workspace = malloc(length);
    Require(text != nullptr && levels != nullptr && workspace != nullptr);
    for (size_t i = 0; i < length; i++)
    {
        text[i] = (char)pattern[i % patternLength];
    }
    size_t offset = 0;
    while (offset < length)
    {
        size_t paragraph = 0;
        uint8_t level = 0;
        Require(muniResolveBidi(text + offset, length - offset, direction, levels, workspace,
                                &paragraph, &level) == muni_success);
        Require(paragraph > 0 && paragraph <= length - offset && level <= 1);
        offset += paragraph;
    }
    free(text);
    free(levels);
    free(workspace);
    return 0;
}
