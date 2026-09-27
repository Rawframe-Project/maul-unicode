// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Lays out one line of mixed-direction text as a renderer would, minus
// the shaping: resolves the paragraph's embedding levels, orders its
// runs from left to right, and prints each right-to-left run backward
// by grapheme cluster, so combining marks stay on their base, with
// mirrored characters such as parentheses turned around. Give the text
// as the first argument; without one, English with a Hebrew phrase in
// parentheses and a number is used.
//
// A terminal applies bidi itself, so the visual string may show
// reordered again; the list of runs is the layout.

#include "maul-unicode/bidi.h"

#include "maul-unicode/encoding.h"
#include "maul-unicode/properties.h"
#include "maul-unicode/segment.h"

#include <stdio.h>
#include <string.h>

#define MAX_TEXT 1024
#define MAX_RUNS 64

static void PutCodePoint(uint32_t codePoint)
{
    char bytes[4];
    size_t size = 0;
    if (muniEncodeUtf8(codePoint, bytes, &size) == muni_success)
    {
        fwrite(bytes, 1, size, stdout);
    }
}

// Prints a cluster with each code point mirrored when it has a mirror.
static void PutMirrored(const char* cluster, size_t length)
{
    size_t offset = 0;
    while (offset < length)
    {
        uint32_t codePoint = 0;
        size_t size = 1;
        (void)muniDecodeUtf8(cluster + offset, length - offset, &codePoint, &size);
        PutCodePoint(muniGetMirroringGlyph(codePoint));
        offset += size;
    }
}

// Prints a right-to-left run: its grapheme clusters from last to first.
static void PutBackward(const char* run, size_t length)
{
    size_t ends[MAX_TEXT];
    size_t count = 0;
    if (muniFindGraphemeBreaks(run, length, ends, MAX_TEXT, &count) != muni_success)
    {
        return;
    }
    for (size_t k = count; k > 0; k--)
    {
        size_t start = k > 1 ? ends[k - 2] : 0;
        PutMirrored(run + start, ends[k - 1] - start);
    }
}

int main(int argc, char** argv)
{
    const char* text = argc > 1 ? argv[1]
                                : "The title is \"\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D "
                                  "(\xD7\xA2\xD7\x95\xD7\x9C\xD7\x9D) 2026\" in Hebrew.";
    size_t length = strlen(text);
    if (length > MAX_TEXT || muniValidateUtf8(text, length).status != muni_success)
    {
        printf("give at most %d bytes of UTF-8\n", MAX_TEXT);
        return 1;
    }
    // One level and one scratch byte per byte of text, both the caller's.
    static uint8_t levels[MAX_TEXT];
    static uint8_t workspace[MAX_TEXT];
    size_t paragraphLength = 0;
    uint8_t paragraphLevel = 0;
    if (muniResolveBidi(text, length, muni_bidiAuto, levels, workspace, &paragraphLength,
                        &paragraphLevel) != muni_success)
    {
        return 1;
    }
    // The whole paragraph is one line here; a layout engine would break
    // it at line break opportunities first and order each line.
    muniBidiRun runs[MAX_RUNS];
    size_t count = 0;
    if (muniReorderBidiLine(text, levels, paragraphLength, paragraphLevel, runs, MAX_RUNS,
                            &count) != muni_success)
    {
        return 1;
    }
    printf("paragraph level %u, %zu runs from left to right:\n", paragraphLevel, count);
    for (size_t i = 0; i < count; i++)
    {
        printf("  level %u %s \"%.*s\"\n", runs[i].level,
               (runs[i].level & 1) != 0 ? "right to left" : "left to right", (int)runs[i].length,
               text + runs[i].start);
    }
    printf("visual: ");
    for (size_t i = 0; i < count; i++)
    {
        const char* run = text + runs[i].start;
        if ((runs[i].level & 1) != 0)
        {
            PutBackward(run, runs[i].length);
        }
        else
        {
            fwrite(run, 1, runs[i].length, stdout);
        }
    }
    printf("\n");
    return 0;
}
