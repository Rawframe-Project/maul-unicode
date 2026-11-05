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
                muniEncodeUtf8((uint32_t)codePoint, out->text + out->length,
                               sizeof(out->text) - out->length, &size) != muni_success)
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
    while (count < MAX_BREAKS && muniSegmentIterator_NextBreak(&iterator, &offset) == muni_success)
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
        muniResult status = muniSegmentIterator_NextBreak(&iterator, &offset);
        if (status == muni_success && count < MAX_BREAKS)
        {
            found[count++] = offset;
        }
        else if (status == muni_needMoreText)
        {
            if (muniSegmentIterator_Feed(&iterator, c->text + fed, 1, fed + 1 < c->length) !=
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
    snprintf(path, sizeof(path), "%s/%s", MUNI_UCD_DATA, kind->file);
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

// Whether the code point at offset is a hard line break, and the one
// before it is not: the anywhere level keeps a line together there
// (LB6), but a break after a hard line break stays mandatory (LB4, LB5).
static bool BeforeHardBreak(const char* text, size_t length, size_t offset)
{
    static const char* const hard[] = {"\r",       "\n",           "\v",          "\f",
                                       "\xC2\x85", "\xE2\x80\xA8", "\xE2\x80\xA9"};
    bool before = false;
    bool after = false;
    for (size_t i = 0; i < sizeof(hard) / sizeof(hard[0]); i++)
    {
        size_t size = strlen(hard[i]);
        before = before || (offset + size <= length && memcmp(text + offset, hard[i], size) == 0);
        after = after || (offset >= size && memcmp(text + offset - size, hard[i], size) == 0);
    }
    return before && !after;
}

static muniResult InitLines(muniSegmentIterator* iterator, const char* text, size_t length,
                            bool moreFollows, muniLineBreakStrictness strictness,
                            muniWritingSystem writingSystem)
{
    muniResult status = muniInitLineIterator(iterator, text, length, moreFollows);
    if (status == muni_success)
    {
        status = muniSegmentIterator_SetLineBreakStrictness(iterator, strictness);
    }
    return status == muni_success ? muniSegmentIterator_SetWritingSystem(iterator, writingSystem)
                                  : status;
}

static muniResult InitStrictLines(muniSegmentIterator* iterator, const char* text, size_t length,
                                  bool moreFollows)
{
    return InitLines(iterator, text, length, moreFollows, muni_lineBreakStrict, muni_writingOther);
}

static muniResult InitStrictCjkLines(muniSegmentIterator* iterator, const char* text, size_t length,
                                     bool moreFollows)
{
    return InitLines(iterator, text, length, moreFollows, muni_lineBreakStrict,
                     muni_writingChineseJapanese);
}

static muniResult InitAnywhereLines(muniSegmentIterator* iterator, const char* text, size_t length,
                                    bool moreFollows)
{
    return InitLines(iterator, text, length, moreFollows, muni_lineBreakAnywhere,
                     muni_writingOther);
}

// Every case of GraphemeBreakTest.txt at the anywhere level, whole and fed
// one byte at a time: its grapheme cluster boundaries but those just
// before a hard line break that does not follow one.
static void RunAnywhere(void)
{
    static const Kind anywhere = {"GraphemeBreakTest.txt", InitAnywhereLines, FindLines};
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", MUNI_UCD_DATA, anywhere.file);
    FILE* file = fopen(path, "r");
    CHECK(file != nullptr, "GraphemeBreakTest.txt opens");
    if (file == nullptr)
    {
        return;
    }
    char line[4096];
    int cases = 0;
    int failures = 0;
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        Case c;
        if (!ParseCase(line, &c))
        {
            continue;
        }
        size_t kept = 0;
        for (size_t i = 0; i < c.breakCount; i++)
        {
            if (!BeforeHardBreak(c.text, c.length, c.breaks[i]))
            {
                c.breaks[kept++] = c.breaks[i];
            }
        }
        c.breakCount = kept;
        cases += 1;
        int ok = RunWhole(&anywhere, &c) && RunBytewise(&anywhere, &c);
        if (!ok && failures++ < 10)
        {
            printf("anywhere, GraphemeBreakTest.txt: %s", line);
        }
    }
    fclose(file);
    printf("anywhere over GraphemeBreakTest.txt: %d cases, %d failures\n", cases, failures);
    CHECK(cases >= 800 && failures == 0, "the anywhere level breaks at grapheme clusters");
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
    // Strict, with any writing system, breaks exactly as the default.
    static const Kind strict = {"LineBreakTest.txt", InitStrictLines, FindLines};
    static const Kind strictCjk = {"LineBreakTest.txt", InitStrictCjkLines, FindLines};
    RunConformance(&strict, 19000);
    RunConformance(&strictCjk, 19000);
    RunAnywhere();
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
    // GB9c as Unicode 18 has it: a linker joins the next consonant, with
    // or without a consonant before it in the cluster; Unicode 15.1, as
    // ICU 74 implements it, wanted one.
    const char conjunct[] = "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7";
    CHECK(muniFindGraphemeBreaks(conjunct, 9, offsets, 4, &count) == muni_success && count == 1,
          "ka virama ssa is one cluster");
    const char cut[] = "\xE0\xA4\x95\n\xE0\xA5\x8D\xE0\xA4\xB7";
    CHECK(muniFindGraphemeBreaks(cut, 10, offsets, 4, &count) == muni_success && count == 3 &&
              offsets[1] == 4 && offsets[2] == 10,
          "a virama after a line feed still joins the next consonant");
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
    CHECK(muniSegmentIterator_NextLineBreak(&iterator, &offset, &must) == muni_errorInvalid,
          "line breaks need a line iterator");
    CHECK(muniInitLineIterator(&iterator, "a b", 3, false) == muni_success, "a line iterator");
    CHECK(muniSegmentIterator_NextLineBreak(&iterator, &offset, &must) == muni_success &&
              offset == 2 && !must,
          "after the space");
}

// Whether the line breaks of text are the expected ones, found whole,
// with an iterator and with an iterator fed one byte at a time.
static bool LineBreaksAre(const char* text, const size_t* breaks, size_t count)
{
    static const Kind line = {"LineBreakTest.txt", muniInitLineIterator, FindLines};
    Case c = {.length = strlen(text), .breakCount = count};
    memcpy(c.text, text, c.length);
    memcpy(c.breaks, breaks, count * sizeof(size_t));
    return RunFind(&line, &c) && RunWhole(&line, &c) && RunBytewise(&line, &c);
}

// LB25 keeps a prefix (PR) or postfix (PO) symbol with an opening bracket
// when an optional infix separator (IS) and a digit follow: (PR | PO) ×
// OP IS? NU. Whether it may break after the symbol is known only at the
// digit, so the answer is held across the bracket and the separator.
static void TestPrefixBeforeNumber(void)
{
    CHECK(LineBreaksAre("$(.5", (size_t[]){4}, 1), "$(.5 stays whole");
    CHECK(LineBreaksAre("$(.a", (size_t[]){1, 4}, 2), "$(.a breaks after the $");
    CHECK(LineBreaksAre("%(.5", (size_t[]){4}, 1), "%(.5 stays whole");
    CHECK(LineBreaksAre("%(.a", (size_t[]){1, 4}, 2), "%(.a breaks after the %");
    CHECK(LineBreaksAre("$(5", (size_t[]){3}, 1), "$(5 stays whole");
    // At most one separator: the second one ends the hold with a break.
    CHECK(LineBreaksAre("$(..5", (size_t[]){1, 5}, 2), "$(..5 breaks after the $");
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
    while (count < 16 && muniSegmentIterator_NextBreak(iterator, &offset) == muni_success)
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
              muniSegmentIterator_SetComplexBreaker(&iterator, ToyBreaker, (void*)words) ==
                  muni_success,
          "a line iterator takes a breaker");
    CHECK(CollectBreaks(&iterator, offsets) == 2 && offsets[0] == 18 && offsets[1] == 30,
          "line breaks between Thai words");
    CHECK(muniInitWordIterator(&iterator, s_thai, 30, false) == muni_success &&
              muniSegmentIterator_SetComplexBreaker(&iterator, ToyBreaker, (void*)badStop) ==
                  muni_success,
          "a word iterator takes a breaker");
    CHECK(CollectBreaks(&iterator, offsets) == 2 && offsets[0] == 18 && offsets[1] == 30,
          "whole Thai words, no break before a mark");
    CHECK(muniInitWordIterator(&iterator, s_thai, 30, false) == muni_success &&
              CollectBreaks(&iterator, offsets) == 7,
          "without a breaker, one word per cluster");
    CHECK(muniInitGraphemeIterator(&iterator, s_thai, 30, false) == muni_success &&
              muniSegmentIterator_SetComplexBreaker(&iterator, ToyBreaker, nullptr) ==
                  muni_errorInvalid,
          "grapheme iterators take no breaker");
    size_t offset;
    CHECK(muniInitLineIterator(&iterator, s_thai, 30, false) == muni_success &&
              muniSegmentIterator_NextBreak(&iterator, &offset) == muni_success &&
              muniSegmentIterator_SetComplexBreaker(&iterator, ToyBreaker, nullptr) ==
                  muni_errorState,
          "no breaker after the first break");
    CHECK(muniInitLineIterator(&iterator, nullptr, 0, true) == muni_success &&
              muniSegmentIterator_NextBreak(&iterator, &offset) == muni_needMoreText &&
              muniSegmentIterator_SetComplexBreaker(&iterator, ToyBreaker, nullptr) ==
                  muni_errorState,
          "no breaker once asked for a break");
    CHECK(muniInitGraphemeIterator(&iterator, s_thai, 30, false) == muni_success &&
              muniSegmentIterator_NextBreak(&iterator, &offset) == muni_success &&
              muniSegmentIterator_SetComplexBreaker(&iterator, ToyBreaker, nullptr) ==
                  muni_errorInvalid,
          "the wrong kind is invalid in any state");
}

// A segmenter that calls its own iterator back before it answers as
// ToyBreaker does, and counts the calls that were not refused as busy and
// the busy calls that wrote an output.
typedef struct Reentry
{
    muniSegmentIterator* iterator;
    const size_t* stops;
    size_t calls;
    size_t notBusy;
    size_t written;
} Reentry;

