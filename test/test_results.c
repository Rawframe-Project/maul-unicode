// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The results version: a hash of what muniNormalize, muniToNfkcCasefold,
// muniConvertCase and muniGetSkeleton give, and of the verdicts a program
// may store beside a skeleton (muniCheckIdentifier,
// muniGetRestrictionLevel, muniCheckMixedNumbers and
// muniGetResolvedScripts), for every code point, every source sequence
// of NormalizationTest.txt and a few strings whose results depend on
// context, against the hash each results version gave.
// A change of any of those outputs, a new Unicode version included,
// fails here until MUNI_RESULTS_VERSION moves up and its hash is added.

#include "test_harness.h"

#include "maul-unicode/case.h"
#include "maul-unicode/identifier.h"
#include "maul-unicode/normalize.h"
#include "maul-unicode/security.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Every results version and the hash of its outputs, oldest first. A
// change of output adds a line with the next version; a line already
// released never changes.
static const struct
{
    uint32_t version;
    uint64_t hash;
} s_history[] = {
    {1, 0x73db1792c2cc9cabull},
};

static uint64_t s_hash = 0xcbf29ce484222325ull;

static void HashBytes(const void* bytes, size_t length)
{
    const uint8_t* p = bytes;
    for (size_t i = 0; i < length; ++i)
    {
        s_hash ^= p[i];
        s_hash *= 0x100000001b3ull;
    }
}

// Hashes a number least significant byte first, the same on every
// platform.
static void HashNumber(uint64_t number)
{
    for (int i = 0; i < 8; ++i)
    {
        uint8_t byte = (uint8_t)(number >> (8 * i));
        HashBytes(&byte, 1);
    }
}

// Hashes a call's status, offset and output, so a change of any of them
// counts.
static void HashResult(muniTextResult result, const char* output, size_t needed)
{
    HashNumber((uint64_t)(int64_t)result.status);
    HashNumber(result.offset);
    HashNumber(needed);
    if (result.status == muni_success)
    {
        HashBytes(output, needed);
    }
}

// Far more room than any of these inputs needs; the workspace takes two
// bytes per byte of the longest.
static char s_output[1 << 14];
static uint8_t s_workspace[1 << 12];

// Hashes a verdict: its status and offset, then what it says.
static void HashVerdict(muniTextResult result, uint64_t value)
{
    HashNumber((uint64_t)(int64_t)result.status);
    HashNumber(result.offset);
    HashNumber(value);
}

static void HashVerdicts(const char* text, size_t length)
{
    HashVerdict(muniCheckIdentifier(text, length), 0);
    muniRestrictionLevel level = 0;
    muniTextResult result = muniGetRestrictionLevel(text, length, &level);
    HashVerdict(result, level);
    bool mixed = false;
    result = muniCheckMixedNumbers(text, length, &mixed);
    HashVerdict(result, mixed);
    muniScript scripts[64];
    size_t count = 0;
    result =
        muniGetResolvedScripts(text, length, scripts, sizeof(scripts) / sizeof(scripts[0]), &count);
    HashVerdict(result, count);
    for (size_t i = 0; result.status == muni_success && i < count; ++i)
    {
        HashNumber(scripts[i]);
    }
}

static void HashOutputs(const char* text, size_t length, bool everyVariant)
{
    static const muniNormalForm forms[] = {muni_nfc, muni_nfd, muni_nfkc, muni_nfkd};
    static const muniCaseOperation operations[] = {muni_caseLower, muni_caseUpper, muni_caseTitle,
                                                   muni_caseFold};
    static const muniCaseLanguage languages[] = {muni_caseDefault, muni_caseTurkic,
                                                 muni_caseLithuanian};
    static const muniBidiDirection directions[] = {muni_bidiAuto, muni_bidiLeftToRight,
                                                   muni_bidiRightToLeft};
    size_t needed = 0;
    for (size_t f = 0; f < sizeof(forms) / sizeof(forms[0]); ++f)
    {
        muniTextResult result = muniNormalize(text, length, forms[f], muni_convertStrict, s_output,
                                              sizeof(s_output), &needed);
        HashResult(result, s_output, needed);
    }
    muniTextResult folded =
        muniToNfkcCasefold(text, length, muni_convertStrict, s_output, sizeof(s_output), &needed);
    HashResult(folded, s_output, needed);
    size_t languageCount = everyVariant ? sizeof(languages) / sizeof(languages[0]) : 1;
    for (size_t o = 0; o < sizeof(operations) / sizeof(operations[0]); ++o)
    {
        for (size_t l = 0; l < languageCount; ++l)
        {
            muniTextResult result =
                muniConvertCase(text, length, operations[o], languages[l], muni_convertStrict,
                                s_output, sizeof(s_output), &needed);
            HashResult(result, s_output, needed);
        }
    }
    size_t directionCount = everyVariant ? sizeof(directions) / sizeof(directions[0]) : 1;
    for (size_t d = 0; d < directionCount; ++d)
    {
        muniTextResult result =
            muniGetSkeleton(text, length, directions[d], s_workspace, sizeof(s_workspace), s_output,
                            sizeof(s_output), &needed);
        HashResult(result, s_output, needed);
    }
    HashVerdicts(text, length);
}

