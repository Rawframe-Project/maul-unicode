// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Segmentation against the Unicode conformance files. Each test line is
// run three ways: through the array convenience, through an iterator over
// the whole text, and through an iterator fed one byte at a time, which
// exercises sequences cut between pieces and the need-more-text path.

#include "test_harness.h"

#include "maul-unicode/encoding.h"
#include "maul-unicode/segment.h"

#include <stdlib.h>
#include <string.h>

#define MAX_CASE_BYTES 512
#define MAX_BREAKS     128

// One conformance case: its UTF-8 text and its boundaries after the start.
typedef struct Case
{
    char text[MAX_CASE_BYTES];
    size_t length;
    size_t breaks[MAX_BREAKS];
    size_t breakCount;
} Case;

// Parses a line such as "÷ 0020 × 0308 ÷ 0020 ÷ # comment". Returns 0 for
// a line without a case.
static int ParseCase(const char* line, Case* out)
{
    memset(out, 0, sizeof(*out));
    const char* cursor = line;
    int sawCodePoint = 0;
    while (*cursor != '\0' && *cursor != '#' && *cursor != '\n')
    {
        if ((unsigned char)cursor[0] == 0xC3 && (unsigned char)cursor[1] == 0xB7)
        {
            if (sawCodePoint)
            {
                out->breaks[out->breakCount++] = out->length;
            }
            cursor += 2;
        }
        else if ((unsigned char)cursor[0] == 0xC3 && (unsigned char)cursor[1] == 0x97)
        {
            cursor += 2;
        }
        else if (*cursor == ' ' || *cursor == '\t')
        {
            cursor += 1;
        }
        else
        {
            char* end = nullptr;
            unsigned long codePoint = strtoul(cursor, &end, 16);
            size_t size = 0;
            if (end == cursor ||
                muniEncodeUtf8((uint32_t)codePoint, out->text + out->length, &size) != muni_success)
            {
                return 0;
            }
            out->length += size;
            sawCodePoint = 1;
            cursor = end;
        }
    }
    return sawCodePoint;
}

typedef muniResult (*FindFn)(const char*, size_t, size_t*, size_t, size_t*);

static int SameBreaks(const size_t* found, size_t foundCount, const Case* expected)
{
    return foundCount == expected->breakCount &&
           memcmp(found, expected->breaks, foundCount * sizeof(size_t)) == 0;
}

static int RunGraphemeWhole(const Case* c)
{
    size_t found[MAX_BREAKS];
    size_t count = 0;
    muniGraphemeIterator iterator;
    if (muniInitGraphemeIterator(&iterator, c->text, c->length, false) != muni_success)
    {
        return 0;
    }
    size_t offset;
    while (count < MAX_BREAKS && muniNextGraphemeBreak(&iterator, &offset) == muni_success)
    {
        found[count++] = offset;
    }
    return SameBreaks(found, count, c);
}

static int RunGraphemeBytewise(const Case* c)
{
    size_t found[MAX_BREAKS];
    size_t count = 0;
    muniGraphemeIterator iterator;
    size_t fed = c->length > 0 ? 1 : 0;
    if (muniInitGraphemeIterator(&iterator, c->text, fed, fed < c->length) != muni_success)
    {
        return 0;
    }
    for (;;)
    {
        size_t offset;
        muniResult status = muniNextGraphemeBreak(&iterator, &offset);
        if (status == muni_success && count < MAX_BREAKS)
        {
            found[count++] = offset;
        }
        else if (status == muni_needMoreText)
        {
            if (muniFeedGraphemeIterator(&iterator, c->text + fed, 1, fed + 1 < c->length) !=
                muni_success)
            {
                return 0;
            }
            fed += 1;
        }
        else
        {
            break;
        }
    }
    return SameBreaks(found, count, c);
}

static int RunFind(FindFn find, const Case* c)
{
    size_t found[MAX_BREAKS];
    size_t count = 0;
    if (find(c->text, c->length, found, MAX_BREAKS, &count) != muni_success)
    {
        return 0;
    }
    return SameBreaks(found, count, c);
}

// Runs every case of a conformance file; returns the number of cases.
static int RunFile(const char* name, FindFn find, int (*whole)(const Case*),
                   int (*bytewise)(const Case*), int* failuresOut)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", MUNI_TEST_DATA, name);
    FILE* file = fopen(path, "r");
    if (file == nullptr)
    {
        printf("cannot open %s\n", path);
        *failuresOut = 1;
        return 0;
    }
    char line[4096];
    int cases = 0;
    int failures = 0;
    int lineNumber = 0;
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        lineNumber += 1;
        Case c;
        if (!ParseCase(line, &c))
        {
            continue;
        }
        cases += 1;
        int ok = RunFind(find, &c) && whole(&c) && bytewise(&c);
        if (!ok && failures < 10)
        {
            printf("%s:%d: %s", name, lineNumber, line);
        }
        failures += ok ? 0 : 1;
    }
    fclose(file);
    *failuresOut = failures;
    return cases;
}

static void TestGraphemeConformance(void)
{
    int failures = 0;
    int cases = RunFile("GraphemeBreakTest.txt", muniFindGraphemeBreaks, RunGraphemeWhole,
                        RunGraphemeBytewise, &failures);
    printf("GraphemeBreakTest.txt: %d cases, %d failures\n", cases, failures);
    CHECK(cases > 500 && failures == 0, "every grapheme conformance case");
}

static void TestGraphemeEdges(void)
{
    size_t offsets[4];
    size_t count = 99;
    CHECK(muniFindGraphemeBreaks(nullptr, 0, offsets, 4, &count) == muni_success && count == 0,
          "empty text has no boundaries");
    CHECK(muniFindGraphemeBreaks("ab", 2, nullptr, 0, &count) == muni_errorCapacity && count == 2,
          "measuring with no buffer");
    // A family emoji: man, ZWJ, woman, ZWJ, girl is one cluster of 18 bytes.
    const char family[] =
        "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7";
    CHECK(muniFindGraphemeBreaks(family, 18, offsets, 4, &count) == muni_success && count == 1 &&
              offsets[0] == 18,
          "a ZWJ family is one cluster");
    // Ill-formed bytes count as U+FFFD each, one cluster each.
    CHECK(muniFindGraphemeBreaks("a\xFF\xFE", 3, offsets, 4, &count) == muni_success && count == 3,
          "ill-formed bytes");
}

int main(void)
{
    TestGraphemeConformance();
    TestGraphemeEdges();
    return s_failures == 0 ? 0 : 1;
}