static void ExpectBusy(Reentry* reentry, muniResult status)
{
    reentry->calls++;
    reentry->notBusy += status != muni_errorBusy;
}

static size_t ReentrantBreaker(void* context, const char* run, size_t length, size_t from)
{
    static const muniBreakClassOverride posix[] = {{0x002E, muni_wbMidNum}};
    static const size_t noStops[] = {0};
    Reentry* reentry = context;
    muniSegmentIterator* iterator = reentry->iterator;
    size_t offset = 77;
    bool mandatory = true;
    ExpectBusy(reentry,
               muniSegmentIterator_SetComplexBreaker(iterator, ToyBreaker, (void*)noStops));
    ExpectBusy(reentry, muniSegmentIterator_Feed(iterator, "a", 1, false));
    ExpectBusy(reentry, muniSegmentIterator_NextBreak(iterator, &offset));
    reentry->written += offset != 77;
    size_t after = 77;
    ExpectBusy(reentry, muniSegmentIterator_FindBreaksAround(iterator, 0, &offset, &after));
    reentry->written += offset != 77 || after != 77;
    if (muniSegmentIterator_SetLineBreakStrictness(iterator, muni_lineBreakLoose) !=
        muni_errorInvalid)
    {
        // A line iterator: the line calls are busy.
        ExpectBusy(reentry,
                   muniSegmentIterator_SetLineBreakStrictness(iterator, muni_lineBreakLoose));
        ExpectBusy(reentry,
                   muniSegmentIterator_SetWritingSystem(iterator, muni_writingChineseJapanese));
        ExpectBusy(reentry, muniSegmentIterator_NextLineBreak(iterator, &offset, &mandatory));
        reentry->written += offset != 77 || !mandatory;
    }
    else
    {
        ExpectBusy(reentry, muniSegmentIterator_SetBreakClassOverrides(iterator, posix, 1));
        muniWordKind kind = 77;
        ExpectBusy(reentry, muniSegmentIterator_GetWordKind(iterator, &kind));
        reentry->written += kind != 77;
    }
    return ToyBreaker((void*)reentry->stops, run, length, from);
}

// A segmenter that calls its iterator back is refused with
// muni_errorBusy, and the outer call finds the breaks it would without
// those calls.
static void TestReentrantBreaker(void)
{
    static const size_t words[] = {18, 0};
    size_t expected[16];
    size_t offsets[16];
    muniSegmentIterator iterator;
    Reentry reentry = {&iterator, words, 0, 0, 0};
    CHECK(muniInitLineIterator(&iterator, s_thai, 30, true) == muni_success &&
              muniSegmentIterator_SetComplexBreaker(&iterator, ToyBreaker, (void*)words) ==
                  muni_success,
          "a plain line iterator");
    size_t count = CollectBreaks(&iterator, expected);
    CHECK(muniInitLineIterator(&iterator, s_thai, 30, true) == muni_success &&
              muniSegmentIterator_SetComplexBreaker(&iterator, ReentrantBreaker, &reentry) ==
                  muni_success,
          "a reentrant line iterator");
    CHECK(CollectBreaks(&iterator, offsets) == count &&
              memcmp(offsets, expected, count * sizeof(size_t)) == 0,
          "a line iterator finds the same breaks");
    CHECK(reentry.calls >= 6 && reentry.notBusy == 0, "every line call back is busy");
    CHECK(reentry.written == 0, "a busy line call leaves its outputs as they were");
    CHECK(muniSegmentIterator_Feed(&iterator, "", 0, false) == muni_success,
          "no longer busy once the outer call returned");

    reentry = (Reentry){&iterator, words, 0, 0, 0};
    CHECK(muniInitWordIterator(&iterator, s_thai, 30, false) == muni_success &&
              muniSegmentIterator_SetComplexBreaker(&iterator, ToyBreaker, (void*)words) ==
                  muni_success,
          "a plain word iterator");
    count = CollectBreaks(&iterator, expected);
    CHECK(muniInitWordIterator(&iterator, s_thai, 30, false) == muni_success &&
              muniSegmentIterator_SetComplexBreaker(&iterator, ReentrantBreaker, &reentry) ==
                  muni_success,
          "a reentrant word iterator");
    CHECK(CollectBreaks(&iterator, offsets) == count &&
              memcmp(offsets, expected, count * sizeof(size_t)) == 0,
          "a word iterator finds the same breaks");
    CHECK(reentry.calls >= 5 && reentry.notBusy == 0, "every word call back is busy");
    CHECK(reentry.written == 0, "a busy word call leaves its output as it was");
}

// The run length the breaker was last given.
static size_t s_runLength;

static size_t RecordingBreaker(void* context, const char* run, size_t length, size_t from)
{
    s_runLength = length;
    return ToyBreaker(context, run, length, from);
}

// Every break the breaker gives inside one run, asked again at each; the
// run ends where SA text does.
static void TestComplexBreakerRuns(void)
{
    // 12 is the very next code point after 9.
    static const size_t threeBreaks[] = {9, 12, 18, 0};
    size_t offsets[16];
    muniSegmentIterator iterator;
    CHECK(muniInitLineIterator(&iterator, s_thai, 30, false) == muni_success &&
              muniSegmentIterator_SetComplexBreaker(&iterator, ToyBreaker, (void*)threeBreaks) ==
                  muni_success &&
              CollectBreaks(&iterator, offsets) == 4 && offsets[0] == 9 && offsets[1] == 12 &&
              offsets[2] == 18 && offsets[3] == 30,
          "breaks inside one run, the next one asked for at each");
    char text[40];
    memcpy(text, s_thai, 30);
    memcpy(text + 30, " abc", 4);
    static const size_t none[] = {0};
    CHECK(muniInitLineIterator(&iterator, text, 34, false) == muni_success &&
              muniSegmentIterator_SetComplexBreaker(&iterator, RecordingBreaker, (void*)none) ==
                  muni_success &&
              CollectBreaks(&iterator, offsets) == 2 && s_runLength == 30,
          "the run ends with the Thai text");
    // A piece cut inside the Thai text, alone in its allocation so that
    // AddressSanitizer sees a read past it: the run ends with the piece.
    char* piece = malloc(15);
    size_t offset = 0;
    bool mandatory = false;
    s_runLength = 0;
    CHECK(piece != nullptr, "a piece");
    if (piece != nullptr)
    {
        memcpy(piece, s_thai, 15);
        CHECK(muniInitLineIterator(&iterator, piece, 15, true) == muni_success &&
                  muniSegmentIterator_SetComplexBreaker(&iterator, RecordingBreaker, (void*)none) ==
                      muni_success &&
                  muniSegmentIterator_NextLineBreak(&iterator, &offset, &mandatory) ==
                      muni_needMoreText &&
                  s_runLength == 15,
              "the run ends with the piece");
        free(piece);
    }
}

// The boundaries of a text fed one byte at a time, each byte alone in its
// allocation, with the given overrides. A piece stays valid until the
// next one has been fed.
static size_t CollectFedBreaks(bool word, const char* text, size_t length,
                               const muniBreakClassOverride* overrides, size_t count,
                               size_t* offsets)
{
    muniSegmentIterator iterator;
    muniResult init = word ? muniInitWordIterator(&iterator, nullptr, 0, true)
                           : muniInitSentenceIterator(&iterator, nullptr, 0, true);
    if (init != muni_success ||
        muniSegmentIterator_SetBreakClassOverrides(&iterator, overrides, count) != muni_success)
    {
        return 0;
    }
    size_t found = 0;
    size_t fed = 0;
    char* piece = nullptr;
    muniResult status;
    for (;;)
    {
        size_t offset;
        status = muniSegmentIterator_NextBreak(&iterator, &offset);
        if (status == muni_success && found < 16)
        {
            offsets[found++] = offset;
            continue;
        }
        if (status != muni_needMoreText || fed == length)
        {
            break;
        }
        char* next = malloc(1);
        if (next == nullptr)
        {
            break;
        }
        *next = text[fed++];
        muniResult feed = muniSegmentIterator_Feed(&iterator, next, 1, fed < length);
        free(piece);
        piece = next;
        if (feed != muni_success)
        {
            break;
        }
    }
    free(piece);
    return status == muni_done ? found : 0;
}

static void TestClassOverrides(void)
{
    // Greek ends a sentence at U+003B and U+037E GREEK QUESTION MARK.
    static const muniBreakClassOverride greek[] = {{0x003B, muni_sbSTerm}, {0x037E, muni_sbSTerm}};
    static const char sentences[] = "Ti; Nai\xCD\xBE Ok.";
    size_t offsets[16];
    muniSegmentIterator iterator;
    CHECK(muniInitSentenceIterator(&iterator, sentences, 13, false) == muni_success &&
              CollectBreaks(&iterator, offsets) == 1,
          "without overrides, U+003B and U+037E continue the sentence");
    // The table alone in its allocation, so AddressSanitizer sees a read
    // past its last entry.
    muniBreakClassOverride* table = malloc(sizeof(greek));
    CHECK(table != nullptr, "a table");
    if (table != nullptr)
    {
        memcpy(table, greek, sizeof(greek));
        CHECK(muniInitSentenceIterator(&iterator, sentences, 13, false) == muni_success &&
                  muniSegmentIterator_SetBreakClassOverrides(&iterator, table, 2) == muni_success &&
                  CollectBreaks(&iterator, offsets) == 3 && offsets[0] == 4 && offsets[1] == 10 &&
                  offsets[2] == 13,
              "as STerm, both end a sentence");
        CHECK(CollectFedBreaks(false, sentences, 13, table, 2, offsets) == 3 && offsets[0] == 4 &&
                  offsets[1] == 10 && offsets[2] == 13,
              "the same fed one byte at a time");
        free(table);
    }

    // POSIX English keeps U+002E out of words, but not out of numbers.
    static const muniBreakClassOverride posix[] = {{0x002E, muni_wbMidNum}};
    CHECK(muniInitWordIterator(&iterator, "a.b 3.5", 7, false) == muni_success &&
              CollectBreaks(&iterator, offsets) == 3 && offsets[0] == 3,
          "without overrides, a.b is one word");
    CHECK(muniInitWordIterator(&iterator, "a.b 3.5", 7, false) == muni_success &&
              muniSegmentIterator_SetBreakClassOverrides(&iterator, posix, 1) == muni_success &&
              CollectBreaks(&iterator, offsets) == 5 && offsets[0] == 1 && offsets[1] == 2 &&
              offsets[2] == 3 && offsets[3] == 4 && offsets[4] == 7,
          "as MidNum, U+002E splits a.b and keeps 3.5");
    CHECK(CollectFedBreaks(true, "a.b 3.5", 7, posix, 1, offsets) == 5 && offsets[1] == 2,
          "the same fed one byte at a time");
    CHECK(muniInitWordIterator(&iterator, "a.b", 3, false) == muni_success &&
              muniSegmentIterator_SetBreakClassOverrides(&iterator, posix, 1) == muni_success &&
              muniSegmentIterator_SetBreakClassOverrides(&iterator, nullptr, 0) == muni_success &&
              CollectBreaks(&iterator, offsets) == 1,
          "no overrides restores the Unicode classes");
}

