// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Case mapping and folding: the table against the generator's hash over
// every code point, the simple mappings, and the full mappings over text
// with their contexts: special mappings that change the length, the
// final sigma, titlecase by word, and the Turkic dotted and dotless i.

#include "tables.h"
#include "test_harness.h"

#include "maul-unicode/case.h"

#include <string.h>

// FNV-1a 64 over the record index of every code point, as munigen hashes.
static uint64_t HashRecords(void)
{
    uint64_t hash = 0xcbf29ce484222325ull;
    for (uint32_t codePoint = 0; codePoint < 0x110000; codePoint++)
    {
        hash ^= muniLookupCase(codePoint);
        hash *= 0x100000001b3ull;
    }
    return hash;
}

static void TestSimple(void)
{
    CHECK(HashRecords() == muniCaseHash, "case records match the generator");
    CHECK(muniToLower('A') == 'a' && muniToUpper('a') == 'A', "ASCII");
    CHECK(muniToUpper(0x00DF) == 0x00DF, "sharp s has no simple uppercase");
    CHECK(muniToLower(0x1E9E) == 0x00DF, "capital sharp s lowercases to sharp s");
    CHECK(muniToTitle(0x01C6) == 0x01C5 && muniToUpper(0x01C6) == 0x01C4, "dz digraph");
    CHECK(muniFoldCase(0x1E9E) == 0x00DF, "simple folding of capital sharp s");
    CHECK(muniFoldCase(0x03C2) == 0x03C3, "final sigma folds to sigma");
    CHECK(muniToLower(0x10400) == 0x10428, "Deseret, outside the BMP");
    CHECK(muniIsCased('a') && !muniIsCased('1'), "cased");
    CHECK(muniToLower(0x110000) == 0x110000, "past U+10FFFF");
}

static bool Converts(const char* text, muniCaseOperation operation, muniCaseLanguage language,
                     const char* expected)
{
    char output[128];
    size_t needed = 0;
    muniTextResult result = muniConvertCase(text, strlen(text), operation, language,
                                            muni_convertStrict, output, sizeof(output), &needed);
    return result.status == muni_success && needed == strlen(expected) &&
           memcmp(output, expected, needed) == 0;
}

static void TestFull(void)
{
    CHECK(Converts("Stra\xC3\x9F"
                   "e",
                   muni_caseUpper, muni_caseDefault, "STRASSE"),
          "sharp s uppercases to SS");
    CHECK(Converts("Stra\xC3\x9F"
                   "e",
                   muni_caseFold, muni_caseDefault, "strasse"),
          "sharp s folds to ss");
    CHECK(Converts("\xEF\xAC\x81"
                   "x",
                   muni_caseUpper, muni_caseDefault, "FIX"),
          "the fi ligature uppercases to FI");
    // U+0390 uppercases to U+0399 U+0308 U+0301.
    CHECK(Converts("\xCE\x90", muni_caseUpper, muni_caseDefault, "\xCE\x99\xCC\x88\xCC\x81"),
          "a three code point mapping");
    // ΣΑΣ lowercases to σας: the last sigma is final.
    CHECK(Converts("\xCE\xA3\xCE\x91\xCE\xA3", muni_caseLower, muni_caseDefault,
                   "\xCF\x83\xCE\xB1\xCF\x82"),
          "final sigma");
    CHECK(Converts("\xCE\xA3", muni_caseLower, muni_caseDefault, "\xCF\x83"),
          "a lone sigma is not final");
    CHECK(Converts("\xCE\x91\xCE\xA3.", muni_caseLower, muni_caseDefault, "\xCE\xB1\xCF\x82."),
          "a sigma before punctuation is final");
    CHECK(Converts("hello wORLD, 'twas", muni_caseTitle, muni_caseDefault, "Hello World, 'Twas"),
          "titlecase by word");
    CHECK(Converts("\xC7\x86"
                   "emal",
                   muni_caseTitle, muni_caseDefault,
                   "\xC7\x85"
                   "emal"),
          "a digraph titlecases to its title form");
    // Default rules: İ lowercases to i and a combining dot above.
    CHECK(Converts("\xC4\xB0", muni_caseLower, muni_caseDefault, "i\xCC\x87"),
          "dotted capital I, default rules");
}

static void TestTurkic(void)
{
    CHECK(Converts("Istanbul", muni_caseLower, muni_caseTurkic, "\xC4\xB1stanbul"),
          "I lowercases to dotless i");
    CHECK(Converts("\xC4\xB0stanbul", muni_caseLower, muni_caseTurkic, "istanbul"),
          "dotted I lowercases to i");
    CHECK(Converts("istanbul", muni_caseUpper, muni_caseTurkic, "\xC4\xB0STANBUL"),
          "i uppercases to dotted I");
    CHECK(Converts("I\xCC\x87", muni_caseLower, muni_caseTurkic, "i"),
          "I with a combining dot lowercases to i");
    CHECK(Converts("izmir", muni_caseTitle, muni_caseTurkic, "\xC4\xB0zmir"),
          "titlecase with dotted I");
    CHECK(Converts("I\xC4\xB0", muni_caseFold, muni_caseTurkic, "\xC4\xB1i"), "Turkic folding");
}

static void TestErrors(void)
{
    char output[4];
    size_t needed = 0;
    muniTextResult result = muniConvertCase("stra\xC3\x9F"
                                            "e",
                                            7, muni_caseUpper, muni_caseDefault, muni_convertStrict,
                                            output, 4, &needed);
    CHECK(result.status == muni_errorCapacity && needed == 7 && memcmp(output, "STRA", 4) == 0,
          "capacity: the bytes that fit, the size needed");
    result = muniConvertCase("a\xFF", 2, muni_caseUpper, muni_caseDefault, muni_convertStrict,
                             output, 4, &needed);
    CHECK(result.status == muni_errorUtf8Lead && result.offset == 1 && needed == 1,
          "strict mode stops at ill-formed input");
    CHECK(muniConvertCase("a", 1, 9, muni_caseDefault, muni_convertStrict, output, 4, &needed)
                  .status == muni_errorInvalid,
          "an unknown operation");
}

int main(void)
{
    TestSimple();
    TestFull();
    TestTurkic();
    TestErrors();
    return s_failures == 0 ? 0 : 1;
}
