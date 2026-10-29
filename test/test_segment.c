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
    CHECK(muniInitGraphemeIterator(&iterator, s_thai, 30, false) == muni_success &&
              muniSegmentIterator_NextBreak(&iterator, &offset) == muni_success &&
              muniSegmentIterator_SetComplexBreaker(&iterator, ToyBreaker, nullptr) ==
                  muni_errorInvalid,
          "the wrong kind is invalid in any state");
}

// A segmenter that calls its own iterator back before it answers as
// ToyBreaker does, and counts the calls that were not refused as busy.
typedef struct Reentry
{
    muniSegmentIterator* iterator;
    const size_t* stops;
    size_t calls;
    size_t notBusy;
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
    size_t offset;
    bool mandatory;
    ExpectBusy(reentry,
               muniSegmentIterator_SetComplexBreaker(iterator, ToyBreaker, (void*)noStops));
    ExpectBusy(reentry, muniSegmentIterator_Feed(iterator, "a", 1, false));
    ExpectBusy(reentry, muniSegmentIterator_NextBreak(iterator, &offset));
    if (muniSegmentIterator_EnableCjAsIdeographic(iterator, true) != muni_errorInvalid)
    {
        // A line iterator: the line calls are busy.
        ExpectBusy(reentry, muniSegmentIterator_EnableCjAsIdeographic(iterator, true));
        ExpectBusy(reentry, muniSegmentIterator_NextLineBreak(iterator, &offset, &mandatory));
    }
    else
    {
        ExpectBusy(reentry, muniSegmentIterator_SetBreakClassOverrides(iterator, posix, 1));
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
    Reentry reentry = {&iterator, words, 0, 0};
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
    CHECK(reentry.calls >= 5 && reentry.notBusy == 0, "every line call back is busy");
    CHECK(muniSegmentIterator_Feed(&iterator, "", 0, false) == muni_success,
          "no longer busy once the outer call returned");

    reentry = (Reentry){&iterator, words, 0, 0};
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
    CHECK(reentry.calls >= 4 && reentry.notBusy == 0, "every word call back is busy");
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

static void TestCjAsIdeographic(void)
{
    // U+3042 HIRAGANA LETTER A (ID), then U+3041 HIRAGANA LETTER SMALL A
    // and U+30FC KATAKANA-HIRAGANA PROLONGED SOUND MARK (both CJ).
    static const char kana[] = "\xE3\x81\x82\xE3\x81\x81\xE3\x83\xBC";
    size_t offsets[16];
    muniSegmentIterator iterator;
    CHECK(muniInitLineIterator(&iterator, kana, 9, false) == muni_success &&
              CollectBreaks(&iterator, offsets) == 1 && offsets[0] == 9,
          "by default CJ is NS: no break before the small kana or the mark");
    CHECK(muniInitLineIterator(&iterator, kana, 9, false) == muni_success &&
              !muniSegmentIterator_IsCjAsIdeographicEnabled(&iterator),
          "off in a new line iterator");
    CHECK(muniInitLineIterator(&iterator, kana, 9, false) == muni_success &&
              muniSegmentIterator_EnableCjAsIdeographic(&iterator, true) == muni_success &&
              muniSegmentIterator_IsCjAsIdeographicEnabled(&iterator) &&
              CollectBreaks(&iterator, offsets) == 3 && offsets[0] == 3 && offsets[1] == 6 &&
              offsets[2] == 9,
          "as ID, a line may break before each");
    CHECK(muniInitLineIterator(&iterator, kana, 9, false) == muni_success &&
              muniSegmentIterator_EnableCjAsIdeographic(&iterator, true) == muni_success &&
              muniSegmentIterator_EnableCjAsIdeographic(&iterator, false) == muni_success &&
              !muniSegmentIterator_IsCjAsIdeographicEnabled(&iterator) &&
              CollectBreaks(&iterator, offsets) == 1,
          "switched off again, the default rules");
    size_t offset;
    CHECK(muniSegmentIterator_EnableCjAsIdeographic(nullptr, true) == muni_errorInvalid &&
              !muniSegmentIterator_IsCjAsIdeographicEnabled(nullptr),
          "no iterator");
    CHECK(muniInitWordIterator(&iterator, kana, 9, false) == muni_success &&
              muniSegmentIterator_EnableCjAsIdeographic(&iterator, true) == muni_errorInvalid &&
              !muniSegmentIterator_IsCjAsIdeographicEnabled(&iterator),
          "word iterators have no such switch");
    CHECK(muniInitLineIterator(&iterator, kana, 9, false) == muni_success &&
              muniSegmentIterator_EnableCjAsIdeographic(&iterator, true) == muni_success &&
              muniSegmentIterator_NextBreak(&iterator, &offset) == muni_success &&
              muniSegmentIterator_EnableCjAsIdeographic(&iterator, false) == muni_errorState &&
              muniSegmentIterator_IsCjAsIdeographicEnabled(&iterator),
          "no switching after the first break, and the switch stays on");
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
    TestCjAsIdeographic();
    TestInvalidArguments();
    TestFeedingRules();
    return s_failures == 0 ? 0 : 1;
}