static void TestClassOverrideArguments(void)
{
    static const muniBreakClassOverride posix[] = {{0x002E, muni_wbMidNum}};
    static const muniBreakClassOverride unordered[] = {{0x003B, muni_sbSTerm},
                                                       {0x002E, muni_sbSTerm}};
    static const muniBreakClassOverride repeated[] = {{0x003B, muni_sbSTerm},
                                                      {0x003B, muni_sbSTerm}};
    static const muniBreakClassOverride pastMax[] = {{0x110000, muni_sbSTerm}};
    static const muniBreakClassOverride surrogate[] = {{0xD800, muni_sbSTerm}};
    static const muniBreakClassOverride wordClass[] = {{0x003B, muni_wbWSegSpace + 1}};
    static const muniBreakClassOverride sentenceClass[] = {{0x003B, muni_sbClose + 1}};
    muniSegmentIterator iterator;
    CHECK(muniSegmentIterator_SetBreakClassOverrides(nullptr, posix, 1) == muni_errorInvalid,
          "no iterator");
    CHECK(muniInitWordIterator(&iterator, "a", 1, false) == muni_success &&
              muniSegmentIterator_SetBreakClassOverrides(&iterator, nullptr, 1) ==
                  muni_errorInvalid,
          "no table with a count");
    CHECK(muniInitWordIterator(&iterator, "a", 1, false) == muni_success &&
              muniSegmentIterator_SetBreakClassOverrides(&iterator, wordClass, 1) ==
                  muni_errorInvalid,
          "past the last Word_Break class");
    CHECK(muniInitSentenceIterator(&iterator, "a", 1, false) == muni_success &&
              muniSegmentIterator_SetBreakClassOverrides(&iterator, sentenceClass, 1) ==
                  muni_errorInvalid,
          "past the last Sentence_Break class");
    CHECK(muniInitSentenceIterator(&iterator, "a", 1, false) == muni_success &&
              muniSegmentIterator_SetBreakClassOverrides(&iterator, unordered, 2) ==
                  muni_errorInvalid,
          "out of order");
    CHECK(muniInitSentenceIterator(&iterator, "a", 1, false) == muni_success &&
              muniSegmentIterator_SetBreakClassOverrides(&iterator, repeated, 2) ==
                  muni_errorInvalid,
          "repeated");
    CHECK(muniInitSentenceIterator(&iterator, "a", 1, false) == muni_success &&
              muniSegmentIterator_SetBreakClassOverrides(&iterator, pastMax, 1) ==
                  muni_errorInvalid,
          "past U+10FFFF");
    // Refused before any entry is read, so the one-entry table is enough.
    CHECK(muniInitWordIterator(&iterator, "a", 1, false) == muni_success &&
              muniSegmentIterator_SetBreakClassOverrides(&iterator, posix, 0x10F801) ==
                  muni_errorInvalid,
          "more overrides than scalar values");
    CHECK(muniInitSentenceIterator(&iterator, "a", 1, false) == muni_success &&
              muniSegmentIterator_SetBreakClassOverrides(&iterator, surrogate, 1) ==
                  muni_errorInvalid,
          "a surrogate");
    CHECK(muniInitGraphemeIterator(&iterator, "a", 1, false) == muni_success &&
              muniSegmentIterator_SetBreakClassOverrides(&iterator, posix, 1) == muni_errorInvalid,
          "grapheme iterators take no overrides");
    CHECK(muniInitLineIterator(&iterator, "a", 1, false) == muni_success &&
              muniSegmentIterator_SetBreakClassOverrides(&iterator, posix, 1) == muni_errorInvalid,
          "line iterators take no overrides");
    size_t offset;
    CHECK(muniInitWordIterator(&iterator, "a b", 3, false) == muni_success &&
              muniSegmentIterator_NextBreak(&iterator, &offset) == muni_success &&
              muniSegmentIterator_SetBreakClassOverrides(&iterator, posix, 1) == muni_errorState,
          "no overrides after the first boundary");
    CHECK(muniInitWordIterator(&iterator, nullptr, 0, true) == muni_success &&
              muniSegmentIterator_NextBreak(&iterator, &offset) == muni_needMoreText &&
              muniSegmentIterator_SetBreakClassOverrides(&iterator, posix, 1) == muni_errorState,
          "no overrides once asked for a boundary");
    CHECK(muniInitWordIterator(&iterator, "a b", 3, false) == muni_success &&
              muniSegmentIterator_NextBreak(&iterator, &offset) == muni_success &&
              muniSegmentIterator_SetBreakClassOverrides(&iterator, unordered, 2) ==
                  muni_errorInvalid,
          "a bad table is invalid in any state");
}

// The two line settings: their defaults, what each call refuses, and
// that they hold once the iterator has reported a break.
static void TestLineSettings(void)
{
    muniSegmentIterator iterator;
    size_t offset;
    CHECK(muniInitLineIterator(&iterator, "a b", 3, false) == muni_success &&
              muniSegmentIterator_GetLineBreakStrictness(&iterator) == muni_lineBreakNormal &&
              muniSegmentIterator_GetWritingSystem(&iterator) == muni_writingOther,
          "a new line iterator is normal and of another writing system");
    bool each = true;
    for (int32_t level = muni_lineBreakNormal; level <= muni_lineBreakAnywhere; level++)
    {
        each = each && muniInitLineIterator(&iterator, "a b", 3, false) == muni_success &&
               muniSegmentIterator_SetLineBreakStrictness(&iterator, level) == muni_success &&
               muniSegmentIterator_SetWritingSystem(&iterator, muni_writingChineseJapanese) ==
                   muni_success &&
               muniSegmentIterator_GetLineBreakStrictness(&iterator) == level &&
               muniSegmentIterator_GetWritingSystem(&iterator) == muni_writingChineseJapanese;
    }
    CHECK(each, "each strictness, with the Chinese or Japanese writing system, reads back");
    const int32_t strictness[] = {-1, INT32_MIN, muni_lineBreakAnywhere + 1, INT32_MAX};
    const int32_t writing[] = {-1, INT32_MIN, muni_writingChineseJapanese + 1, INT32_MAX};
    bool refused = true;
    for (size_t i = 0; i < sizeof(strictness) / sizeof(strictness[0]); i++)
    {
        refused =
            refused && muniInitLineIterator(&iterator, "a b", 3, false) == muni_success &&
            muniSegmentIterator_SetLineBreakStrictness(&iterator, muni_lineBreakLoose) ==
                muni_success &&
            muniSegmentIterator_SetLineBreakStrictness(&iterator, strictness[i]) ==
                muni_errorInvalid &&
            muniSegmentIterator_SetWritingSystem(&iterator, muni_writingChineseJapanese) ==
                muni_success &&
            muniSegmentIterator_SetWritingSystem(&iterator, writing[i]) == muni_errorInvalid &&
            muniSegmentIterator_GetLineBreakStrictness(&iterator) == muni_lineBreakLoose &&
            muniSegmentIterator_GetWritingSystem(&iterator) == muni_writingChineseJapanese;
    }
    CHECK(refused, "an unknown or negative value refused, and the setting kept");
    CHECK(muniSegmentIterator_SetLineBreakStrictness(nullptr, muni_lineBreakLoose) ==
                  muni_errorInvalid &&
              muniSegmentIterator_SetWritingSystem(nullptr, muni_writingChineseJapanese) ==
                  muni_errorInvalid &&
              muniSegmentIterator_GetLineBreakStrictness(nullptr) == muni_lineBreakNormal &&
              muniSegmentIterator_GetWritingSystem(nullptr) == muni_writingOther,
          "no iterator");
    CHECK(muniInitWordIterator(&iterator, "a b", 3, false) == muni_success &&
              muniSegmentIterator_SetLineBreakStrictness(&iterator, muni_lineBreakLoose) ==
                  muni_errorInvalid &&
              muniSegmentIterator_SetWritingSystem(&iterator, muni_writingChineseJapanese) ==
                  muni_errorInvalid &&
              muniSegmentIterator_GetLineBreakStrictness(&iterator) == muni_lineBreakNormal &&
              muniSegmentIterator_GetWritingSystem(&iterator) == muni_writingOther,
          "other kinds of iterator have no line settings");
    CHECK(muniInitLineIterator(&iterator, "a b", 3, false) == muni_success &&
              muniSegmentIterator_SetLineBreakStrictness(&iterator, muni_lineBreakLoose) ==
                  muni_success &&
              muniSegmentIterator_NextBreak(&iterator, &offset) == muni_success &&
              muniSegmentIterator_SetLineBreakStrictness(&iterator, muni_lineBreakStrict) ==
                  muni_errorState &&
              muniSegmentIterator_SetWritingSystem(&iterator, muni_writingChineseJapanese) ==
                  muni_errorState &&
              muniSegmentIterator_GetLineBreakStrictness(&iterator) == muni_lineBreakLoose &&
              muniSegmentIterator_GetWritingSystem(&iterator) == muni_writingOther,
          "no setting after the first break, and the settings stay");
    CHECK(muniInitLineIterator(&iterator, nullptr, 0, true) == muni_success &&
              muniSegmentIterator_NextBreak(&iterator, &offset) == muni_needMoreText &&
              muniSegmentIterator_SetLineBreakStrictness(&iterator, muni_lineBreakLoose) ==
                  muni_errorState &&
              muniSegmentIterator_SetWritingSystem(&iterator, muni_writingChineseJapanese) ==
                  muni_errorState &&
              muniSegmentIterator_GetLineBreakStrictness(&iterator) == muni_lineBreakNormal &&
              muniSegmentIterator_GetWritingSystem(&iterator) == muni_writingOther,
          "no setting once asked for a break, before any was reported");
}

