// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The bidi algorithm against BidiCharacterTest.txt (code points, with
// brackets) and BidiTest.txt (bidi classes, which the test turns into
// one representative code point each). Levels are compared after rule L1
// and the visual order through muniReorderBidiLine, both over the whole
// paragraph as one line, as the files expect.

#include "test_harness.h"

#include "maul-unicode/bidi.h"
#include "maul-unicode/encoding.h"
#include "maul-unicode/properties.h"

#include <stdlib.h>
#include <string.h>

#define MAX_CODE_POINTS 256
#define NO_LEVEL        0xFF

// One case: its code points as UTF-8, and what the file expects.
typedef struct Case
{
    char text[MAX_CODE_POINTS * 4];
    size_t length;
    size_t offsets[MAX_CODE_POINTS + 1]; // byte offset of each code point
    size_t count;
    uint8_t expectedLevels[MAX_CODE_POINTS]; // NO_LEVEL for x
    size_t expectedOrder[MAX_CODE_POINTS];
    size_t orderCount;
} Case;

static bool AddCodePoint(Case* c, uint32_t codePoint)
{
    size_t size = 0;
    if (c->count == MAX_CODE_POINTS ||
        muniEncodeUtf8(codePoint, c->text + c->length, &size) != muni_success)
    {
        return false;
    }
    c->offsets[c->count++] = c->length;
    c->length += size;
    c->offsets[c->count] = c->length;
    return true;
}

// Parses space-separated levels, with x for none.
static bool ParseLevels(const char* text, Case* c)
{
    size_t count = 0;
    while (*text != '\0' && *text != ';' && *text != '\n')
    {
        if (*text == ' ' || *text == '\t')
        {
            text += 1;
            continue;
        }
        if (count == MAX_CODE_POINTS)
        {
            return false;
        }
        if (*text == 'x')
        {
            c->expectedLevels[count++] = NO_LEVEL;
            text += 1;
            continue;
        }
        char* end = nullptr;
        c->expectedLevels[count++] = (uint8_t)strtoul(text, &end, 10);
        text = end;
    }
    return count == c->count;
}

static void ParseOrder(const char* text, Case* c)
{
    c->orderCount = 0;
    while (*text != '\0' && *text != '\n' && *text != ';')
    {
        if (*text == ' ' || *text == '\t')
        {
            text += 1;
            continue;
        }
        char* end = nullptr;
        c->expectedOrder[c->orderCount++] = strtoul(text, &end, 10);
        text = end;
    }
}

// Resolves the case and compares levels and order.
static bool Check(const Case* c, muniBidiDirection direction, int expectedParagraphLevel)
{
    static uint8_t levels[MAX_CODE_POINTS * 4];
    static uint8_t workspace[MAX_CODE_POINTS * 4];
    static uint8_t lineLevels[MAX_CODE_POINTS * 4];
    static muniBidiRun runs[MAX_CODE_POINTS];
    size_t paragraphLength = 0;
    uint8_t paragraphLevel = 0;
    if (muniResolveBidi(c->text, c->length, direction, levels, workspace, &paragraphLength,
                        &paragraphLevel) != muni_success ||
        paragraphLength != c->length ||
        (expectedParagraphLevel >= 0 && paragraphLevel != expectedParagraphLevel))
    {
        return false;
    }
    size_t runCount = 0;
    if (muniReorderBidiLine(c->text, levels, c->length, paragraphLevel, runs, MAX_CODE_POINTS,
                            &runCount) != muni_success)
    {
        return false;
    }
    for (size_t r = 0; r < runCount; r++)
    {
        memset(lineLevels + runs[r].start, runs[r].level, runs[r].length);
    }
    size_t visual = 0;
    for (size_t r = 0; r < runCount; r++)
    {
        bool reversed = (runs[r].level & 1) != 0;
        for (size_t k = 0; k < c->count; k++)
        {
            size_t i = reversed ? c->count - 1 - k : k;
            if (c->offsets[i] < runs[r].start || c->offsets[i] >= runs[r].start + runs[r].length ||
                c->expectedLevels[i] == NO_LEVEL)
            {
                continue;
            }
            if (visual >= c->orderCount || c->expectedOrder[visual] != i)
            {
                return false;
            }
            visual += 1;
        }
    }
    for (size_t i = 0; i < c->count; i++)
    {
        if (c->expectedLevels[i] != NO_LEVEL && lineLevels[c->offsets[i]] != c->expectedLevels[i])
        {
            return false;
        }
    }
    return visual == c->orderCount;
}