static void HashCodePoints(void)
{
    for (uint32_t codePoint = 0; codePoint <= 0x10FFFF; ++codePoint)
    {
        char text[4];
        size_t length = 0;
        if (muniEncodeUtf8(codePoint, text, &length) == muni_success)
        {
            HashOutputs(text, length, false);
        }
    }
}

// The source column of every line of NormalizationTest.txt, which puts
// marks in and out of canonical order and builds every composition.
static bool HashNormalizationTest(void)
{
    FILE* file = fopen(MUNI_UCD_DATA "/NormalizationTest.txt", "r");
    if (file == nullptr)
    {
        return false;
    }
    static char line[4096];
    size_t sequences = 0;
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        if (line[0] == '#' || line[0] == '@' || line[0] == '\n')
        {
            continue;
        }
        char text[256];
        size_t length = 0;
        const char* field = line;
        while (*field != ';')
        {
            char* end = nullptr;
            uint32_t codePoint = (uint32_t)strtoul(field, &end, 16);
            size_t size = 0;
            if (end == field || length + 4 > sizeof(text) ||
                muniEncodeUtf8(codePoint, text + length, &size) != muni_success)
            {
                fclose(file);
                return false;
            }
            length += size;
            field = end;
            while (*field == ' ')
            {
                field++;
            }
        }
        HashOutputs(text, length, true);
        sequences += 1;
    }
    fclose(file);
    return sequences > 10000;
}

// Strings whose results depend on context or tailoring: final sigma,
// Turkic and Lithuanian dots, title case across words, and skeletons in
// mixed directions.
static void HashContexts(void)
{
    static const char* const strings[] = {
        "\xCE\x8C\xCE\xA3\xCE\x9F\xCE\xA3 \xCE\xA3\xCE\x91", // Greek, sigma at the ends
        "I\xC4\xB0i\xC4\xB1 \xC4\xB0stanbul",                // dotted and dotless i
        "Ii\xCC\x87 J\xCC\x81 \xC3\x8C",                     // Lithuanian dots
        "hello wORLD, 'tis o'neill",                         // title case
        "abc \xD7\x90\xD7\x91\xD7\x92 123 \xD9\xA1\xD9\xA2", // mixed directions
        "paypal p\xD0\xB0yp\xD0\xB0l",                       // Cyrillic a
        "\xEF\xAC\x81 \xE2\x84\xAB \xEF\xBC\xA1",            // ligature, angstrom, full width
    };
    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i)
    {
        HashOutputs(strings[i], strlen(strings[i]), true);
    }
}

static void TestResultsVersion(void)
{
    size_t count = sizeof(s_history) / sizeof(s_history[0]);
    for (size_t i = 1; i < count; ++i)
    {
        CHECK(s_history[i].version == s_history[i - 1].version + 1 &&
                  s_history[i].hash != s_history[i - 1].hash,
              "each version follows the one before and changes the outputs");
    }
    CHECK(s_history[count - 1].version == MUNI_RESULTS_VERSION &&
              muniGetResultsVersion() == MUNI_RESULTS_VERSION,
          "the last version is the library's");
    HashCodePoints();
    CHECK(HashNormalizationTest(), "NormalizationTest.txt reads");
    HashContexts();
    if (s_hash != s_history[count - 1].hash)
    {
        printf("the outputs hash to 0x%016llxull: move MUNI_RESULTS_VERSION up and add it\n",
               (unsigned long long)s_hash);
    }
    CHECK(s_hash == s_history[count - 1].hash, "the outputs are those of this results version");
}

int main(void)
{
    TestResultsVersion();
    return s_failures == 0 ? 0 : 1;
}
