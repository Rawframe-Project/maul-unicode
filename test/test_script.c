// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Script runs: plain runs, Common and Inherited code points, extensions,
// brackets around foreign text, and text fed one byte at a time.

#include "test_harness.h"

#include "maul-unicode/script.h"

#include <string.h>

#define LATN MUNI_MAKE_SCRIPT('L', 'a', 't', 'n')
#define HEBR MUNI_MAKE_SCRIPT('H', 'e', 'b', 'r')
#define DEVA MUNI_MAKE_SCRIPT('D', 'e', 'v', 'a')
#define BENG MUNI_MAKE_SCRIPT('B', 'e', 'n', 'g')
#define HANI MUNI_MAKE_SCRIPT('H', 'a', 'n', 'i')
#define HIRA MUNI_MAKE_SCRIPT('H', 'i', 'r', 'a')
#define GREK MUNI_MAKE_SCRIPT('G', 'r', 'e', 'k')

// Whether text gives exactly the expected runs, whole and byte by byte.
static bool Runs(const char* text, const muniScriptRun* expected, size_t count)
{
    size_t length = strlen(text);
    muniScriptRun runs[16];
    size_t found = 0;
    if (muniFindScriptRuns(text, length, runs, 16, &found) != muni_success || found != count)
    {
        return false;
    }
    for (size_t i = 0; i < count; i++)
    {
        if (runs[i].end != expected[i].end || runs[i].script != expected[i].script)
        {
            return false;
        }
    }
    muniScriptIterator iterator;
    size_t fed = length > 0 ? 1 : 0;
    if (muniInitScriptIterator(&iterator, text, fed, fed < length) != muni_success)
    {
        return false;
    }
    size_t index = 0;
    for (;;)
    {
        muniScriptRun run;
        muniResult status = muniScriptIterator_NextRun(&iterator, &run);
        if (status == muni_needMoreText)
        {
            if (muniScriptIterator_Feed(&iterator, text + fed, 1, fed + 1 < length) != muni_success)
            {
                return false;
            }
            fed += 1;
            continue;
        }
        if (status != muni_success)
        {
            return status == muni_done && index == count;
        }
        if (index == count || run.end != expected[index].end ||
            run.script != expected[index].script)
        {
            return false;
        }
        index += 1;
    }
}

static void TestRuns(void)
{
    CHECK(Runs("Hello, world", (muniScriptRun[]){{12, LATN}}, 1), "one Latin run");
    CHECK(Runs("", nullptr, 0), "no runs in empty text");
    CHECK(Runs("123 !", (muniScriptRun[]){{5, MUNI_SCRIPT_COMMON}}, 1), "only Common");
    CHECK(Runs("123 abc", (muniScriptRun[]){{7, LATN}}, 1), "leading Common joins");
    CHECK(Runs("e\xCC\x81t\xC3\xA9", (muniScriptRun[]){{6, LATN}}, 1), "a combining mark joins");
    // "abc " then Hebrew U+05E9 U+05DC U+05D5 U+05DD, " def".
    CHECK(Runs("abc \xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D def",
               (muniScriptRun[]){{4, LATN}, {13, HEBR}, {16, LATN}}, 3),
          "Latin, Hebrew, Latin");
    // Hebrew, " (", Latin "abc", ") x": the closing bracket goes with the
    // opening one.
    CHECK(Runs("\xD7\xA9\xD7\x9C (abc) x",
               (muniScriptRun[]){{6, HEBR}, {9, LATN}, {11, HEBR}, {12, LATN}}, 4),
          "brackets keep their opening script");
    // Devanagari KA, the danda U+0964 (many Indic scripts), Bengali KA.
    CHECK(Runs("\xE0\xA4\x95\xE0\xA5\xA4\xE0\xA6\x95", (muniScriptRun[]){{6, DEVA}, {9, BENG}}, 2),
          "the danda narrows to Devanagari");
    // U+65E5 U+672C (Han), U+306E (Hiragana).
    CHECK(Runs("\xE6\x97\xA5\xE6\x9C\xAC\xE3\x81\xAE", (muniScriptRun[]){{6, HANI}, {9, HIRA}}, 2),
          "Han and Hiragana");
}