static char* NextField(char* text)
{
    char* separator = strchr(text, ';');
    return separator != nullptr ? separator + 1 : nullptr;
}

// BidiCharacterTest.txt: code points; direction; paragraph level; levels;
// order.
static bool RunCharacterLine(char* line, Case* c)
{
    memset(c, 0, sizeof(*c));
    char* cursor = line;
    while (*cursor != ';')
    {
        char* end = nullptr;
        unsigned long codePoint = strtoul(cursor, &end, 16);
        if (end == cursor)
        {
            cursor += 1;
            continue;
        }
        if (!AddCodePoint(c, (uint32_t)codePoint))
        {
            return false;
        }
        cursor = end;
    }
    char* direction = NextField(line);
    char* paragraphLevel = NextField(direction);
    char* levels = NextField(paragraphLevel);
    char* order = NextField(levels);
    if (order == nullptr || !ParseLevels(levels, c))
    {
        return false;
    }
    ParseOrder(order, c);
    static const muniBidiDirection directions[3] = {muni_bidiLeftToRight, muni_bidiRightToLeft,
                                                    muni_bidiAuto};
    return Check(c, directions[atoi(direction) % 3], atoi(paragraphLevel));
}

static void TestCharacterFile(void)
{
    FILE* file = fopen(MUNI_TEST_DATA "/BidiCharacterTest.txt", "r");
    CHECK(file != nullptr, "BidiCharacterTest.txt opens");
    if (file == nullptr)
    {
        return;
    }
    static char line[8192];
    static Case c;
    int cases = 0;
    int failures = 0;
    int lineNumber = 0;
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        lineNumber += 1;
        if (line[0] == '#' || line[0] == '\n')
        {
            continue;
        }
        cases += 1;
        if (!RunCharacterLine(line, &c))
        {
            if (failures < 10)
            {
                printf("BidiCharacterTest.txt:%d: %s", lineNumber, line);
            }
            failures += 1;
        }
    }
    fclose(file);
    printf("BidiCharacterTest.txt: %d cases, %d failures\n", cases, failures);
    CHECK(cases > 90000 && failures == 0, "every BidiCharacterTest case");
}

// A code point of each bidi class, in the order of the muni_bc values.
static const char* const s_classNames[] = {"L",   "R",   "AL",  "EN",  "ES",  "ET",  "AN",  "CS",
                                           "NSM", "BN",  "B",   "S",   "WS",  "ON",  "LRE", "LRO",
                                           "RLE", "RLO", "PDF", "LRI", "RLI", "FSI", "PDI"};
static const uint32_t s_classSamples[] = {
    0x0061, 0x05D0, 0x0627, 0x0031, 0x002B, 0x0023, 0x0660, 0x002C, 0x0300, 0x00AD, 0x2029, 0x0009,
    0x0020, 0x0021, 0x202A, 0x202D, 0x202B, 0x202E, 0x202C, 0x2066, 0x2067, 0x2068, 0x2069};

static bool ParseClasses(char* text, Case* c)
{
    c->count = 0;
    c->length = 0;
    char* token = strtok(text, " \t");
    while (token != nullptr)
    {
        size_t k = 0;
        while (k < sizeof(s_classSamples) / sizeof(s_classSamples[0]) &&
               strcmp(token, s_classNames[k]) != 0)
        {
            k += 1;
        }
        if (k == sizeof(s_classSamples) / sizeof(s_classSamples[0]) ||
            !AddCodePoint(c, s_classSamples[k]))
        {
            return false;
        }
        token = strtok(nullptr, " \t");
    }
    return true;
}

