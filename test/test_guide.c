// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The C snippets of docs/user/, each as written there (tools/check_guide.py
// checks it), run on small texts: each function gives a snippet what it
// uses and checks what it keeps. Built with normalization and case, which
// the snippets use; the HarfBuzz snippet runs in test_guide_harfbuzz.c.

#include "test_harness.h"

#include "maul-unicode/bidi.h"
#include "maul-unicode/case.h"
#include "maul-unicode/encoding.h"
#include "maul-unicode/normalize.h"
#include "maul-unicode/properties.h"
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

static void GuideRepair(void)
{
    char buffer[8] = "a\xC0\xAF";
    size_t length = 3;
    size_t capacity = 6;
    size_t repaired = 0;
    muniResult status = muniRepairUtf8(buffer, length, capacity, &repaired);
    if (status == muni_errorCapacity)
    {
        // Grow the buffer to at least repaired bytes, then repair again.
    }
    CHECK(status == muni_errorCapacity && repaired == 7, "the length the repair needs");
    capacity = sizeof(buffer);
    CHECK(muniRepairUtf8(buffer, length, capacity, &repaired) == muni_success && repaired == 7 &&
              memcmp(buffer, "a\xEF\xBF\xBD\xEF\xBF\xBD", 7) == 0,
          "repaired in the grown buffer");
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
        while (muniSegmentIterator_NextBreak(&iterator, &end) == muni_success)
        {
            UseCluster(text + start, end - start);
            start = end;
        }
    }
    CHECK(s_clusters == 1, "one cluster: e and its accent");
}

static size_t s_removedStart;
static size_t s_removedLength;

static void RemoveText(size_t start, size_t length)
{
    s_removedStart = start;
    s_removedLength = length;
}