// The line breaks of a text at a strictness and writing system, whole and
// fed one byte at a time; count is 0 when the two disagree.
static size_t LineBreaksAt(const char* text, muniLineBreakStrictness strictness,
                           muniWritingSystem writingSystem, size_t* offsets)
{
    size_t length = strlen(text);
    size_t count = 0;
    muniSegmentIterator iterator;
    if (muniInitLineIterator(&iterator, text, length, false) != muni_success ||
        muniSegmentIterator_SetLineBreakStrictness(&iterator, strictness) != muni_success ||
        muniSegmentIterator_SetWritingSystem(&iterator, writingSystem) != muni_success)
    {
        return 0;
    }
    count = CollectBreaks(&iterator, offsets);
    size_t fed = length > 0 ? 1 : 0;
    size_t again[16];
    size_t againCount = 0;
    if (muniInitLineIterator(&iterator, text, fed, fed < length) != muni_success ||
        muniSegmentIterator_SetLineBreakStrictness(&iterator, strictness) != muni_success ||
        muniSegmentIterator_SetWritingSystem(&iterator, writingSystem) != muni_success)
    {
        return 0;
    }
    for (;;)
    {
        size_t offset;
        muniResult status = muniSegmentIterator_NextBreak(&iterator, &offset);
        if (status == muni_success && againCount < 16)
        {
            again[againCount++] = offset;
        }
        else if (status == muni_needMoreText &&
                 muniSegmentIterator_Feed(&iterator, text + fed, 1, fed + 1 < length) ==
                     muni_success)
        {
            fed += 1;
        }
        else
        {
            break;
        }
    }
    bool same = againCount == count && memcmp(again, offsets, count * sizeof(size_t)) == 0;
    return same ? count : 0;
}

// Whether a text breaks only at its end (first == 0), or before the code
// point at first and at its end, at a strictness and writing system.
static bool BreaksAt(const char* text, muniLineBreakStrictness strictness,
                     muniWritingSystem writingSystem, size_t first)
{
    size_t offsets[16];
    size_t count = LineBreaksAt(text, strictness, writingSystem, offsets);
    size_t length = strlen(text);
    return first == 0 ? count == 1 && offsets[0] == length
                      : count == 2 && offsets[0] == first && offsets[1] == length;
}

// A tailored break, at each strictness and writing system: whether each
// of normal, strict, loose (with the writing system other, then Chinese
// or Japanese) breaks before the code point at first.
typedef struct Tailoring
{
    const char* text;
    size_t first;
    bool normalOther, strictOther, looseOther, normalCjk, strictCjk, looseCjk;
    const char* name;
} Tailoring;

