// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Versions and result names.

#include "test_harness.h"

#include "maul-unicode/base.h"

#include <string.h>

static void TestVersionMatchesHeader(void)
{
    muniVersion version = muniGetVersion();
    CHECK(version.major == MUNI_VERSION_MAJOR, "major version");
    CHECK(version.minor == MUNI_VERSION_MINOR, "minor version");
    CHECK(version.patch == MUNI_VERSION_PATCH, "patch version");
}

static void TestUnicodeVersionIsEighteen(void)
{
    muniVersion version = muniGetUnicodeVersion();
    CHECK(version.major == 18 && version.minor == 0 && version.patch == 0, "Unicode 18.0.0");
}

static void TestResultsVersionMatchesHeader(void)
{
    CHECK(muniGetResultsVersion() == MUNI_RESULTS_VERSION, "the linked results version");
}

static void TestResultNames(void)
{
    static const struct
    {
        muniResult result;
        const char* name;
    } names[] = {
        {muni_success, "muni_success"},
        {muni_done, "muni_done"},
        {muni_needMoreText, "muni_needMoreText"},
        {muni_errorInvalid, "muni_errorInvalid"},
        {muni_errorCapacity, "muni_errorCapacity"},
        {muni_errorState, "muni_errorState"},
        {muni_errorLimit, "muni_errorLimit"},
        {muni_errorUtf8Lead, "muni_errorUtf8Lead"},
        {muni_errorUtf8Continuation, "muni_errorUtf8Continuation"},
        {muni_errorUtf8Truncated, "muni_errorUtf8Truncated"},
        {muni_errorUtf8Overlong, "muni_errorUtf8Overlong"},
        {muni_errorUtf8Surrogate, "muni_errorUtf8Surrogate"},
        {muni_errorUtf8TooLarge, "muni_errorUtf8TooLarge"},
        {muni_errorUtf16Surrogate, "muni_errorUtf16Surrogate"},
        {muni_errorUtf32Value, "muni_errorUtf32Value"},
        {muni_errorIdentifier, "muni_errorIdentifier"},
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        CHECK(strcmp(muniResultName(names[i].result), names[i].name) == 0, names[i].name);
    }
    CHECK(strcmp(muniResultName(12345), "unknown result") == 0, "unknown name");
}

// The errors the family shares keep the family's values, and the
// library's own errors start at -32.
static void TestResultValues(void)
{
    CHECK(muni_errorInvalid == -1 && muni_errorCapacity == -2 && muni_errorState == -6 &&
              muni_errorLimit == -9,
          "the family's errors");
    CHECK(muni_errorUtf8Lead == -32 && muni_errorUtf8Continuation == -33 &&
              muni_errorUtf8Truncated == -34 && muni_errorUtf8Overlong == -35 &&
              muni_errorUtf8Surrogate == -36 && muni_errorUtf8TooLarge == -37 &&
              muni_errorUtf16Surrogate == -38 && muni_errorUtf32Value == -39 &&
              muni_errorIdentifier == -40,
          "the library's own errors");
}

int main(void)
{
    TestVersionMatchesHeader();
    TestUnicodeVersionIsEighteen();
    TestResultsVersionMatchesHeader();
    TestResultNames();
    TestResultValues();
    return s_failures == 0 ? 0 : 1;
}
