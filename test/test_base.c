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

static void TestResultNames(void)
{
    CHECK(strcmp(muniResultName(muni_success), "muni_success") == 0, "success name");
    CHECK(strcmp(muniResultName(muni_errorUtf32Value), "muni_errorUtf32Value") == 0,
          "the last error's name");
    CHECK(strcmp(muniResultName(muni_needMoreText), "muni_needMoreText") == 0, "more text name");
    CHECK(strcmp(muniResultName(12345), "unknown result") == 0, "unknown name");
}

// The errors the family shares keep the family's values, and the
// library's own errors start at -32.
static void TestResultValues(void)
{
    CHECK(muni_errorInvalid == -1 && muni_errorCapacity == -2 && muni_errorLimit == -9,
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
    TestResultNames();
    TestResultValues();
    return s_failures == 0 ? 0 : 1;
}