static void TestClassFile(void)
{
    FILE* file = fopen(MUNI_TEST_DATA "/BidiTest.txt", "r");
    CHECK(file != nullptr, "BidiTest.txt opens");
    if (file == nullptr)
    {
        return;
    }
    static char line[8192];
    static char levels[8192];
    static Case c;
    int cases = 0;
    int failures = 0;
    int lineNumber = 0;
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        lineNumber += 1;
        if (strncmp(line, "@Levels:", 8) == 0)
        {
            snprintf(levels, sizeof(levels), "%s", line + 8);
            continue;
        }
        if (strncmp(line, "@Reorder:", 9) == 0)
        {
            ParseOrder(line + 9, &c);
            continue;
        }
        char* bits = strchr(line, ';');
        if (line[0] == '#' || line[0] == '@' || bits == nullptr)
        {
            continue;
        }
        *bits = '\0';
        int set = atoi(bits + 1);
        bool parsed = ParseClasses(line, &c) && ParseLevels(levels, &c);
        static const muniBidiDirection directions[3] = {muni_bidiAuto, muni_bidiLeftToRight,
                                                        muni_bidiRightToLeft};
        for (int bit = 0; bit < 3; bit++)
        {
            if ((set & (1 << bit)) == 0)
            {
                continue;
            }
            cases += 1;
            if (!parsed || !Check(&c, directions[bit], -1))
            {
                if (failures < 10)
                {
                    printf("BidiTest.txt:%d: bit %d: %s\n", lineNumber, bit, line);
                }
                failures += 1;
            }
        }
    }
    fclose(file);
    printf("BidiTest.txt: %d cases, %d failures\n", cases, failures);
    CHECK(cases > 400000 && failures == 0, "every BidiTest case");
}

static void TestParagraphsAndLines(void)
{
    // "abc" then a Hebrew word, a newline, and a second paragraph in
    // Hebrew: U+05D0 U+05D1.
    const char text[] = "ab \xD7\x90\xD7\x91\n\xD7\x90\xD7\x91";
    size_t length = sizeof(text) - 1;
    uint8_t levels[32];
    uint8_t workspace[32];
    size_t paragraph = 0;
    uint8_t level = 9;
    CHECK(muniResolveBidi(text, length, muni_bidiAuto, levels, workspace, &paragraph, &level) ==
                  muni_success &&
              paragraph == 8 && level == 0,
          "the first paragraph ends after the newline, left to right");
    CHECK(levels[0] == 0 && levels[3] == 1 && levels[4] == 1 && levels[6] == 1,
          "the Hebrew word is at level 1, the newline too before L1");
    muniBidiRun runs[4];
    size_t count = 0;
    CHECK(muniReorderBidiLine(text, levels, paragraph, level, runs, 4, &count) == muni_success &&
              count == 3,
          "three runs");
    CHECK(runs[0].start == 0 && runs[0].length == 3 && runs[1].start == 3 && runs[1].level == 1 &&
              runs[2].start == 7 && runs[2].level == 0,
          "ab, the Hebrew word, the newline reset to the paragraph level");
    CHECK(muniReorderBidiLine(text, levels, paragraph, level, runs, 2, &count) ==
                  muni_errorCapacity &&
              count == 3,
          "too few runs");
    CHECK(muniResolveBidi(text + paragraph, length - paragraph, muni_bidiAuto, levels, workspace,
                          &paragraph, &level) == muni_success &&
              paragraph == 4 && level == 1,
          "the second paragraph is right to left");
    CHECK(muniResolveBidi("\r\nx", 3, muni_bidiAuto, levels, workspace, &paragraph, &level) ==
                  muni_success &&
              paragraph == 2,
          "CR LF ends one paragraph");
    CHECK(muniResolveBidi(nullptr, 0, muni_bidiRightToLeft, nullptr, nullptr, &paragraph, &level) ==
                  muni_success &&
              paragraph == 0 && level == 1,
          "an empty paragraph");
    CHECK(muniResolveBidi("a", 1, 3, levels, workspace, &paragraph, &level) == muni_errorInvalid,
          "an unknown direction");
    CHECK(muniResolveBidi("a", 1, muni_bidiAuto, nullptr, workspace, &paragraph, &level) ==
              muni_errorInvalid,
          "no levels");
}

static void TestMaps(void)
{
    const uint8_t levels[] = {0, 0, 1, 1, 2, 1, 0};
    size_t visual[7];
    size_t logical[7];
    CHECK(muniReorderBidiLevels(levels, 7, visual) == muni_success, "reorder levels");
    const size_t expected[] = {0, 1, 5, 4, 3, 2, 6};
    CHECK(memcmp(visual, expected, sizeof(expected)) == 0, "the visual order");
    CHECK(muniInvertBidiMap(visual, 7, logical) == muni_success && logical[5] == 2 &&
              logical[2] == 5,
          "the inverse");
    const size_t broken[] = {0, 9};
    CHECK(muniInvertBidiMap(broken, 2, logical) == muni_errorInvalid, "an entry out of range");
}

int main(void)
{
    TestCharacterFile();
    TestClassFile();
    TestParagraphsAndLines();
    TestMaps();
    return s_failures == 0 ? 0 : 1;
}