static void TestStrictnessLevels(void)
{
    // あ U+3042 (ID) or 人 U+4EBA (ID) before the code point under test.
    static const Tailoring cases[] = {
        {"\xE3\x81\x82\xE3\x80\x9C", 3, false, false, false, true, false, true, "あ U+301C"},
        {"\xE3\x81\x82\xE3\x82\xA0", 3, false, false, false, true, false, true, "あ U+30A0"},
        {"\xE3\x81\x82\xE2\x80\x90", 3, false, false, true, false, false, true, "あ U+2010"},
        {"\xE3\x81\x82\xE2\x80\x93", 3, false, false, true, false, false, true, "あ U+2013"},
        {"a\xE2\x80\x90", 1, false, false, false, false, false, false, "a U+2010: not after ID"},
        {"\xE3\x81\x82\xE3\x81\x81", 3, false, false, true, false, false, true, "あ ぁ (CJ)"},
        {"\xE3\x81\x82\xE3\x83\xBC", 3, false, false, true, false, false, true, "あ ー (CJ)"},
        {"\xE4\xBA\xBA\xE3\x80\x85", 3, false, false, true, false, false, true, "人 U+3005"},
        {"\xE4\xBA\xBA\xE3\x80\xBB", 3, false, false, true, false, false, true, "人 U+303B"},
        {"\xE3\x81\x82\xE3\x82\x9D", 3, false, false, true, false, false, true, "あ U+309D"},
        {"\xE3\x81\x82\xE3\x82\x9E", 3, false, false, true, false, false, true, "あ U+309E"},
        {"\xE3\x82\xA2\xE3\x83\xBD", 3, false, false, true, false, false, true, "ア U+30FD"},
        {"\xE3\x82\xA2\xE3\x83\xBE", 3, false, false, true, false, false, true, "ア U+30FE"},
        {"\xE2\x80\xA5\xE2\x80\xA6", 3, false, false, true, false, false, true, "U+2025 U+2026"},
        {"\xE3\x81\x82\xE2\x80\xA6", 3, false, false, false, false, false, false,
         "あ U+2026: IN only after IN"},
        {"\xE3\x81\x82\xE3\x83\xBB", 3, false, false, false, false, false, true, "あ U+30FB"},
        {"\xE3\x81\x82\xEF\xBC\x9A", 3, false, false, false, false, false, true, "あ U+FF1A"},
        {"\xE3\x81\x82\xEF\xBC\x9B", 3, false, false, false, false, false, true, "あ U+FF1B"},
        {"\xE3\x81\x82\xEF\xBD\xA5", 3, false, false, false, false, false, true, "あ U+FF65"},
        {"\xE3\x81\x82\xE2\x80\xBC", 3, false, false, false, false, false, true, "あ U+203C"},
        {"\xE3\x81\x82\xE2\x81\x87", 3, false, false, false, false, false, true, "あ U+2047"},
        {"\xE3\x81\x82\xE2\x81\x88", 3, false, false, false, false, false, true, "あ U+2048"},
        {"\xE3\x81\x82\xE2\x81\x89", 3, false, false, false, false, false, true, "あ U+2049"},
        {"\xE3\x81\x82\xEF\xBC\x81", 3, false, false, false, false, false, true, "あ U+FF01"},
        {"\xE3\x81\x82\xEF\xBC\x9F", 3, false, false, false, false, false, true, "あ U+FF1F"},
        {"\xE3\x81\x82\xEF\xBC\x85", 3, false, false, false, false, false, true,
         "あ ％: a wide postfix"},
        {"1%", 0, false, false, false, false, false, false, "1%: a narrow postfix"},
        {"\xEF\xBF\xA5\xE3\x81\x82", 3, false, false, false, false, false, true,
         "￥ あ: after a wide prefix"},
        {"$\xE3\x81\x82", 0, false, false, false, false, false, false, "$ あ: a narrow prefix"},
        {"a\xEF\xBC\x85", 1, false, false, false, false, false, true,
         "a ％: a wide postfix after a letter"},
        {"1\xE2\x80\xB0", 1, false, false, false, false, false, true, "1 ‰: an ambiguous postfix"},
        {"\xE3\x81\x82\xEF\xB9\xAA", 3, false, false, false, false, false, true,
         "あ ﹪: a wide postfix"},
        {"\xE2\x82\xAC\xE3\x81\x82", 3, false, false, false, false, false, true,
         "€ あ: after an ambiguous prefix"},
        {"\xEF\xB9\xA9\xE3\x81\x82", 3, false, false, false, false, false, true,
         "﹩ あ: after a wide prefix"},
        {"\xEF\xBF\xA5\xE2\x80\xA6", 3, false, false, false, false, false, true,
         "￥ U+2026: before an ellipsis"},
        {"\xEF\xBF\xA5\xEF\xBC\x89", 0, false, false, false, false, false, false,
         "￥ ）: LB13 after a prefix"},
        {"\xEF\xBF\xA5\xE3\x80\x81", 0, false, false, false, false, false, false,
         "￥ 、: LB13 after a prefix"},
        {"\xEF\xBF\xA5\xEF\xBC\x81", 3, false, false, false, false, false, true,
         "￥ ！: centered punctuation, whatever LB13 says"},
        {"\xEF\xBC\x88\xE3\x80\x9C", 0, false, false, false, false, false, false,
         "（ U+301C: LB14 keeps an opening bracket"},
        {"\xE2\x80\x9C\xE3\x80\x9C", 0, false, false, false, false, false, false,
         "“ U+301C: LB15a keeps an initial quotation mark"},
        {"\xE3\x81\x82\xE2\x81\xA0\xE3\x80\x9C", 0, false, false, false, false, false, false,
         "あ WJ U+301C: LB11 keeps the word joiner"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        const Tailoring* c = &cases[i];
        size_t at = c->first;
        bool ok =
            BreaksAt(c->text, muni_lineBreakNormal, muni_writingOther, c->normalOther ? at : 0) &&
            BreaksAt(c->text, muni_lineBreakStrict, muni_writingOther, c->strictOther ? at : 0) &&
            BreaksAt(c->text, muni_lineBreakLoose, muni_writingOther, c->looseOther ? at : 0) &&
            BreaksAt(c->text, muni_lineBreakNormal, muni_writingChineseJapanese,
                     c->normalCjk ? at : 0) &&
            BreaksAt(c->text, muni_lineBreakStrict, muni_writingChineseJapanese,
                     c->strictCjk ? at : 0) &&
            BreaksAt(c->text, muni_lineBreakLoose, muni_writingChineseJapanese,
                     c->looseCjk ? at : 0);
        CHECK(ok, c->name);
    }
}

// The anywhere level breaks between any two grapheme clusters, but not
// just before a hard line break, which stays mandatory.
static void TestAnywhere(void)
{
    static const struct
    {
        const char* text;
        size_t breaks[8];
        size_t count;
        const char* name;
    } cases[] = {
        {"ab", {1, 2}, 2, "between letters"},
        {"a b", {1, 2, 3}, 3, "around a space"},
        {"e\xCC\x81x", {3, 4}, 2, "not before a combining mark"},
        {"a\xE2\x81\xA0"
         "b",
         {1, 4, 5},
         3,
         "around a word joiner"},
        {"a\xC2\xA0"
         "b",
         {1, 3, 4},
         3,
         "around a no-break space"},
        {"\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8",
         {8, 16},
         2,
         "between flags, not inside one"},
        {"a\nb", {2, 3}, 2, "not before a line feed"},
        {"a\r\nb", {3, 4}, 2, "not inside CR LF"},
        {"a\xE2\x80\xA8"
         "b",
         {4, 5},
         2,
         "not before a line separator, after it"},
        {"a\fb", {2, 3}, 2, "not before a form feed, after it"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        size_t offsets[16];
        size_t count =
            LineBreaksAt(cases[i].text, muni_lineBreakAnywhere, muni_writingOther, offsets);
        CHECK(count == cases[i].count &&
                  memcmp(offsets, cases[i].breaks, count * sizeof(size_t)) == 0,
              cases[i].name);
    }
    muniSegmentIterator iterator;
    size_t offset = 0;
    bool mandatory = false;
    CHECK(muniInitLineIterator(&iterator, "a\nb", 3, false) == muni_success &&
              muniSegmentIterator_SetLineBreakStrictness(&iterator, muni_lineBreakAnywhere) ==
                  muni_success &&
              muniSegmentIterator_NextLineBreak(&iterator, &offset, &mandatory) == muni_success &&
              offset == 2 && mandatory &&
              muniSegmentIterator_NextLineBreak(&iterator, &offset, &mandatory) == muni_success &&
              offset == 3 && mandatory,
          "a line feed stays a mandatory break");
    CHECK(muniInitLineIterator(&iterator, "a\fb", 3, false) == muni_success &&
              muniSegmentIterator_SetLineBreakStrictness(&iterator, muni_lineBreakAnywhere) ==
                  muni_success &&
              muniSegmentIterator_NextLineBreak(&iterator, &offset, &mandatory) == muni_success &&
              offset == 2 && mandatory,
          "so does a form feed (LB4)");
}

// Every segment call refuses a NULL iterator, output or text with a
// length, and writes no output.
static void TestInvalidArguments(void)
{
    static const InitFn inits[] = {muniInitGraphemeIterator, muniInitWordIterator,
                                   muniInitSentenceIterator, muniInitLineIterator};
    static const FindFn finds[] = {muniFindGraphemeBreaks, muniFindWordBreaks,
                                   muniFindSentenceBreaks, FindLines};
    muniSegmentIterator iterator;
    size_t offsets[2] = {7, 7};
    for (size_t i = 0; i < 4; i++)
    {
        CHECK(inits[i](nullptr, "a", 1, false) == muni_errorInvalid, "init no iterator");
        CHECK(inits[i](&iterator, nullptr, 1, false) == muni_errorInvalid,
              "init no text with a length");
        CHECK(inits[i](&iterator, nullptr, 0, false) == muni_success, "init no text and no length");
        size_t count = 7;
        CHECK(finds[i](nullptr, 1, offsets, 2, &count) == muni_errorInvalid && count == 7 &&
                  offsets[0] == 7,
              "find in no text with a length");
        CHECK(finds[i]("a", 1, nullptr, 2, &count) == muni_errorInvalid && count == 7,
              "find into no offsets with a capacity");
        CHECK(finds[i]("a", 1, offsets, 2, nullptr) == muni_errorInvalid && offsets[0] == 7,
              "find with no count");
    }
    size_t offset = 7;
    bool mandatory = true;
    CHECK(muniSegmentIterator_SetComplexBreaker(nullptr, ToyBreaker, nullptr) == muni_errorInvalid,
          "a breaker for no iterator");
    CHECK(muniInitGraphemeIterator(&iterator, "a", 1, false) == muni_success &&
              muniSegmentIterator_SetComplexBreaker(&iterator, ToyBreaker, nullptr) ==
                  muni_errorInvalid,
          "grapheme iterators take no breaker");
    CHECK(muniSegmentIterator_NextBreak(nullptr, &offset) == muni_errorInvalid && offset == 7,
          "the next break of no iterator");
    CHECK(muniSegmentIterator_NextBreak(&iterator, nullptr) == muni_errorInvalid,
          "the next break into nothing");
    CHECK(muniSegmentIterator_NextLineBreak(nullptr, &offset, &mandatory) == muni_errorInvalid &&
              offset == 7 && mandatory,
          "the next line break of no iterator");
    CHECK(muniInitLineIterator(&iterator, "a", 1, false) == muni_success &&
              muniSegmentIterator_NextLineBreak(&iterator, nullptr, &mandatory) ==
                  muni_errorInvalid &&
              mandatory,
          "the next line break with no offset out");
    CHECK(muniSegmentIterator_NextLineBreak(&iterator, &offset, nullptr) == muni_errorInvalid &&
              offset == 7,
          "the next line break with no flag out");
    CHECK(muniInitWordIterator(&iterator, "a b", 3, false) == muni_success &&
              muniSegmentIterator_NextLineBreak(&iterator, &offset, &mandatory) ==
                  muni_errorInvalid &&
              offset == 7 && mandatory,
          "the next line break of a word iterator");
    bool flags[2] = {true, true};
    size_t count = 7;
    CHECK(muniFindLineBreaks("a", 1, nullptr, flags, 2, &count) == muni_errorInvalid &&
              count == 7 && flags[0],
          "line breaks into no offsets with a capacity");
}

static void TestFeedingRules(void)
{
    muniSegmentIterator iterator;
    size_t offset;
    CHECK(muniInitWordIterator(&iterator, "ab", 2, false) == muni_success, "init");
    CHECK(muniSegmentIterator_Feed(&iterator, "c", 1, false) == muni_errorState,
          "no piece was announced");
    CHECK(muniInitWordIterator(&iterator, "ab", 2, true) == muni_success, "init with more");
    CHECK(muniSegmentIterator_Feed(&iterator, "c", 1, false) == muni_errorState,
          "a piece before the iterator asked");
    CHECK(muniSegmentIterator_Feed(&iterator, nullptr, 1, false) == muni_errorInvalid,
          "a NULL piece is invalid in any state");
    CHECK(muniSegmentIterator_NextBreak(&iterator, &offset) == muni_needMoreText,
          "the word may go on");
    CHECK(muniSegmentIterator_Feed(&iterator, " d", 2, false) == muni_success, "feed");
    CHECK(muniSegmentIterator_NextBreak(&iterator, &offset) == muni_success && offset == 2,
          "after ab");
    muniSegmentIterator zeroed = {{0}};
    CHECK(muniSegmentIterator_NextBreak(&zeroed, &offset) == muni_errorInvalid,
          "never initialized");
}

// An iterator no init call set up, of one byte repeated: zeroed as a
// static one is, or of bytes that would make a count run past the state
// (0x41) or a flag no bool (0x02, 0x04). Every call that takes it back
// refuses it as a bad argument and changes nothing in it, and the getters
// give their defaults.
static void TestUninitializedIterators(void)
{
    static const unsigned char fills[] = {0x00, 0x41, 0x02, 0x04};
    static const muniBreakClassOverride posix[] = {{'_', muni_wbExtendNumLet}};
    static const size_t stops[] = {0};
    for (size_t i = 0; i < sizeof(fills); i++)
    {
        muniSegmentIterator iterator;
        memset(&iterator, fills[i], sizeof(iterator));
        muniSegmentIterator before = iterator;
        size_t offset = 7;
        bool mandatory = true;
        muniWordKind kind = muni_wordLetter;
        bool refused =
            muniSegmentIterator_NextBreak(&iterator, &offset) == muni_errorInvalid &&
            muniSegmentIterator_NextLineBreak(&iterator, &offset, &mandatory) ==
                muni_errorInvalid &&
            muniSegmentIterator_Feed(&iterator, "a", 1, false) == muni_errorInvalid &&
            muniSegmentIterator_SetComplexBreaker(&iterator, ToyBreaker, (void*)stops) ==
                muni_errorInvalid &&
            muniSegmentIterator_SetBreakClassOverrides(&iterator, posix, 1) == muni_errorInvalid &&
            muniSegmentIterator_SetLineBreakStrictness(&iterator, muni_lineBreakLoose) ==
                muni_errorInvalid &&
            muniSegmentIterator_SetWritingSystem(&iterator, muni_writingChineseJapanese) ==
                muni_errorInvalid &&
            muniSegmentIterator_GetWordKind(&iterator, &kind) == muni_errorInvalid &&
            muniSegmentIterator_FindBreaksAround(&iterator, 0, &offset, &offset) ==
                muni_errorInvalid;
        bool defaults =
            muniSegmentIterator_GetLineBreakStrictness(&iterator) == muni_lineBreakNormal &&
            muniSegmentIterator_GetWritingSystem(&iterator) == muni_writingOther;
        CHECK(refused && defaults && offset == 7 && mandatory && kind == muni_wordLetter &&
                  memcmp(&iterator, &before, sizeof(iterator)) == 0,
              "an iterator no init call set up is refused and left as it was");
    }
    // A copy of an iterator an init call set up is one too.
    muniSegmentIterator iterator;
    size_t offset = 0;
    CHECK(muniInitWordIterator(&iterator, "a b", 3, false) == muni_success, "init");
    muniSegmentIterator copy = iterator;
    CHECK(muniSegmentIterator_NextBreak(&copy, &offset) == muni_success && offset == 1,
          "a copy of an iterator steps on");
}

// A kind is an int32_t, so a caller can pass a negative one; every call
// that takes a kind refuses -1 and INT32_MIN as it refuses a kind past the
// last, and leaves its outputs as they were.
static const int32_t s_negative[] = {-1, INT32_MIN};

static void TestNegativeClasses(void)
{
    bool refused = true;
    for (size_t i = 0; i < sizeof(s_negative) / sizeof(s_negative[0]); i++)
    {
        const muniBreakClassOverride table[] = {{0x003B, s_negative[i]}};
        muniSegmentIterator iterator;
        size_t offset = 77;
        refused =
            refused && muniInitWordIterator(&iterator, "a;b", 3, false) == muni_success &&
            muniSegmentIterator_SetBreakClassOverrides(&iterator, table, 1) == muni_errorInvalid &&
            muniSegmentIterator_NextBreak(&iterator, &offset) == muni_success &&
            muniInitSentenceIterator(&iterator, "a;b", 3, false) == muni_success &&
            muniSegmentIterator_SetBreakClassOverrides(&iterator, table, 1) == muni_errorInvalid &&
            muniSegmentIterator_NextBreak(&iterator, &offset) == muni_success;
    }
    CHECK(refused, "a negative class in an override refused, and the iterator still works");
}

// What muniMeasureUtf8 should give: the last boundary within both limits
// among the code points muniDecodeUtf8 steps over (weighed as UTF-16
// units for muni_unitUtf16) or the clusters muniFindGraphemeBreaks
// reports, and its count of units.
static size_t ExpectedMeasure(const char* text, size_t length, muniTextUnit unit, size_t maxBytes,
                              size_t maxUnits, size_t* unitsOut)
{
    size_t bytes = 0;
    size_t units = 0;
    if (unit != muni_unitGraphemeCluster)
    {
        size_t offset = 0;
        size_t count = 0;
        while (offset < length)
        {
            uint32_t codePoint;
            size_t size;
            bool decoded =
                muniDecodeUtf8(text + offset, length - offset, &codePoint, &size) == muni_success;
            offset += size;
            count += unit == muni_unitUtf16 && decoded && codePoint > 0xFFFF ? 2 : 1;
            if (offset <= maxBytes && count <= maxUnits)
            {
                bytes = offset;
                units = count;
            }
        }
        *unitsOut = units;
        return bytes;
    }
    size_t breaks[64];
    size_t count = 0;
    if (muniFindGraphemeBreaks(text, length, breaks, 64, &count) != muni_success)
    {
        return SIZE_MAX;
    }
    for (size_t i = 0; i < count && breaks[i] <= maxBytes && i < maxUnits; i++)
    {
        bytes = breaks[i];
        units = i + 1;
    }
    *unitsOut = units;
    return bytes;
}

// muniMeasureUtf8 keeps a code point or a cluster whole: not a sequence
// cut in two, a base cut from its mark, an emoji sequence joined by U+200D
// cut at the joiner or a flag cut between its regional indicators. Each
// text is a heap allocation of exactly its length, so a read past it is
// seen, and every pair of limits gives the boundary the stepping and the
// break finder agree on, with and without a count of units.
static void TestMeasureLimits(void)
{
    static const struct
    {
        const char* text;
        size_t maxBytes;
        size_t codePoint;
        size_t cluster;
        const char* what;
    } s_cases[] = {
        {"abc", 2, 2, 2, "ASCII"},
        {"a\xC3\xA9", 2, 1, 1, "not inside a two-byte sequence"},
        {"e\xCC\x81x", 2, 1, 0, "not between a base and its mark"},
        {"e\xCC\x81x", 3, 3, 3, "a base and its mark"},
        {"\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9!", 10, 7, 0,
         "not inside an emoji ZWJ sequence"},
        {"\xF0\x9F\x87\xB9\xF0\x9F\x87\xB7", 7, 4, 0, "not between two regional indicators"},
        {"\xE1\x80"
         "A",
         1, 0, 0, "a maximal ill-formed subpart is one unit"},
        {"\xE1\x80"
         "A",
         2, 2, 2, "and fits whole"},
        {"abc", 3, 3, 3, "a limit of the whole text"},
        {"abc", 9, 3, 3, "a limit past the text"},
        {"abc", 0, 0, 0, "a limit of 0"},
    };
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); i++)
    {
        size_t length = strlen(s_cases[i].text);
        char* text = malloc(length);
        CHECK(text != nullptr, "allocation");
        if (text == nullptr)
        {
            return;
        }
        memcpy(text, s_cases[i].text, length);
        size_t codePoint = 99;
        size_t cluster = 99;
        CHECK(muniMeasureUtf8(text, length, muni_unitCodePoint, s_cases[i].maxBytes, SIZE_MAX,
                              &codePoint, nullptr) == muni_success &&
                  codePoint == s_cases[i].codePoint &&
                  muniMeasureUtf8(text, length, muni_unitGraphemeCluster, s_cases[i].maxBytes,
                                  SIZE_MAX, &cluster, nullptr) == muni_success &&
                  cluster == s_cases[i].cluster,
              s_cases[i].what);
        size_t failures = 0;
        for (muniTextUnit unit = muni_unitCodePoint; unit <= muni_unitUtf16; unit++)
        {
            for (size_t maxBytes = 0; maxBytes <= length + 1; maxBytes++)
            {
                for (size_t maxUnits = 0; maxUnits <= length + 2; maxUnits++)
                {
                    size_t limit = maxUnits == length + 2 ? SIZE_MAX : maxUnits;
                    size_t units = 99;
                    size_t expectedUnits = 0;
                    size_t expected =
                        ExpectedMeasure(text, length, unit, maxBytes, limit, &expectedUnits);
                    size_t bytes = 99;
                    size_t alone = 99;
                    failures += muniMeasureUtf8(text, length, unit, maxBytes, limit, &bytes,
                                                &units) == muni_success &&
                                        muniMeasureUtf8(text, length, unit, maxBytes, limit, &alone,
                                                        nullptr) == muni_success &&
                                        bytes == expected && alone == expected &&
                                        units == expectedUnits
                                    ? 0
                                    : 1;
                }
            }
        }
        CHECK(failures == 0, "every pair of limits stops at the last boundary within both");
        free(text);
    }
}

