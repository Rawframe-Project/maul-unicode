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
        muniEncodeUtf8(codePoint, c->text + c->length, sizeof(c->text) - c->length, &size) !=
            muni_success)
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

static uint8_t s_lineLevels[MAX_CODE_POINTS * 4];

// Resolves the case into the buffers given and compares levels and order.
static bool Resolved(const Case* c, const char* text, uint8_t* levels, uint8_t* workspace,
                     muniBidiRun** runsOut, muniBidiDirection direction, int expectedParagraphLevel)
{
    uint8_t* lineLevels = s_lineLevels;
    size_t paragraphLength = 0;
    uint8_t paragraphLevel = 0;
    if (muniResolveBidi(text, c->length, direction, levels, workspace, &paragraphLength,
                        &paragraphLevel) != muni_success ||
        paragraphLength != c->length ||
        (expectedParagraphLevel >= 0 && paragraphLevel != expectedParagraphLevel))
    {
        return false;
    }
    size_t runCount = 0;
    muniResult measured =
        muniReorderBidiLine(text, levels, c->length, paragraphLevel, nullptr, 0, &runCount);
    if ((measured != muni_errorCapacity && runCount != 0) || runCount == 0)
    {
        return false;
    }
    muniBidiRun* runs = malloc(runCount * sizeof(muniBidiRun));
    *runsOut = runs;
    size_t written = 0;
    if (runs == nullptr ||
        muniReorderBidiLine(text, levels, c->length, paragraphLevel, runs, runCount, &written) !=
            muni_success ||
        written != runCount)
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

// Resolves the case and compares levels and order. The text, levels,
// workspace and runs each sit alone in a heap allocation of exactly their
// size, so that AddressSanitizer sees any access past them, and the runs
// are counted first and then written into an array that fits exactly.
static bool Check(const Case* c, muniBidiDirection direction, int expectedParagraphLevel)
{
    size_t length = c->length;
    char* text = malloc(length);
    uint8_t* levels = malloc(length);
    uint8_t* workspace = malloc(length);
    muniBidiRun* runs = nullptr;
    bool passed = false;
    if (text != nullptr && levels != nullptr && workspace != nullptr)
    {
        memcpy(text, c->text, length);
        passed = Resolved(c, text, levels, workspace, &runs, direction, expectedParagraphLevel);
    }
    free(runs);
    free(workspace);
    free(levels);
    free(text);
    return passed;
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
    FILE* file = fopen(MUNI_UCD_DATA "/BidiCharacterTest.txt", "r");
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
    FILE* file = fopen(MUNI_UCD_DATA "/BidiTest.txt", "r");
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
    muniBidiRun untouched[4];
    memset(runs, 0xA5, sizeof(runs));
    memset(untouched, 0xA5, sizeof(untouched));
    CHECK(muniReorderBidiLine(text, levels, paragraph, level, runs, 2, &count) ==
                  muni_errorCapacity &&
              count == 3 && memcmp(runs, untouched, sizeof(runs)) == 0,
          "too few runs, and none written");
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
    // Each refusal leaves the empty paragraph's outputs as they were.
    CHECK(muniResolveBidi("a", 1, 3, levels, workspace, &paragraph, &level) == muni_errorInvalid &&
              paragraph == 0 && level == 1,
          "an unknown direction");
    CHECK(muniResolveBidi("a", 1, muni_bidiAuto, nullptr, workspace, &paragraph, &level) ==
                  muni_errorInvalid &&
              paragraph == 0 && level == 1,
          "no levels");
    CHECK(muniResolveBidi(nullptr, 0, muni_bidiAuto, nullptr, nullptr, nullptr, &level) ==
                  muni_errorInvalid &&
              level == 1 &&
              muniResolveBidi(nullptr, 0, muni_bidiAuto, nullptr, nullptr, &paragraph, nullptr) ==
                  muni_errorInvalid &&
              paragraph == 0,
          "the paragraph outputs are needed even for empty text");
    muniBidiRun run = {7, 7, 7};
    size_t runCount = 7;
    CHECK(muniReorderBidiLine(nullptr, nullptr, 0, 0, nullptr, 0, &runCount) == muni_success &&
              runCount == 0,
          "an empty line, with no text and no levels");
    runCount = 7;
    CHECK(muniReorderBidiLine(nullptr, nullptr, 0, 0, &run, 1, nullptr) == muni_errorInvalid &&
              run.start == 7,
          "a line needs a count out even when empty");
    CHECK(muniReorderBidiLine(nullptr, levels, 1, 0, &run, 1, &runCount) == muni_errorInvalid &&
              runCount == 7 && run.start == 7,
          "no line with a length");
}

// The level of the byte probe bytes into the content of an FSI that
// follows left-to-right embeddings (each raises the level by 2) and then
// the given extra initiator.
static uint8_t LevelInFsi(int embeddings, const char* extra, const char* content, size_t probe)
{
    char text[256];
    size_t length = 0;
    for (int i = 0; i < embeddings; i++)
    {
        memcpy(text + length, "\xE2\x80\xAA", 3); // LRE
        length += 3;
    }
    memcpy(text + length, extra, strlen(extra));
    length += strlen(extra);
    memcpy(text + length, "\xE2\x81\xA8", 3); // FSI
    length += 3;
    size_t at = length + probe;
    memcpy(text + length, content, strlen(content));
    length += strlen(content);
    memcpy(text + length, "\xE2\x81\xA9", 3); // PDI
    length += 3;
    uint8_t levels[256];
    uint8_t workspace[256];
    size_t paragraph = 0;
    uint8_t level = 0;
    if (muniResolveBidi(text, length, muni_bidiLeftToRight, levels, workspace, &paragraph,
                        &level) != muni_success ||
        paragraph != length)
    {
        return 0xFF;
    }
    return levels[at];
}

static void TestIsolateDepth(void)
{
    // A Hebrew letter, then a Latin one. Inside a right-to-left isolate at
    // 125 the Latin letter rises to 126 (I2); when the FSI overflows, it
    // stays at the even level outside, while the Hebrew letter reaches 125
    // either way (I1), so only the Latin letter tells the two apart.
    const char* hebrewLatin = "\xD7\x90"
                              "a";
    const char* rle = "\xE2\x80\xAB";
    // At 124 a right-to-left isolate fits at 125 and a left-to-right one
    // at 126 does not, so the FSI's direction decides whether it takes a
    // level.
    CHECK(LevelInFsi(62, "", hebrewLatin, 0) == 125, "Hebrew in an FSI at 124");
    CHECK(LevelInFsi(62, "", hebrewLatin, 2) == 126, "an FSI of Hebrew at 124 isolates at 125");
    CHECK(LevelInFsi(62, "", "a", 0) == 124, "an FSI of Latin at 124 overflows");
    // At 123 both fit: right to left at 125, left to right at 124.
    CHECK(LevelInFsi(61, rle, hebrewLatin, 2) == 126, "an FSI of Hebrew at 123 isolates at 125");
    CHECK(LevelInFsi(61, rle, "a", 0) == 124, "an FSI of Latin at 123 isolates at 124");
    // At 125 neither fits: the FSI overflows whatever follows it.
    CHECK(LevelInFsi(62, rle, hebrewLatin, 0) == 125, "Hebrew in an FSI at 125");
    CHECK(LevelInFsi(62, rle, hebrewLatin, 2) == 126, "an FSI of Hebrew at 125 overflows");
    CHECK(LevelInFsi(62, rle, "a", 0) == 126, "an FSI of Latin at 125 overflows");
    // A PDF inside the overflowed FSI does nothing (X7), so the Latin
    // letter after it stays at 125 and rises to 126; were the RLE popped,
    // it would sit at 124.
    CHECK(LevelInFsi(62, rle,
                     "\xE2\x80\xAC"
                     "a",
                     3) == 126,
          "a PDF inside an overflowed FSI");
}

// Bidi takes work in proportion to its text. A long run of FSIs with no
// PDI and no strong character keeps every FSI's isolate open, and each
// FSI that can still take a level looks ahead for its direction, so
// work that grew faster than the text would outlast the suite's timeout
// here.
static void TestLongIsolateRun(void)
{
    const size_t units = 512 * 1024; // 2 MiB of FSI, then a digit
    size_t length = units * 4;
    char* text = malloc(length);
    uint8_t* levels = malloc(length);
    uint8_t* workspace = malloc(length);
    CHECK(text != nullptr && levels != nullptr && workspace != nullptr, "memory for a long text");
    if (text != nullptr && levels != nullptr && workspace != nullptr)
    {
        for (size_t i = 0; i < units; i++)
        {
            memcpy(text + 4 * i,
                   "\xE2\x81\xA8"
                   "1",
                   4);
        }
        size_t paragraph = 0;
        uint8_t level = 0;
        CHECK(muniResolveBidi(text, length, muni_bidiLeftToRight, levels, workspace, &paragraph,
                              &level) == muni_success &&
                  paragraph == length,
              "a long run of FSIs is one paragraph");
        // 62 left-to-right isolates reach 124 and the rest overflow; the
        // digits, which W7 makes L after a left-to-right start, stay there.
        CHECK(levels[length - 1] == 124, "the last digit of a long run of FSIs");
    }
    free(text);
    free(levels);
    free(workspace);
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
    const uint8_t deep[] = {0, 126, 127};
    CHECK(muniReorderBidiLevels(deep, 2, visual) == muni_success, "a level of 126");
    CHECK(muniReorderBidiLevels(nullptr, 0, nullptr) == muni_success &&
              muniInvertBidiMap(nullptr, 0, nullptr) == muni_success,
          "no units, and no arrays for them");
    memset(visual, 0xA5, sizeof(visual));
    CHECK(muniReorderBidiLevels(deep, 3, visual) == muni_errorInvalid && visual[0] != 0,
          "a level above 126, and no map written");
    muniBidiRun runs[3];
    size_t count = 0;
    CHECK(muniReorderBidiLine("abc", deep, 3, 0, runs, 3, &count) == muni_errorInvalid,
          "a line with a level above 126");
    const uint8_t flat[] = {0, 0};
    CHECK(muniReorderBidiLine("a ", flat, 2, 2, runs, 3, &count) == muni_errorInvalid,
          "a paragraph level of 2");
    CHECK(muniReorderBidiLine("a ", flat, 2, 200, runs, 3, &count) == muni_errorInvalid,
          "a paragraph level of 200");
    CHECK(muniReorderBidiLine("a ", flat, 2, 1, runs, 3, &count) == muni_success,
          "a paragraph level of 1");
    const size_t broken[] = {0, 9};
    CHECK(muniInvertBidiMap(broken, 2, logical) == muni_errorInvalid, "an entry out of range");
    const size_t repeated[] = {2, 0, 2};
    CHECK(muniInvertBidiMap(repeated, 3, logical) == muni_errorInvalid, "a repeated entry");
    const size_t first[] = {0, 0};
    CHECK(muniInvertBidiMap(first, 2, logical) == muni_errorInvalid, "a repeated first entry");
    logical[0] = 7;
    CHECK(muniInvertBidiMap(nullptr, 1, logical) == muni_errorInvalid && logical[0] == 7,
          "no map with a count, and nothing written");
    CHECK(muniInvertBidiMap(first, 1, nullptr) == muni_errorInvalid, "no inverse with a count");
}

int main(void)
{
    TestCharacterFile();
    TestClassFile();
    TestParagraphsAndLines();
    TestIsolateDepth();
    TestLongIsolateRun();
    TestMaps();
    return s_failures == 0 ? 0 : 1;
}