// A run that could still be several scripts takes the first Script of its
// own, among its characters, that the run could still be when that
// character came, while the run can still be that script, and otherwise
// the script whose tag comes first alphabetically.
static void TestSharedScripts(void)
{
    // U+30FC, Common with the extensions Hiragana and Katakana, alone and
    // before a digit.
    CHECK(Runs("\xE3\x83\xBC", (muniScriptRun[]){{3, HIRA}}, 1),
          "a run only of a sign shared by two scripts");
    CHECK(Runs("\xE3\x83\xBC"
               "1",
               (muniScriptRun[]){{4, HIRA}}, 1),
          "a shared sign before a digit");
    // U+00B7, Common with extensions that hold Coptic and Greek, then
    // U+0375, Greek with the extensions Coptic and Greek: the run can be
    // either, and the first Script of its own is Greek.
    CHECK(Runs("\xC2\xB7\xCD\xB5", (muniScriptRun[]){{4, GREK}}, 1),
          "a run that starts neutral takes the first Script of its own");
}

// The bracket stack: a closing bracket matches only its own opening one,
// popped once; an opening one before the run's first script takes it;
// one that matches nothing is neutral.
static void TestBrackets(void)
{
    // Hebrew, " (", "abc", ")", " x", a second ")" that nothing opened, " d".
    CHECK(Runs("\xD7\xA9\xD7\x9C (abc) x) d",
               (muniScriptRun[]){{6, HEBR}, {9, LATN}, {11, HEBR}, {15, LATN}}, 4),
          "a matched opening bracket is popped");
    CHECK(Runs("a) b", (muniScriptRun[]){{4, LATN}}, 1), "a closing bracket alone joins the run");
    CHECK(Runs(")", (muniScriptRun[]){{1, MUNI_SCRIPT_COMMON}}, 1), "only a closing bracket");
    // "(" before Hebrew U+05E9 U+05DC U+05D5 U+05DD, " abc", ")".
    CHECK(Runs("(\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D abc)",
               (muniScriptRun[]){{10, HEBR}, {13, LATN}, {14, HEBR}}, 3),
          "a bracket opened before the first script takes it");
    CHECK(Runs("\xD7\xA9\xD7\x9C (abc] x", (muniScriptRun[]){{6, HEBR}, {12, LATN}}, 2),
          "a bracket of another pair does not close");
    // Hebrew, " ", U+2329, "abc", U+232A, " x".
    CHECK(Runs("\xD7\xA9\xD7\x9C \xE2\x8C\xA9"
               "abc\xE2\x8C\xAA x",
               (muniScriptRun[]){{8, HEBR}, {11, LATN}, {15, HEBR}, {16, LATN}}, 4),
          "angle brackets pair too");
}

// Greek alpha, then opening brackets: "(", "[" and then braces, so many
// that the stack holds all of them or drops "(", then a Latin "a", ")"
// and "]". Returns whether the runs are the expected ones.
static bool DeepBrackets(size_t braces, const muniScriptRun* expected, size_t count)
{
    char text[64];
    size_t length = 0;
    memcpy(text, "\xCE\xB1([", 4);
    length += 4;
    memset(text + length, '{', braces);
    length += braces;
    memcpy(text + length, "a)]", 4);
    return Runs(text, expected, count);
}

// The stack holds 32 opening brackets and forgets the oldest when a 33rd
// comes.
static void TestBracketDepth(void)
{
    // 32 openers: ")" closes "(" and is Greek, and "]" then matches
    // nothing and stays with it.
    CHECK(DeepBrackets(30, (muniScriptRun[]){{34, GREK}, {35, LATN}, {37, GREK}}, 3),
          "32 opening brackets are all kept");
    // 33 openers: "(" is forgotten, so ")" joins the Latin run, and "]"
    // still closes "[" and is Greek.
    CHECK(DeepBrackets(31, (muniScriptRun[]){{35, GREK}, {37, LATN}, {38, GREK}}, 3),
          "the 33rd opening bracket drops the oldest");
}