// The four uses the header names, on "a", U+1F600 and "é": one, four and
// two bytes, one, two and one UTF-16 units.
static void TestMeasureUses(void)
{
    static const char text[] = "a\xF0\x9F\x98\x80\xC3\xA9";
    size_t bytes = 99;
    size_t units = 99;
    CHECK(muniMeasureUtf8(text, 7, muni_unitUtf16, 5, SIZE_MAX, &bytes, &units) == muni_success &&
              bytes == 5 && units == 3,
          "a byte offset as UTF-16 units");
    CHECK(muniMeasureUtf8(text, 7, muni_unitCodePoint, 5, SIZE_MAX, &bytes, &units) ==
                  muni_success &&
              bytes == 5 && units == 2,
          "a byte offset as code points");
    CHECK(muniMeasureUtf8(text, 7, muni_unitUtf16, 3, SIZE_MAX, &bytes, &units) == muni_success &&
              bytes == 1 && units == 1,
          "an offset inside a code point counts the units before it");
    CHECK(muniMeasureUtf8(text, 7, muni_unitUtf16, SIZE_MAX, 3, &bytes, &units) == muni_success &&
              bytes == 5 && units == 3,
          "UTF-16 units as a byte offset");
    CHECK(muniMeasureUtf8(text, 7, muni_unitUtf16, SIZE_MAX, 2, &bytes, &units) == muni_success &&
              bytes == 1 && units == 1,
          "a count inside a surrogate pair ends before the pair");
    CHECK(muniMeasureUtf8(text, 7, muni_unitUtf16, SIZE_MAX, 9, &bytes, &units) == muni_success &&
              bytes == 7 && units == 4,
          "a count past the text ends at its end");
    units = 99;
    CHECK(muniMeasureUtf8(text, 7, muni_unitCodePoint, 6, SIZE_MAX, &bytes, nullptr) ==
                  muni_success &&
              bytes == 5 && units == 99,
          "a cut to a buffer");
    // e and U+0301, x, a flag: three clusters of 3, 1 and 8 bytes.
    static const char clusters[] = "e\xCC\x81x\xF0\x9F\x87\xB9\xF0\x9F\x87\xB7";
    CHECK(muniMeasureUtf8(clusters, 12, muni_unitGraphemeCluster, SIZE_MAX, SIZE_MAX, &bytes,
                          &units) == muni_success &&
              bytes == 12 && units == 3,
          "the clusters of a whole text");
    CHECK(muniMeasureUtf8(clusters, 12, muni_unitGraphemeCluster, SIZE_MAX, 2, &bytes, &units) ==
                  muni_success &&
              bytes == 4 && units == 2,
          "where a count of clusters ends");
}

static void TestMeasureRefusals(void)
{
    size_t bytes = 7;
    size_t units = 7;
    CHECK(muniMeasureUtf8(nullptr, 0, muni_unitGraphemeCluster, 5, SIZE_MAX, &bytes, &units) ==
                  muni_success &&
              bytes == 0 && units == 0,
          "empty text");
    bytes = 7;
    units = 7;
    CHECK(muniMeasureUtf8(nullptr, 1, muni_unitCodePoint, 5, SIZE_MAX, &bytes, &units) ==
                  muni_errorInvalid &&
              bytes == 7 && units == 7,
          "no text with a length is refused and leaves the outputs");
    CHECK(muniMeasureUtf8("a", 1, 3, 5, SIZE_MAX, &bytes, &units) == muni_errorInvalid &&
              bytes == 7 && units == 7,
          "an unknown unit");
    CHECK(muniMeasureUtf8("a", 1, -1, 5, SIZE_MAX, &bytes, &units) == muni_errorInvalid &&
              bytes == 7 && units == 7,
          "a negative unit");
    CHECK(muniMeasureUtf8("a", 1, muni_unitUtf16, 5, SIZE_MAX, nullptr, nullptr) == muni_success,
          "no outputs");
}

// Collects the boundaries of a word iterator over text and the kind of
// each segment, fed whole or one byte at a time; returns the count.
static size_t CollectKinds(const char* text, size_t length, bool bytewise, size_t* offsets,
                           muniWordKind* kinds)
{
    muniSegmentIterator iterator;
    size_t fed = bytewise ? (length > 0 ? 1 : 0) : length;
    if (muniInitWordIterator(&iterator, text, fed, fed < length) != muni_success)
    {
        return SIZE_MAX;
    }
    size_t count = 0;
    for (;;)
    {
        size_t offset;
        muniResult status = muniSegmentIterator_NextBreak(&iterator, &offset);
        if (status == muni_needMoreText)
        {
            if (muniSegmentIterator_Feed(&iterator, text + fed, 1, fed + 1 < length) !=
                muni_success)
            {
                return SIZE_MAX;
            }
            fed += 1;
            continue;
        }
        if (status != muni_success || count == 16 ||
            muniSegmentIterator_GetWordKind(&iterator, &kinds[count]) != muni_success)
        {
            return status == muni_done ? count : SIZE_MAX;
        }
        offsets[count++] = offset;
    }
}

