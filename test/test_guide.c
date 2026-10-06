// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The guide's C snippets (docs/guide.md), each as written there
// (tools/check_guide.py checks it, family record 0019), run on small
// texts: each function gives a snippet what it uses and checks what it
// keeps. Built with normalization and case, which the snippets use;
// the HarfBuzz snippet runs in test_guide_harfbuzz.c.

#include "test_harness.h"

#include "maul-unicode/bidi.h"
#include "maul-unicode/case.h"
#include "maul-unicode/encoding.h"
#include "maul-unicode/normalize.h"
#include "maul-unicode/script.h"
#include "maul-unicode/segment.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// "e" and a combining acute accent, whose NFC is the two bytes of "é".
static const char kDecomposed[] = "e\xCC\x81";

static void GuideMeasure(void)
{
    const char* text = kDecomposed;
    size_t length = sizeof(kDecomposed) - 1;
    size_t needed = 0;
    muniTextResult result =
        muniNormalize(text, length, muni_nfc, muni_convertStrict, nullptr, 0, &needed);
    // result.status is muni_errorCapacity (or muni_success for empty output);
    // allocate needed bytes and call again.
    CHECK(result.status == muni_errorCapacity && needed == 2, "measured, then allocated");
}

static void GuideValidate(void)
{
    const char* text = "a\xC0\xAF";
    size_t length = 3;
    muniTextResult valid = muniValidateUtf8(text, length);
    if (valid.status != muni_success)
    {
        // valid.status says what is wrong (muni_errorUtf8Overlong, ...),
        // valid.offset where.
    }
    CHECK(valid.status == muni_errorUtf8Overlong && valid.offset == 1, "what and where");
}

// What the program does with each cluster: here, counts them.
static int s_clusters;

static void UseCluster(const char* cluster, size_t length)
{
    (void)cluster;
    s_clusters += length > 0 ? 1 : 0;
}

static void GuideGraphemes(void)
{
    const char* text = kDecomposed;
    size_t length = sizeof(kDecomposed) - 1;
    muniSegmentIterator iterator;
    if (muniInitGraphemeIterator(&iterator, text, length, false) == muni_success)
    {
        size_t start = 0;
        size_t end = 0;
        while (muniNextSegmentBreak(&iterator, &end) == muni_success)
        {
            UseCluster(text + start, end - start);
            start = end;
        }
    }
    CHECK(s_clusters == 1, "one cluster: e and its accent");
}

static void GuideLines(void)
{
    const char* text = "one two\nthree";
    size_t length = strlen(text);
    muniSegmentIterator lines;
    if (muniInitLineIterator(&lines, text, length, false) == muni_success)
    {
        size_t end = 0;
        bool mandatory = false;
        while (muniNextLineBreak(&lines, &end, &mandatory) == muni_success)
        {
            // a line may end at end; it must when mandatory is true
        }
    }
}

// A dictionary of Thai words, all of the same length here, and how
// often the iterator asked it.
typedef struct ThaiDictionary
{
    size_t wordBytes;
} ThaiDictionary;

static int s_asked;

static size_t FindWordBreak(const ThaiDictionary* dictionary, const char* run, size_t length,
                            size_t from)
{
    (void)run;
    s_asked += 1;
    size_t next = (from / dictionary->wordBytes + 1) * dictionary->wordBytes;
    return next < length ? next : length;
}

static size_t BreakThai(void* context, const char* run, size_t length, size_t from)
{
    const ThaiDictionary* dictionary = context;
    return FindWordBreak(dictionary, run, length, from);
}

static void GuideComplexBreaker(void)
{
    // Four Thai consonants, three bytes each, read as two words of two.
    const char text[] = "\xE0\xB8\x81\xE0\xB8\x82\xE0\xB8\x84\xE0\xB8\x87";
    size_t length = sizeof(text) - 1;
    static ThaiDictionary words = {.wordBytes = 6};
    ThaiDictionary* dictionary = &words;
    muniSegmentIterator lines;
    CHECK(muniInitLineIterator(&lines, text, length, false) == muni_success, "a line iterator");
    if (muniSetComplexBreaker(&lines, BreakThai, dictionary) != muni_success)
    {
        // lines is not a line or word iterator, or has already started
    }
    size_t end = 0;
    bool mandatory = false;
    bool between = false;
    while (muniNextLineBreak(&lines, &end, &mandatory) == muni_success)
    {
        between = between || end == 6;
    }
    CHECK(s_asked > 0 && between, "the dictionary's break between the words");
}

static void GuideBidi(void)
{
    const char* text = "abc";
    size_t length = 3;
    uint8_t levels[3];
    uint8_t workspace[3];
    size_t paragraphLength = 0;
    uint8_t paragraphLevel = 0;
    muniResult status = muniResolveBidi(text, length, muni_bidiAuto, levels, workspace,
                                        &paragraphLength, &paragraphLevel);
    CHECK(status == muni_success && paragraphLength == 3 && paragraphLevel == 0 && levels[0] == 0,
          "a left-to-right paragraph");
}

static void GuideScripts(void)
{
    const char* text = "abc \xCE\xB1\xCE\xB2";
    size_t length = strlen(text);
    muniScriptIterator scripts;
    if (muniInitScriptIterator(&scripts, text, length, false) == muni_success)
    {
        muniScriptRun run;
        while (muniNextScriptRun(&scripts, &run) == muni_success)
        {
            // the text up to run.end has script run.script
        }
    }
}

static void GuideIdentifierKey(void)
{
    const char* name = "ALICE";
    size_t length = 5;
    char key[32];
    size_t keyLength = 0;
    muniTextResult result =
        muniToNfkcCasefold(name, length, muni_convertStrict, key, sizeof(key), &keyLength);
    // "Alice", "ALICE", a full-width "ALICE" and "Al" + soft hyphen + "ice"
    // all get the key "alice".
    CHECK(result.status == muni_success && keyLength == 5 && memcmp(key, "alice", 5) == 0,
          "the key");
}

int main(void)
{
    GuideMeasure();
    GuideValidate();
    GuideGraphemes();
    GuideLines();
    GuideComplexBreaker();
    GuideBidi();
    GuideScripts();
    GuideIdentifierKey();
    return s_failures == 0 ? 0 : 1;
}
