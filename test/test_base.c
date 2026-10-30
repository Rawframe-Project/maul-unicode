// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Versions and result names, and the layouts of the public types.

#include "test_harness.h"

#include "maul-unicode/base.h"
#include "maul-unicode/bidi.h"
#include "maul-unicode/case.h"
#include "maul-unicode/encoding.h"
#include "maul-unicode/normalize.h"
#include "maul-unicode/properties.h"
#include "maul-unicode/script.h"
#include "maul-unicode/security.h"
#include "maul-unicode/segment.h"

#include <stddef.h>
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
        {muni_errorBusy, "muni_errorBusy"},
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
        CHECK(strcmp(muniGetResultName(names[i].result), names[i].name) == 0, names[i].name);
    }
    CHECK(strcmp(muniGetResultName(12345), "unknown result") == 0, "unknown name");
}

// The errors the family shares keep the family's values, and the
// library's own errors start at -32.
static void TestResultValues(void)
{
    CHECK(muni_errorInvalid == -1 && muni_errorCapacity == -2 && muni_errorState == -6 &&
              muni_errorLimit == -9 && muni_errorBusy == -10,
          "the family's errors");
    CHECK(muni_errorUtf8Lead == -32 && muni_errorUtf8Continuation == -33 &&
              muni_errorUtf8Truncated == -34 && muni_errorUtf8Overlong == -35 &&
              muni_errorUtf8Surrogate == -36 && muni_errorUtf8TooLarge == -37 &&
              muni_errorUtf16Surrogate == -38 && muni_errorUtf32Value == -39 &&
              muni_errorIdentifier == -40,
          "the library's own errors");
}

// The binary interface of the public types. Every enumeration of kinds,
// modes and choices is an int32_t, as the result type is, and a script is
// a packed tag in a uint32_t. A struct's fields sit where the family's
// rules put them, on 32-bit and 64-bit targets alike.
#define SIGNED_32(type) (sizeof(type) == 4 && (type) - 1 < 0)
static_assert(SIGNED_32(muniResult), "muniResult is an int32_t");
static_assert(SIGNED_32(muniBidiDirection), "muniBidiDirection is an int32_t");
static_assert(SIGNED_32(muniConvertMode), "muniConvertMode is an int32_t");
static_assert(SIGNED_32(muniNormalForm), "muniNormalForm is an int32_t");
static_assert(SIGNED_32(muniQuickCheck), "muniQuickCheck is an int32_t");
static_assert(SIGNED_32(muniGeneralCategory), "muniGeneralCategory is an int32_t");
static_assert(SIGNED_32(muniGraphemeBreak), "muniGraphemeBreak is an int32_t");
static_assert(SIGNED_32(muniWordBreak), "muniWordBreak is an int32_t");
static_assert(SIGNED_32(muniSentenceBreak), "muniSentenceBreak is an int32_t");
static_assert(SIGNED_32(muniIndicConjunctBreak), "muniIndicConjunctBreak is an int32_t");
static_assert(SIGNED_32(muniLineBreak), "muniLineBreak is an int32_t");
static_assert(SIGNED_32(muniEastAsianWidth), "muniEastAsianWidth is an int32_t");
static_assert(SIGNED_32(muniBidiClass), "muniBidiClass is an int32_t");
static_assert(SIGNED_32(muniBracketType), "muniBracketType is an int32_t");
static_assert(SIGNED_32(muniRestrictionLevel), "muniRestrictionLevel is an int32_t");
static_assert(SIGNED_32(muniCaseOperation), "muniCaseOperation is an int32_t");
static_assert(SIGNED_32(muniCaseLanguage), "muniCaseLanguage is an int32_t");
static_assert(sizeof(muniScript) == 4 && (muniScript)-1 > 0, "muniScript is a uint32_t");
static_assert(sizeof(muniBreakClassOverride) == 8 &&
                  offsetof(muniBreakClassOverride, codePoint) == 0 &&
                  offsetof(muniBreakClassOverride, value) == 4,
              "a break class override is a code point and an int32_t value");
static_assert(offsetof(muniTextResult, status) == 0 &&
                  offsetof(muniTextResult, offset) == sizeof(size_t) &&
                  sizeof(muniTextResult) == 2 * sizeof(size_t),
              "a text result is a status and an offset");
static_assert(offsetof(muniScriptRun, end) == 0 &&
                  offsetof(muniScriptRun, script) == sizeof(size_t) &&
                  sizeof(muniScriptRun) == 2 * sizeof(size_t),
              "a script run is an end and a script");
static_assert(offsetof(muniBidiRun, start) == 0 &&
                  offsetof(muniBidiRun, length) == sizeof(size_t) &&
                  offsetof(muniBidiRun, level) == 2 * sizeof(size_t) &&
                  sizeof(muniBidiRun) == 3 * sizeof(size_t),
              "a bidi run is a start, a length and a level");
static_assert(sizeof(muniVersion) == 6 && offsetof(muniVersion, minor) == 2 &&
                  offsetof(muniVersion, patch) == 4,
              "a version is three uint16_t");

int main(void)
{
    TestVersionMatchesHeader();
    TestUnicodeVersionIsEighteen();
    TestResultsVersionMatchesHeader();
    TestResultNames();
    TestResultValues();
    return s_failures == 0 ? 0 : 1;
}
