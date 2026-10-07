// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Script runs: plain runs, Common and Inherited code points, extensions,
// brackets around foreign text, and text fed one byte at a time.

#include "test_harness.h"

#include "maul-unicode/script.h"

#include <string.h>

#define LATN MUNI_SCRIPT('L', 'a', 't', 'n')
#define HEBR MUNI_SCRIPT('H', 'e', 'b', 'r')
#define DEVA MUNI_SCRIPT('D', 'e', 'v', 'a')
#define BENG MUNI_SCRIPT('B', 'e', 'n', 'g')
#define HANI MUNI_SCRIPT('H', 'a', 'n', 'i')
#define HIRA MUNI_SCRIPT('H', 'i', 'r', 'a')

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
        muniResult status = muniNextScriptRun(&iterator, &run);
        if (status == muni_needMoreText)
        {
            if (muniFeedScriptIterator(&iterator, text + fed, 1, fed + 1 < length) != muni_success)
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

static void TestCapacityAndArguments(void)
{
    muniScriptRun runs[1];
    size_t count = 0;
    CHECK(muniFindScriptRuns("a\xD7\xA9", 3, runs, 1, &count) == muni_errorCapacity && count == 2,
          "two runs, room for one");
    CHECK(muniFindScriptRuns("a", 1, nullptr, 0, nullptr) == muni_errorInvalid, "no count");
    muniScriptIterator iterator;
    CHECK(muniInitScriptIterator(&iterator, "ab", 2, true) == muni_success &&
              muniFeedScriptIterator(&iterator, "c", 1, false) == muni_errorInvalid,
          "feeding before the iterator asks");
}

int main(void)
{
    TestRuns();
    TestBrackets();
    TestCapacityAndArguments();
    return s_failures == 0 ? 0 : 1;
}
