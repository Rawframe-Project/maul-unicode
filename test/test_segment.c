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

typedef muniResult (*InitFn)(muniSegmentIterator*, const char*, size_t, bool);

static muniResult FindLines(const char* text, size_t length, size_t* offsets, size_t capacity,
                            size_t* countOut)
{
    return muniFindLineBreaks(text, length, offsets, nullptr, capacity, countOut);
}

// A segmentation kind: its conformance file and its functions.
typedef struct Kind
{
    const char* file;
    InitFn init;
    FindFn find;
} Kind;

static int RunWhole(const Kind* kind, const Case* c)
{
    size_t found[MAX_BREAKS];
    size_t count = 0;
    muniSegmentIterator iterator;
    if (kind->init(&iterator, c->text, c->length, false) != muni_success)
    {
        return 0;
    }
    size_t offset;
    while (count < MAX_BREAKS && muniNextSegmentBreak(&iterator, &offset) == muni_success)
    {
        found[count++] = offset;
    }
    return SameBreaks(found, count, c);
}

static int RunBytewise(const Kind* kind, const Case* c)
{
    size_t found[MAX_BREAKS];
    size_t count = 0;
    muniSegmentIterator iterator;
    size_t fed = c->length > 0 ? 1 : 0;
    if (kind->init(&iterator, c->text, fed, fed < c->length) != muni_success)
    {
        return 0;
    }
    for (;;)
    {
        size_t offset;
        muniResult status = muniNextSegmentBreak(&iterator, &offset);
        if (status == muni_success && count < MAX_BREAKS)
        {
            found[count++] = offset;
        }
        else if (status == muni_needMoreText)
        {
            if (muniFeedSegmentIterator(&iterator, c->text + fed, 1, fed + 1 < c->length) !=
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

static int RunFind(const Kind* kind, const Case* c)
{
    size_t found[MAX_BREAKS];
    size_t count = 0;
    if (kind->find(c->text, c->length, found, MAX_BREAKS, &count) != muni_success)
    {
        return 0;
    }
    return SameBreaks(found, count, c);
}

// Runs every case of a conformance file three ways.
static void RunConformance(const Kind* kind, int minimumCases)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", MUNI_TEST_DATA, kind->file);
    FILE* file = fopen(path, "r");
    CHECK(file != nullptr, "the conformance file opens");
    if (file == nullptr)
    {
        return;
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
        int ok = RunFind(kind, &c) && RunWhole(kind, &c) && RunBytewise(kind, &c);
        if (!ok && failures < 10)
        {
            printf("%s:%d: %s", kind->file, lineNumber, line);
        }
        failures += ok ? 0 : 1;
    }
    fclose(file);
    printf("%s: %d cases, %d failures\n", kind->file, cases, failures);
    CHECK(cases >= minimumCases && failures == 0, "every conformance case");
}

static void TestConformance(void)
{
    static const Kind grapheme = {"GraphemeBreakTest.txt", muniInitGraphemeIterator,
                                  muniFindGraphemeBreaks};
    static const Kind word = {"WordBreakTest.txt", muniInitWordIterator, muniFindWordBreaks};
    static const Kind sentence = {"SentenceBreakTest.txt", muniInitSentenceIterator,
                                  muniFindSentenceBreaks};
    RunConformance(&grapheme, 800);
    RunConformance(&word, 1800);
    RunConformance(&sentence, 500);
    static const Kind line = {"LineBreakTest.txt", muniInitLineIterator, FindLines};
    RunConformance(&line, 19000);
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

static void TestWordAndSentence(void)
{
    size_t offsets[16];
    size_t count = 0;
    // "can't", ",", " ", "3.14", ".": the apostrophe and the decimal
    // point hold until the next code point shows they join.
    const char words[] = "can't, 3.14.";
    CHECK(muniFindWordBreaks(words, 12, offsets, 16, &count) == muni_success && count == 5 &&
              offsets[0] == 5 && offsets[3] == 11 && offsets[4] == 12,
          "word boundaries around an apostrophe and a decimal point");
    // SB8: after "etc." a lowercase word continues the sentence.
    const char sentences[] = "See etc. and more. Next";
    CHECK(muniFindSentenceBreaks(sentences, 23, offsets, 16, &count) == muni_success &&
              count == 2 && offsets[0] == 19 && offsets[1] == 23,
          "an abbreviation does not end the sentence");
}

static void TestLineBreaks(void)
{
    size_t offsets[16];
    bool mandatory[16];
    size_t count = 0;
    // "Hello ", "world\n", "a-b": after the space, after the newline
    // (mandatory), and the end (mandatory); "a-b" stays whole before a
    // letter only when the hyphen starts a word, so it breaks after "-".
    const char text[] = "Hello world\na-b";
    CHECK(muniFindLineBreaks(text, 15, offsets, mandatory, 16, &count) == muni_success &&
              count == 4,
          "four line break opportunities");
    CHECK(offsets[0] == 6 && !mandatory[0], "after the space, allowed");
    CHECK(offsets[1] == 12 && mandatory[1], "after the newline, mandatory");
    CHECK(offsets[2] == 14 && !mandatory[2], "after the hyphen, allowed");
    CHECK(offsets[3] == 15 && mandatory[3], "the end, mandatory");
    muniSegmentIterator iterator;
    size_t offset;
    bool must = false;
    CHECK(muniInitWordIterator(&iterator, "a b", 3, false) == muni_success, "a word iterator");
    CHECK(muniNextLineBreak(&iterator, &offset, &must) == muni_errorInvalid,
          "line breaks need a line iterator");
    CHECK(muniInitLineIterator(&iterator, "a b", 3, false) == muni_success, "a line iterator");
    CHECK(muniNextLineBreak(&iterator, &offset, &must) == muni_success && offset == 2 && !must,
          "after the space");
}

// "Hello" in Thai, two words: U+0E2A U+0E27 U+0E31 U+0E2A U+0E14 U+0E35
// (18 bytes), then U+0E04 U+0E23 U+0E31 U+0E1A (12 bytes). U+0E31 and
// U+0E35 are combining vowels.
static const char s_thai[] = "\xE0\xB8\xAA\xE0\xB8\xA7\xE0\xB8\xB1\xE0\xB8\xAA\xE0\xB8\x94"
                             "\xE0\xB8\xB5\xE0\xB8\x84\xE0\xB8\xA3\xE0\xB8\xB1\xE0\xB8\x9A";

// A toy dictionary: its context lists the breaks of the run.
static size_t ToyBreaker(void* context, const char* run, size_t length, size_t from)
{
    (void)run;
    for (const size_t* stop = context; *stop != 0; stop++)
    {
        if (*stop > from && *stop < length)
        {
            return *stop;
        }
    }
    return length;
}

static size_t CollectBreaks(muniSegmentIterator* iterator, size_t* offsets)
{
    size_t count = 0;
    size_t offset;
    while (count < 16 && muniNextSegmentBreak(iterator, &offset) == muni_success)
    {
        offsets[count++] = offset;
    }
    return count;
}

static void TestComplexBreaker(void)
{
    static const size_t words[] = {18, 0};
    // 6 lies before the combining vowel U+0E31, which no break may split.
    static const size_t badStop[] = {6, 18, 0};
    size_t offsets[16];
    muniSegmentIterator iterator;
    CHECK(muniInitLineIterator(&iterator, s_thai, 30, false) == muni_success &&
              muniSetComplexBreaker(&iterator, ToyBreaker, (void*)words) == muni_success,
          "a line iterator takes a breaker");
    CHECK(CollectBreaks(&iterator, offsets) == 2 && offsets[0] == 18 && offsets[1] == 30,
          "line breaks between Thai words");
    CHECK(muniInitWordIterator(&iterator, s_thai, 30, false) == muni_success &&
              muniSetComplexBreaker(&iterator, ToyBreaker, (void*)badStop) == muni_success,
          "a word iterator takes a breaker");
    CHECK(CollectBreaks(&iterator, offsets) == 2 && offsets[0] == 18 && offsets[1] == 30,
          "whole Thai words, no break before a mark");
    CHECK(muniInitWordIterator(&iterator, s_thai, 30, false) == muni_success &&
              CollectBreaks(&iterator, offsets) == 7,
          "without a breaker, one word per cluster");
    CHECK(muniInitGraphemeIterator(&iterator, s_thai, 30, false) == muni_success &&
              muniSetComplexBreaker(&iterator, ToyBreaker, nullptr) == muni_errorInvalid,
          "grapheme iterators take no breaker");
    size_t offset;
    CHECK(muniInitLineIterator(&iterator, s_thai, 30, false) == muni_success &&
              muniNextSegmentBreak(&iterator, &offset) == muni_success &&
              muniSetComplexBreaker(&iterator, ToyBreaker, nullptr) == muni_errorInvalid,
          "no breaker after the first break");
}

static void TestFeedingRules(void)
{
    muniSegmentIterator iterator;
    size_t offset;
    CHECK(muniInitWordIterator(&iterator, "ab", 2, false) == muni_success, "init");
    CHECK(muniFeedSegmentIterator(&iterator, "c", 1, false) == muni_errorInvalid,
          "no piece was announced");
    CHECK(muniInitWordIterator(&iterator, "ab", 2, true) == muni_success, "init with more");
    CHECK(muniFeedSegmentIterator(&iterator, "c", 1, false) == muni_errorInvalid,
          "a piece before the iterator asked");
    CHECK(muniNextSegmentBreak(&iterator, &offset) == muni_needMoreText, "the word may go on");
    CHECK(muniFeedSegmentIterator(&iterator, " d", 2, false) == muni_success, "feed");
    CHECK(muniNextSegmentBreak(&iterator, &offset) == muni_success && offset == 2, "after ab");
    muniSegmentIterator zeroed = {{0}};
    CHECK(muniNextSegmentBreak(&zeroed, &offset) == muni_errorInvalid, "never initialized");
}

int main(void)
{
    TestConformance();
    TestGraphemeEdges();
    TestWordAndSentence();
    TestLineBreaks();
    TestComplexBreaker();
    TestFeedingRules();
    return s_failures == 0 ? 0 : 1;
}
