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
    CHECK(strcmp(muniResultName(muni_errorInvalid), "muni_errorInvalid") == 0, "invalid name");
    CHECK(strcmp(muniResultName(muni_errorCapacity), "muni_errorCapacity") == 0, "capacity name");
    CHECK(strcmp(muniResultName(12345), "unknown result") == 0, "unknown name");
}

int main(void)
{
    TestVersionMatchesHeader();
    TestUnicodeVersionIsEighteen();
    TestResultNames();
    return s_failures == 0 ? 0 : 1;
}