// A word segment's kind is the highest of its code points': letters with
// digits are letters, connectors and marks join but make no word, and a
// held boundary gives the text after it to the next segment when the hold
// fails. Fed one byte at a time, the iterator gives the same kinds.
static void TestWordKinds(void)
{
    enum
    {
        N = muni_wordNone,
        D = muni_wordNumber,
        L = muni_wordLetter,
        K = muni_wordKana,
        I = muni_wordIdeograph,
    };
    static const struct
    {
        const char* text;
        size_t count;
        size_t offsets[8];
        muniWordKind kinds[8];
        const char* what;
    } s_cases[] = {
        {"abc 123", 3, {3, 4, 7}, {L, N, D}, "a word, a space, a number"},
        {"ab12 12ab", 3, {4, 5, 9}, {L, N, L}, "letters with digits are letters"},
        {"\xC2\xB2", 1, {2}, {N}, "a superscript digit is no number"},
        {"a.b", 1, {3}, {L}, "a joined hold"},
        {"a.", 2, {1, 2}, {L, N}, "a hold that fails at the end"},
        {"a.1", 3, {1, 2, 3}, {L, N, D}, "a hold that fails before a digit"},
        {"3.14 3.", 4, {4, 5, 6, 7}, {D, N, D, N}, "numbers, joined and not"},
        {"foo_bar _", 3, {7, 8, 9}, {L, N, N}, "a connector joins but makes no word"},
        {"e\xCC\x81 \xCC\x81", 2, {3, 6}, {L, N}, "a mark joins but makes no word"},
        {"\xCC\x81"
         "a",
         2,
         {2, 3},
         {N, L},
         "a mark at the start makes no word"},
        {"\xD7\x90\"\xD7\x91", 1, {5}, {L}, "a Hebrew acronym"},
        {"\xE3\x82\xAB\xE3\x83\x8A", 1, {6}, {K}, "katakana"},
        {"\xE3\x81\xB2\xE3\x82\x89", 2, {3, 6}, {K, K}, "hiragana"},
        {"\xE6\xBC\xA2\xE5\xAD\x97", 2, {3, 6}, {I, I}, "ideographs"},
        {"\xE0\xB8\x81", 1, {3}, {L}, "a Thai letter"},
        {"\r\n!", 2, {2, 3}, {N, N}, "a line break and a mark of punctuation"},
    };
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); i++)
    {
        size_t length = strlen(s_cases[i].text);
        for (int bytewise = 0; bytewise <= 1; bytewise++)
        {
            size_t offsets[16];
            muniWordKind kinds[16];
            size_t count = CollectKinds(s_cases[i].text, length, bytewise != 0, offsets, kinds);
            CHECK(count == s_cases[i].count &&
                      memcmp(offsets, s_cases[i].offsets, count * sizeof(size_t)) == 0 &&
                      memcmp(kinds, s_cases[i].kinds, count * sizeof(muniWordKind)) == 0,
                  s_cases[i].what);
        }
    }
    // The word rules skip the property lookups for Word_Break=Other below
    // U+0E01, which holds while no code point there makes a word.
    size_t early = 0;
    for (uint32_t c = 0; c < 0x0E01; c++)
    {
        early +=
            muniGetWordBreak(c) == muni_wbOther &&
            (muniIsIdeographic(c) || muniGetScript(c) == MUNI_MAKE_SCRIPT('H', 'i', 'r', 'a') ||
             muniGetLineBreak(c) == muni_lbSa);
    }
    CHECK(early == 0, "no Word_Break=Other code point below U+0E01 makes a word");
    muniSegmentIterator iterator;
    muniWordKind kind = 77;
    CHECK(muniInitWordIterator(&iterator, "ab", 2, false) == muni_success &&
              muniSegmentIterator_GetWordKind(&iterator, &kind) == muni_success &&
              kind == muni_wordNone,
          "no kind before the first boundary");
    kind = 77;
    CHECK(muniSegmentIterator_GetWordKind(nullptr, &kind) == muni_errorInvalid && kind == 77,
          "no iterator");
    CHECK(muniSegmentIterator_GetWordKind(&iterator, nullptr) == muni_errorInvalid, "no kind out");
    CHECK(muniInitGraphemeIterator(&iterator, "ab", 2, false) == muni_success &&
              muniSegmentIterator_GetWordKind(&iterator, &kind) == muni_errorInvalid && kind == 77,
          "a grapheme iterator has no word kinds");
}

// A segmenter for runs of random text: a break every six bytes, two SA
// code points.
static size_t StepBreaker(void* context, const char* run, size_t length, size_t from)
{
    (void)context;
    (void)run;
    return from + 6 < length ? from + 6 : length;
}

static muniResult SetupWordOverrides(muniSegmentIterator* iterator, const char* text, size_t length)
{
    // Spaces skipped as Format, hyphens inside words, full stops in numbers.
    static const muniBreakClassOverride table[] = {
        {' ', muni_wbFormat}, {'-', muni_wbMidLetter}, {'.', muni_wbMidNum}};
    muniResult status = muniInitWordIterator(iterator, text, length, false);
    return status == muni_success ? muniSegmentIterator_SetBreakClassOverrides(iterator, table, 3)
                                  : status;
}

static muniResult SetupWordBreaker(muniSegmentIterator* iterator, const char* text, size_t length)
{
    muniResult status = muniInitWordIterator(iterator, text, length, false);
    return status == muni_success
               ? muniSegmentIterator_SetComplexBreaker(iterator, StepBreaker, nullptr)
               : status;
}

static muniResult SetupSentenceOverrides(muniSegmentIterator* iterator, const char* text,
                                         size_t length)
{
    // Commas separate paragraphs, Greek ends a sentence at U+003B.
    static const muniBreakClassOverride table[] = {{',', muni_sbSep}, {';', muni_sbSTerm}};
    muniResult status = muniInitSentenceIterator(iterator, text, length, false);
    return status == muni_success ? muniSegmentIterator_SetBreakClassOverrides(iterator, table, 2)
                                  : status;
}

static muniResult SetupLineBreaker(muniSegmentIterator* iterator, const char* text, size_t length)
{
    muniResult status = muniInitLineIterator(iterator, text, length, false);
    return status == muni_success
               ? muniSegmentIterator_SetComplexBreaker(iterator, StepBreaker, nullptr)
               : status;
}

static muniResult SetupStrictLines(muniSegmentIterator* iterator, const char* text, size_t length)
{
    return InitStrictLines(iterator, text, length, false);
}

static muniResult SetupLooseLines(muniSegmentIterator* iterator, const char* text, size_t length)
{
    return InitLines(iterator, text, length, false, muni_lineBreakLoose, muni_writingOther);
}

static muniResult SetupNormalCjkLines(muniSegmentIterator* iterator, const char* text,
                                      size_t length)
{
    return InitLines(iterator, text, length, false, muni_lineBreakNormal,
                     muni_writingChineseJapanese);
}

static muniResult SetupLooseCjkLines(muniSegmentIterator* iterator, const char* text, size_t length)
{
    return InitLines(iterator, text, length, false, muni_lineBreakLoose,
                     muni_writingChineseJapanese);
}

static muniResult SetupAnywhereLines(muniSegmentIterator* iterator, const char* text, size_t length)
{
    return InitAnywhereLines(iterator, text, length, false);
}

typedef muniResult (*SetupFn)(muniSegmentIterator* iterator, const char* text, size_t length);

static muniResult SetupGraphemes(muniSegmentIterator* iterator, const char* text, size_t length)
{
    return muniInitGraphemeIterator(iterator, text, length, false);
}

static muniResult SetupWords(muniSegmentIterator* iterator, const char* text, size_t length)
{
    return muniInitWordIterator(iterator, text, length, false);
}

static muniResult SetupSentences(muniSegmentIterator* iterator, const char* text, size_t length)
{
    return muniInitSentenceIterator(iterator, text, length, false);
}

static muniResult SetupLines(muniSegmentIterator* iterator, const char* text, size_t length)
{
    return muniInitLineIterator(iterator, text, length, false);
}

// Every kind and setting muniSegmentIterator_FindBreaksAround serves.
static const SetupFn s_graphemeSetups[] = {SetupGraphemes, SetupAnywhereLines};
static const SetupFn s_wordSetups[] = {SetupWords, SetupWordOverrides, SetupWordBreaker};
static const SetupFn s_sentenceSetups[] = {SetupSentences, SetupSentenceOverrides};
static const SetupFn s_lineSetups[] = {SetupLines,        SetupLineBreaker,    SetupStrictLines,
                                       SetupLooseLines,   SetupNormalCjkLines, SetupLooseCjkLines,
                                       SetupAnywhereLines};

// Whether muniSegmentIterator_FindBreaksAround gives, at every offset of
// text, the boundaries stepping from the start finds: on the iterator as
// set up for the odd offsets, and on one stepped to the end for the even
// ones. The text sits alone in its allocation, so AddressSanitizer sees a
// read past its end.
static bool AroundMatches(SetupFn setup, const char* source, size_t length)
{
    char* text = malloc(length > 0 ? length : 1);
    size_t* breaks = malloc((length + 1) * sizeof(size_t));
    bool same = text != nullptr && breaks != nullptr;
    muniSegmentIterator iterator;
    memset(&iterator, 0, sizeof(iterator));
    if (same)
    {
        memcpy(text, source, length);
        same = setup(&iterator, text, length) == muni_success;
    }
    size_t count = 0;
    muniSegmentIterator stepped = iterator;
    size_t offset;
    while (same && muniSegmentIterator_NextBreak(&stepped, &offset) == muni_success)
    {
        same = count < length;
        breaks[count++] = offset;
    }
    size_t next = 0;
    for (size_t at = 0; same && at <= length; at++)
    {
        while (next < count && breaks[next] <= at)
        {
            next++;
        }
        size_t before = next > 0 ? breaks[next - 1] : 0;
        size_t after = next < count ? breaks[next] : length;
        muniSegmentIterator kept = at % 2 == 0 ? stepped : iterator;
        size_t foundBefore = 0;
        size_t foundAfter = 0;
        same = muniSegmentIterator_FindBreaksAround(&kept, at, &foundBefore, &foundAfter) ==
                   muni_success &&
               foundBefore == before && foundAfter == after &&
               memcmp(&kept, at % 2 == 0 ? &stepped : &iterator, sizeof(kept)) == 0;
        if (!same)
        {
            printf("offset %zu: %zu and %zu, expected %zu and %zu\n", at, foundBefore, foundAfter,
                   before, after);
        }
    }
    free(text);
    free(breaks);
    return same;
}