static void GuideAroundCaret(void)
{
    // "a", e with a combining acute and "b", the caret after the accent.
    static const char text[] = "ae\xCC\x81"
                               "b";
    size_t caret = 4;
    size_t right = 0;
    size_t left = 0;
    muniSegmentIterator clusters;
    CHECK(muniInitGraphemeIterator(&clusters, text, sizeof(text) - 1, false) == muni_success,
          "a grapheme iterator");
    size_t before = 0;
    size_t after = 0;
    if (caret > 0 &&
        muniSegmentIterator_FindBreaksAround(&clusters, caret - 1, &before, &after) == muni_success)
    {
        RemoveText(before, caret - before);
    }
    if (muniSegmentIterator_FindBreaksAround(&clusters, caret, &before, &after) == muni_success)
    {
        right = after;
    }
    if (caret > 0 &&
        muniSegmentIterator_FindBreaksAround(&clusters, caret - 1, &before, &after) == muni_success)
    {
        left = before;
    }
    CHECK(s_removedStart == 1 && s_removedLength == 3 && right == 5 && left == 1,
          "Backspace removes e and its accent; the arrows move past b and before e");
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
        while (muniSegmentIterator_NextLineBreak(&lines, &end, &mandatory) == muni_success)
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

static void GuideLineBreakStrictness(void)
{
    // U+3042, then U+3041 HIRAGANA LETTER SMALL A (Line_Break=CJ).
    const char text[] = "\xE3\x81\x82\xE3\x81\x81";
    muniSegmentIterator lines;
    CHECK(muniInitLineIterator(&lines, text, sizeof(text) - 1, false) == muni_success,
          "a line iterator");
    if (muniSegmentIterator_SetLineBreakStrictness(&lines, muni_lineBreakLoose) != muni_success ||
        muniSegmentIterator_SetWritingSystem(&lines, muni_writingChineseJapanese) != muni_success)
    {
        // lines is not a line iterator, or has already started
    }
    size_t end = 0;
    bool mandatory = false;
    CHECK(muniSegmentIterator_NextLineBreak(&lines, &end, &mandatory) == muni_success && end == 3 &&
              !mandatory,
          "a break before the small kana");
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
    if (muniSegmentIterator_SetComplexBreaker(&lines, BreakThai, dictionary) != muni_success)
    {
        // lines is not a line or word iterator, or has already started
    }
    size_t end = 0;
    bool mandatory = false;
    bool between = false;
    while (muniSegmentIterator_NextLineBreak(&lines, &end, &mandatory) == muni_success)
    {
        between = between || end == 6;
    }
    CHECK(s_asked > 0 && between, "the dictionary's break between the words");
}

static void GuideLanguageConventions(void)
{
    const char text[] = "Ti; Nai\xCD\xBE Ok.";
    size_t length = sizeof(text) - 1;
    static const muniBreakClassOverride greek[] = {
        {0x003B, muni_sbSTerm},
        {0x037E, muni_sbSTerm},
    };
    muniSegmentIterator sentences;
    if (muniInitSentenceIterator(&sentences, text, length, false) == muni_success &&
        muniSegmentIterator_SetBreakClassOverrides(&sentences, greek, 2) == muni_success)
    {
        // a sentence now ends after "; " as after "? "
    }
    size_t end = 0;
    size_t count = 0;
    while (muniSegmentIterator_NextBreak(&sentences, &end) == muni_success)
    {
        count += 1;
    }
    CHECK(count == 3, "three Greek sentences");
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

static void GuideBaseDirection(void)
{
    const char* text = "\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D!";
    size_t length = 9;
    muniBidiDirection direction = muni_bidiAuto;
    muniResult status = muniGetBaseDirection(text, length, &direction, nullptr);
    if (status == muni_success && direction == muni_bidiRightToLeft)
    {
        // Align the line to the right.
    }
    CHECK(status == muni_success && direction == muni_bidiRightToLeft, "a Hebrew line");
}

static void GuideScripts(void)
{
    const char* text = "abc \xCE\xB1\xCE\xB2";
    size_t length = strlen(text);
    muniScriptIterator scripts;
    if (muniInitScriptIterator(&scripts, text, length, false) == muni_success)
    {
        muniScriptRun run;
        while (muniScriptIterator_NextRun(&scripts, &run) == muni_success)
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
        muniGetNfkcCasefold(name, length, muni_convertStrict, key, sizeof(key), &keyLength);
    // "Alice", "ALICE", a full-width "ALICE" and "Al" + soft hyphen + "ice"
    // all get the key "alice".
    CHECK(result.status == muni_success && keyLength == 5 && memcmp(key, "alice", 5) == 0,
          "the key");
}

static void GuideCaseless(void)
{
    const char* typed = "/GIVE";
    size_t typedLength = 5;
    int32_t order = 1;
    muniResult status =
        muniCompareCaseless(typed, typedLength, "/give", 5, muni_caselessIdentifier, &order);
    if (status == muni_success && order == 0)
    {
        // The player typed the command, in any case.
    }
    CHECK(status == muni_success && order == 0, "a typed command");
}

static void GuideOnlyEmoji(void)
{
    // A flag and a ZWJ family: man, woman, girl.
    const char* text = "\xF0\x9F\x87\xB9\xF0\x9F\x87\xB7"
                       "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7";
    size_t length = 26;
    bool onlyEmoji = length > 0;
    muniSegmentIterator clusters;
    (void)muniInitGraphemeIterator(&clusters, text, length, false);
    size_t start = 0;
    size_t end = 0;
    while (onlyEmoji && muniSegmentIterator_NextBreak(&clusters, &end) == muni_success)
    {
        muniEmojiKind kind = muni_emojiKindNone;
        onlyEmoji = muniGetEmojiKind(text + start, end - start, &kind) == muni_success &&
                    kind != muni_emojiKindNone;
        start = end;
    }
    CHECK(onlyEmoji, "a flag and a family are emoji alone");
}

int main(void)
{
    GuideMeasure();
    GuideValidate();
    GuideRepair();
    GuideGraphemes();
    GuideAroundCaret();
    GuideLines();
    GuideLineBreakStrictness();
    GuideComplexBreaker();
    GuideLanguageConventions();
    GuideBidi();
    GuideBaseDirection();
    GuideScripts();
    GuideIdentifierKey();
    GuideCaseless();
    GuideOnlyEmoji();
    return s_failures == 0 ? 0 : 1;
}