// Script_Extensions as a list: measured with no room, written into room
// that fits exactly.
static void TestScriptExtensionsList(void)
{
    size_t count = 0;
    CHECK(muniGetScriptExtensions('a', nullptr, 0, &count) == muni_errorCapacity && count == 1,
          "measuring a code point without extensions");
    muniScript scripts[32];
    // The danda U+0964 lists many scripts.
    CHECK(muniGetScriptExtensions(0x0964, nullptr, 0, &count) == muni_errorCapacity && count > 1 &&
              count < 31,
          "measuring the danda's list");
    for (size_t i = 0; i < 32; i++)
    {
        scripts[i] = 0;
    }
    size_t written = 0;
    CHECK(muniGetScriptExtensions(0x0964, scripts, count, &written) == muni_success &&
              written == count && scripts[0] != 0 && scripts[count - 1] != 0 && scripts[count] == 0,
          "written into room that fits exactly");
}

static void TestCapacityAndArguments(void)
{
    muniScriptRun runs[1];
    size_t count = 0;
    CHECK(muniFindScriptRuns("a\xD7\xA9", 3, runs, 1, &count) == muni_errorCapacity && count == 2,
          "two runs, room for one");
    CHECK(muniFindScriptRuns("a", 1, nullptr, 0, nullptr) == muni_errorInvalid, "no count");
    muniScriptIterator iterator;
    CHECK(muniInitScriptIterator(&iterator, "ab", 2, true) == muni_success &&
              muniScriptIterator_Feed(&iterator, "c", 1, false) == muni_errorState,
          "feeding before the iterator asks");
    CHECK(muniInitScriptIterator(&iterator, "ab", 2, false) == muni_success &&
              muniScriptIterator_Feed(&iterator, "c", 1, false) == muni_errorState,
          "feeding an iterator told no more text follows");
    CHECK(muniScriptIterator_Feed(&iterator, nullptr, 1, false) == muni_errorInvalid,
          "a NULL piece is invalid in any state");
    CHECK(muniInitScriptIterator(nullptr, "a", 1, false) == muni_errorInvalid, "no iterator");
    CHECK(muniInitScriptIterator(&iterator, nullptr, 1, false) == muni_errorInvalid,
          "no text with a length");
    CHECK(muniInitScriptIterator(&iterator, nullptr, 0, false) == muni_success,
          "no text and no length");
    muniScriptRun run = {7, LATN};
    CHECK(muniScriptIterator_NextRun(nullptr, &run) == muni_errorInvalid && run.end == 7 &&
              run.script == LATN,
          "the next run of no iterator");
    CHECK(muniInitScriptIterator(&iterator, "a", 1, false) == muni_success &&
              muniScriptIterator_NextRun(&iterator, nullptr) == muni_errorInvalid,
          "the next run into nothing");
    count = 7;
    CHECK(muniFindScriptRuns(nullptr, 1, runs, 1, &count) == muni_errorInvalid && count == 7,
          "finding runs in no text with a length");
    CHECK(muniFindScriptRuns("a", 1, nullptr, 1, &count) == muni_errorInvalid && count == 7,
          "no runs with a capacity");
    runs[0] = (muniScriptRun){7, LATN};
    CHECK(muniFindScriptRuns("a", 1, runs, 1, nullptr) == muni_errorInvalid && runs[0].end == 7 &&
              runs[0].script == LATN,
          "no count, and no run written");
}

int main(void)
{
    TestRuns();
    TestSharedScripts();
    TestBrackets();
    TestBracketDepth();
    TestScriptExtensionsList();
    TestCapacityAndArguments();
    return s_failures == 0 ? 0 : 1;
}