static bool AroundMatchesAll(const SetupFn* setups, size_t setupCount, const char* text,
                             size_t length)
{
    for (size_t i = 0; i < setupCount; i++)
    {
        if (!AroundMatches(setups[i], text, length))
        {
            printf("setup %zu, %zu bytes\n", i, length);
            return false;
        }
    }
    return true;
}

#define AROUND_JOINED 8192

// Every case of a conformance file, and its cases joined into one text of
// up to AROUND_JOINED bytes, where the call backs up across cases.
static void RunAround(const char* name, const SetupFn* setups, size_t setupCount)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", MUNI_UCD_DATA, name);
    FILE* file = fopen(path, "r");
    CHECK(file != nullptr, "the conformance file opens");
    char* joined = malloc(AROUND_JOINED);
    if (file == nullptr || joined == nullptr)
    {
        if (file != nullptr)
        {
            fclose(file);
        }
        free(joined);
        return;
    }
    size_t joinedLength = 0;
    char line[4096];
    int cases = 0;
    int failures = 0;
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        Case c;
        if (!ParseCase(line, &c))
        {
            continue;
        }
        cases += 1;
        if (!AroundMatchesAll(setups, setupCount, c.text, c.length) && failures++ < 10)
        {
            printf("%s: %s", name, line);
        }
        if (joinedLength + c.length <= AROUND_JOINED)
        {
            memcpy(joined + joinedLength, c.text, c.length);
            joinedLength += c.length;
        }
    }
    fclose(file);
    printf("%s around every offset: %d cases, %d failures\n", name, cases, failures);
    CHECK(cases >= 500 && failures == 0, "the boundaries around every offset of every case");
    CHECK(joinedLength >= AROUND_JOINED / 4 &&
              AroundMatchesAll(setups, setupCount, joined, joinedLength),
          "the boundaries around every offset of the cases joined");
    free(joined);
}

// Adds the code points of a conformance file to a bit set.
static void CollectCodePoints(const char* name, uint8_t* seen)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", MUNI_UCD_DATA, name);
    FILE* file = fopen(path, "r");
    if (file == nullptr)
    {
        return;
    }
    char line[4096];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        for (char* cursor = line; *cursor != '\0' && *cursor != '#';)
        {
            char* end = nullptr;
            unsigned long codePoint = strtoul(cursor, &end, 16);
            if (end != cursor && codePoint <= 0x10FFFF)
            {
                seen[codePoint >> 3] |= (uint8_t)(1u << (codePoint & 7u));
                cursor = end;
            }
            else
            {
                cursor++;
            }
        }
    }
    fclose(file);
}

#define AROUND_TEXTS 600
#define AROUND_LONG  16

// Random texts of the code points the conformance files use, SA letters
// and marks, and ill-formed UTF-8, under every kind and setting: short
// ones, and AROUND_LONG of about a kilobyte, where the call backs up far
// from the start.
static void RunAroundRandom(void)
{
    static const char* const files[] = {"GraphemeBreakTest.txt", "WordBreakTest.txt",
                                        "SentenceBreakTest.txt", "LineBreakTest.txt"};
    static const uint32_t extra[] = {0x0E01, 0x0E2A, 0x0E31, 0x0E48, 0x0EAA, 0x1000,
                                     0x102D, 0x1780, 0x17B6, 0x0020, 0x000A, 0x2029};
    static const char* const broken[] = {"\x80",         "\xBF\xBF", "\xE0\xB8",
                                         "\xF0\x9F\x91", "\xC0",     "\xFF"};
    uint8_t* seen = calloc(0x110000 / 8, 1);
    uint32_t* pool = malloc(0x110000 * sizeof(uint32_t));
    CHECK(seen != nullptr && pool != nullptr, "the pool");
    if (seen == nullptr || pool == nullptr)
    {
        free(seen);
        free(pool);
        return;
    }
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++)
    {
        CollectCodePoints(files[i], seen);
    }
    size_t poolSize = 0;
    for (uint32_t codePoint = 0; codePoint <= 0x10FFFF; codePoint++)
    {
        if ((seen[codePoint >> 3] >> (codePoint & 7u) & 1u) != 0)
        {
            pool[poolSize++] = codePoint;
        }
    }
    for (size_t i = 0; i < sizeof(extra) / sizeof(extra[0]); i++)
    {
        pool[poolSize++] = extra[i];
    }
    uint32_t state = 0x2400u;
    int failures = 0;
    for (int n = 0; n < AROUND_TEXTS + AROUND_LONG; n++)
    {
        char text[2048];
        size_t length = 0;
        state = state * 1103515245u + 12345u;
        size_t units = n < AROUND_TEXTS ? 1 + (state >> 16) % 48 : 400;
        for (size_t u = 0; u < units; u++)
        {
            state = state * 1103515245u + 12345u;
            uint32_t pick = state >> 8;
            size_t size = 0;
            if (pick % 16 == 0)
            {
                const char* piece = broken[(pick >> 4) % (sizeof(broken) / sizeof(broken[0]))];
                size = strlen(piece);
                memcpy(text + length, piece, size);
            }
            else if (muniEncodeUtf8(pool[(pick >> 4) % poolSize], text + length,
                                    sizeof(text) - length, &size) != muni_success)
            {
                size = 0;
            }
            length += size;
        }
        bool same = AroundMatchesAll(s_graphemeSetups, 2, text, length) &&
                    AroundMatchesAll(s_wordSetups, 3, text, length) &&
                    AroundMatchesAll(s_sentenceSetups, 2, text, length) &&
                    AroundMatchesAll(s_lineSetups, 7, text, length);
        failures += same ? 0 : 1;
    }
    printf("around every offset of %d random texts: %d failures\n", AROUND_TEXTS + AROUND_LONG,
           failures);
    CHECK(failures == 0, "the boundaries around every offset of random texts");
    free(seen);
    free(pool);
}

static void TestBreaksAround(void)
{
    RunAround("GraphemeBreakTest.txt", s_graphemeSetups, 2);
    RunAround("WordBreakTest.txt", s_wordSetups, 3);
    RunAround("SentenceBreakTest.txt", s_sentenceSetups, 2);
    RunAround("LineBreakTest.txt", s_lineSetups, 7);
    RunAroundRandom();
}

// An iterator that does not hold its whole text, a NULL argument and an
// offset past the end are refused, and the outputs left as they were.
static void TestBreaksAroundRefusals(void)
{
    muniSegmentIterator iterator;
    size_t before = 7;
    size_t after = 7;
    CHECK(muniInitWordIterator(&iterator, "ab cd", 5, false) == muni_success &&
              muniSegmentIterator_FindBreaksAround(&iterator, 4, &before, &after) == muni_success &&
              before == 3 && after == 5,
          "the word around offset 4");
    CHECK(muniSegmentIterator_FindBreaksAround(&iterator, 5, &before, &after) == muni_success &&
              before == 5 && after == 5,
          "the end of the text");
    before = 7;
    after = 7;
    CHECK(muniSegmentIterator_FindBreaksAround(nullptr, 0, &before, &after) == muni_errorInvalid &&
              muniSegmentIterator_FindBreaksAround(&iterator, 0, nullptr, &after) ==
                  muni_errorInvalid &&
              muniSegmentIterator_FindBreaksAround(&iterator, 0, &before, nullptr) ==
                  muni_errorInvalid &&
              muniSegmentIterator_FindBreaksAround(&iterator, 6, &before, &after) ==
                  muni_errorInvalid &&
              before == 7 && after == 7,
          "a NULL argument or an offset past the end is invalid");
    CHECK(muniInitWordIterator(&iterator, "", 0, false) == muni_success &&
              muniSegmentIterator_FindBreaksAround(&iterator, 0, &before, &after) == muni_success &&
              before == 0 && after == 0,
          "an empty text");
    before = 7;
    after = 7;
    CHECK(muniInitWordIterator(&iterator, "ab", 2, true) == muni_success &&
              muniSegmentIterator_FindBreaksAround(&iterator, 1, &before, &after) ==
                  muni_errorState &&
              muniSegmentIterator_FindBreaksAround(&iterator, 3, &before, &after) ==
                  muni_errorInvalid,
          "more text to follow is a state error, after an offset past the end");
    size_t offset;
    CHECK(muniSegmentIterator_NextBreak(&iterator, &offset) == muni_needMoreText &&
              muniSegmentIterator_Feed(&iterator, " cd", 3, false) == muni_success &&
              muniSegmentIterator_FindBreaksAround(&iterator, 1, &before, &after) ==
                  muni_errorState &&
              before == 7 && after == 7,
          "a last piece that is not the whole text is a state error");
}

int main(void)
{
    TestConformance();
    TestGraphemeEdges();
    TestWordAndSentence();
    TestLineBreaks();
    TestPrefixBeforeNumber();
    TestComplexBreaker();
    TestComplexBreakerRuns();
    TestReentrantBreaker();
    TestClassOverrides();
    TestClassOverrideArguments();
    TestNegativeClasses();
    TestLineSettings();
    TestStrictnessLevels();
    TestAnywhere();
    TestInvalidArguments();
    TestFeedingRules();
    TestUninitializedIterators();
    TestWordKinds();
    TestMeasureLimits();
    TestMeasureUses();
    TestMeasureRefusals();
    TestBreaksAround();
    TestBreaksAroundRefusals();
    return s_failures == 0 ? 0 : 1;
}
