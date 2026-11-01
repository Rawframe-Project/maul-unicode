// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Case mapping and folding: the table against the generator's hash over
// every code point, the simple mappings, and the full mappings over text
// with their contexts: special mappings that change the length, the
// final sigma, titlecase by word, the Turkic dotted and dotless i, and
// the Lithuanian dot above.
// With normalization built, NFKC_Casefold of every code point against
// DerivedNormalizationProps.txt, and over text, and the simple and full
// mappings of every code point against the UCD files.

#include "tables.h"
#include "test_harness.h"

#include "maul-unicode/case.h"

#ifdef MUNI_UCD_DATA
#include "ucd.h"
#endif

#include <stdlib.h>
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
    CHECK(muniGetLowercase('A') == 'a' && muniGetUppercase('a') == 'A', "ASCII");
    CHECK(muniGetUppercase(0x00DF) == 0x00DF, "sharp s has no simple uppercase");
    CHECK(muniGetLowercase(0x1E9E) == 0x00DF, "capital sharp s lowercases to sharp s");
    CHECK(muniGetTitlecase(0x01C6) == 0x01C5 && muniGetUppercase(0x01C6) == 0x01C4, "dz digraph");
    CHECK(muniFoldCase(0x1E9E) == 0x00DF, "simple folding of capital sharp s");
    CHECK(muniFoldCase(0x03C2) == 0x03C3, "final sigma folds to sigma");
    CHECK(muniGetLowercase(0x10400) == 0x10428, "Deseret, outside the BMP");
    CHECK(muniIsCased('a') && !muniIsCased('1'), "cased");
    for (int i = 0; i < 2; i++)
    {
        uint32_t past = i == 0 ? 0x110000 : UINT32_MAX;
        CHECK(muniGetLowercase(past) == past && muniGetUppercase(past) == past &&
                  muniGetTitlecase(past) == past && muniFoldCase(past) == past &&
                  !muniIsCased(past),
              "past U+10FFFF maps to itself and is not cased");
    }
}

// Converts text held alone in a heap allocation of its length, so that
// AddressSanitizer sees a context rule reading past its end.
static bool Converts(const char* text, muniCaseOperation operation, muniCaseLanguage language,
                     const char* expected)
{
    size_t length = strlen(text);
    char* copy = malloc(length > 0 ? length : 1);
    if (copy == nullptr)
    {
        return false;
    }
    memcpy(copy, text, length);
    char output[128];
    size_t needed = 0;
    muniTextResult result = muniConvertCase(copy, length, operation, language, muni_convertStrict,
                                            output, sizeof(output), &needed);
    free(copy);
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
    CHECK(Converts("XI", muni_caseLower, muni_caseTurkic, "x\xC4\xB1"),
          "I at the end, no dot after it, lowercases to dotless i");
    // Before_Dot and After_I look past marks of classes other than 0 and
    // 230. U+0323 DOT BELOW (220) between the I and the dot lets the dot
    // go; U+0301 ACUTE ACCENT (230) stops the look, so the I is dotless and
    // the dot stays.
    CHECK(Converts("I\xCC\xA3\xCC\x87", muni_caseLower, muni_caseTurkic, "i\xCC\xA3"),
          "the dot of an I past a dot below");
    CHECK(
        Converts("I\xCC\x81\xCC\x87", muni_caseLower, muni_caseTurkic, "\xC4\xB1\xCC\x81\xCC\x87"),
        "an acute accent keeps the dot from the I");
}

// Final_Sigma: a sigma ends a word when no cased letter follows past
// case-ignorables; a space is not one.
static void TestFinalSigma(void)
{
    CHECK(Converts("\xCE\x91\xCE\xA3", muni_caseLower, muni_caseDefault, "\xCE\xB1\xCF\x82"),
          "a sigma at the end is final");
    CHECK(Converts("\xCE\x91\xCE\xA3 \xCE\x91", muni_caseLower, muni_caseDefault,
                   "\xCE\xB1\xCF\x82 \xCE\xB1"),
          "a sigma before a space and a word is final");
    CHECK(Converts("\xCE\x91\xCE\xA3\xCE\x91", muni_caseLower, muni_caseDefault,
                   "\xCE\xB1\xCF\x83\xCE\xB1"),
          "a sigma inside a word is not");
}

// NFKC_Casefold of more combining marks than a normalizer segment holds
// is refused, as normalization refuses it.
static void TestNfkcCasefoldLimit(void)
{
    char marks[1 + 40 * 2];
    marks[0] = 'a';
    for (int i = 0; i < 40; i++)
    {
        marks[1 + 2 * i] = (char)0xCC;
        marks[2 + 2 * i] = (char)0x81;
    }
    char output[256];
    size_t needed = 0;
    muniTextResult result =
        muniGetNfkcCasefold(marks, sizeof(marks), muni_convertStrict, output, 256, &needed);
    CHECK(result.status == muni_errorLimit && result.offset > 0, "too many marks is refused");
}

// Each Lithuanian rule of SpecialCasing.txt, with the context each
// conditional one needs and lacks.
static void TestLithuanian(void)
{
    CHECK(Converts("I\xCC\x80", muni_caseLower, muni_caseLithuanian, "i\xCC\x87\xCC\x80"),
          "I with an accent above keeps a dot");
    CHECK(Converts("J\xCC\x81", muni_caseLower, muni_caseLithuanian, "j\xCC\x87\xCC\x81"),
          "so does J");
    CHECK(Converts("\xC4\xAE\xCC\xA3\xCC\x83", muni_caseLower, muni_caseLithuanian,
                   "\xC4\xAF\xCC\x87\xCC\xA3\xCC\x83"),
          "I with an ogonek, past a mark below");
    CHECK(Converts("I\xCC\xA3", muni_caseLower, muni_caseLithuanian, "i\xCC\xA3"),
          "no accent above, no dot");
    CHECK(Converts("\xC3\x8C\xC3\x8D\xC4\xA8", muni_caseLower, muni_caseLithuanian,
                   "i\xCC\x87\xCC\x80i\xCC\x87\xCC\x81i\xCC\x87\xCC\x83"),
          "precomposed I with grave, acute and tilde");
    CHECK(Converts("i\xCC\x87\xCC\x80", muni_caseUpper, muni_caseLithuanian, "I\xCC\x80"),
          "uppercasing drops the dot");
    CHECK(Converts("i\xCC\xA8\xCC\x87", muni_caseUpper, muni_caseLithuanian, "I\xCC\xA8"),
          "past a mark of another class");
    CHECK(Converts("i\xCC\x80\xCC\x87", muni_caseUpper, muni_caseLithuanian, "I\xCC\x80\xCC\x87"),
          "not past another accent above");
    CHECK(Converts("a\xCC\x87", muni_caseUpper, muni_caseLithuanian, "A\xCC\x87"),
          "an a is not soft dotted");
    CHECK(Converts("\xF0\x9D\x90\xA2\xCC\x87", muni_caseUpper, muni_caseLithuanian,
                   "\xF0\x9D\x90\xA2"),
          "a mathematical i is soft dotted");
    CHECK(Converts("I\xCC\x80", muni_caseLower, muni_caseDefault, "i\xCC\x80"),
          "the default rules keep no dot");
    CHECK(Converts("I\xCC\x80", muni_caseFold, muni_caseLithuanian, "i\xCC\x80"),
          "folding has no Lithuanian rules");
}

// Greek uppercasing, with the results ICU's Greek rules give.
static void TestGreek(void)
{
    // άδικος, κείμενο, ίριδα
    CHECK(Converts("\xCE\xAC\xCE\xB4\xCE\xB9\xCE\xBA\xCE\xBF\xCF\x82, "
                   "\xCE\xBA\xCE\xB5\xCE\xAF\xCE\xBC\xCE\xB5\xCE\xBD\xCE\xBF, "
                   "\xCE\xAF\xCF\x81\xCE\xB9\xCE\xB4\xCE\xB1",
                   muni_caseUpper, muni_caseGreek,
                   "\xCE\x91\xCE\x94\xCE\x99\xCE\x9A\xCE\x9F\xCE\xA3, "
                   "\xCE\x9A\xCE\x95\xCE\x99\xCE\x9C\xCE\x95\xCE\x9D\xCE\x9F, "
                   "\xCE\x99\xCE\xA1\xCE\x99\xCE\x94\xCE\x91"),
          "the tonos goes");
    // Μαΐου to ΜΑΪΟΥ, and Μάιος to ΜΑΪΟΣ.
    CHECK(Converts("\xCE\x9C\xCE\xB1\xCE\x90\xCE\xBF\xCF\x85", muni_caseUpper, muni_caseGreek,
                   "\xCE\x9C\xCE\x91\xCE\xAA\xCE\x9F\xCE\xA5"),
          "a dialytika stays when its tonos goes");
    CHECK(Converts("\xCE\x9C\xCE\xAC\xCE\xB9\xCE\xBF\xCF\x82", muni_caseUpper, muni_caseGreek,
                   "\xCE\x9C\xCE\x91\xCE\xAA\xCE\x9F\xCE\xA3"),
          "an iota after a vowel that lost its tonos gains a dialytika");
    // άυλος to ΑΫΛΟΣ.
    CHECK(Converts("\xCE\xAC\xCF\x85\xCE\xBB\xCE\xBF\xCF\x82", muni_caseUpper, muni_caseGreek,
                   "\xCE\x91\xCE\xAB\xCE\x9B\xCE\x9F\xCE\xA3"),
          "so does an upsilon");
    CHECK(Converts("\xCE\xB1\xCC\x81\xCE\xB9", muni_caseUpper, muni_caseGreek,
                   "\xCE\x91\xCE\x99\xCC\x88"),
          "a combining tonos gives a combining dialytika");
    // ρήματα ή άκλιτες to ΡΗΜΑΤΑ Ή ΑΚΛΙΤΕΣ.
    CHECK(Converts("\xCF\x81\xCE\xAE\xCE\xBC\xCE\xB1\xCF\x84\xCE\xB1 \xCE\xAE "
                   "\xCE\xAC\xCE\xBA\xCE\xBB\xCE\xB9\xCF\x84\xCE\xB5\xCF\x82",
                   muni_caseUpper, muni_caseGreek,
                   "\xCE\xA1\xCE\x97\xCE\x9C\xCE\x91\xCE\xA4\xCE\x91 \xCE\x89 "
                   "\xCE\x91\xCE\x9A\xCE\x9B\xCE\x99\xCE\xA4\xCE\x95\xCE\xA3"),
          "the disjunctive eta keeps its tonos");
    CHECK(Converts("\xCE\xAE.", muni_caseUpper, muni_caseGreek, "\xCE\x89."),
          "before punctuation too");
    CHECK(Converts("\xCE\xB7\xCC\x81", muni_caseUpper, muni_caseGreek, "\xCE\x97\xCC\x81"),
          "a combining tonos stays combining");
    CHECK(Converts("\xCE\xAE\xCE\xBB\xCE\xB9\xCE\xBF\xCF\x82", muni_caseUpper, muni_caseGreek,
                   "\xCE\x97\xCE\x9B\xCE\x99\xCE\x9F\xCE\xA3"),
          "an eta in a word does not");
    // ὀδυσσεὺς ᾄδει to ΟΔΥΣΣΕΥΣ ΑΙΔΕΙ, and ῃ to ΗΙ.
    CHECK(Converts("\xE1\xBD\x80\xCE\xB4\xCF\x85\xCF\x83\xCF\x83\xCE\xB5\xE1\xBD\xBA\xCF\x82 "
                   "\xE1\xBE\x84\xCE\xB4\xCE\xB5\xCE\xB9",
                   muni_caseUpper, muni_caseGreek,
                   "\xCE\x9F\xCE\x94\xCE\xA5\xCE\xA3\xCE\xA3\xCE\x95\xCE\xA5\xCE\xA3 "
                   "\xCE\x91\xCE\x99\xCE\x94\xCE\x95\xCE\x99"),
          "polytonic breathings and accents go, a subscript becomes an iota");
    CHECK(Converts("\xE1\xBF\x83", muni_caseUpper, muni_caseGreek, "\xCE\x97\xCE\x99"),
          "an eta with a subscript keeps no tonos");
    CHECK(Converts("\xCE\xB1\xCC\x93\xCC\x81\xCD\x85\xCD\x85", muni_caseUpper, muni_caseGreek,
                   "\xCE\x91\xCE\x99\xCE\x99"),
          "combining marks go with the letter, each subscript an iota");
    CHECK(Converts("\xCE\xAC\xCE\xB9", muni_caseLower, muni_caseGreek, "\xCE\xAC\xCE\xB9") &&
              Converts("\xCE\xAC\xCE\xB9", muni_caseTitle, muni_caseGreek, "\xCE\x86\xCE\xB9") &&
              Converts("\xCE\x86\xCE\x99", muni_caseFold, muni_caseGreek, "\xCE\xAC\xCE\xB9"),
          "lowercasing, titlecasing and folding are the default ones");
    CHECK(Converts("\xCE\xAC\xCE\xB9", muni_caseUpper, muni_caseDefault, "\xCE\x86\xCE\x99"),
          "the default rules keep the tonos");
}

// The Armenian ligature ech yiwn, which ICU's hy rules write with vew.
static void TestArmenian(void)
{
    CHECK(Converts("\xD6\x87", muni_caseUpper, muni_caseArmenian, "\xD4\xB5\xD5\x8E"),
          "uppercase: ech and capital vew");
    CHECK(Converts("\xD6\x87", muni_caseTitle, muni_caseArmenian, "\xD4\xB5\xD5\xBE"),
          "titlecase: ech and small vew");
    // Երևան to ԵՐԵՎԱՆ.
    CHECK(Converts("\xD4\xB5\xD6\x80\xD6\x87\xD5\xA1\xD5\xB6", muni_caseUpper, muni_caseArmenian,
                   "\xD4\xB5\xD5\x90\xD4\xB5\xD5\x8E\xD4\xB1\xD5\x86"),
          "inside a word");
    CHECK(Converts("\xD6\x87", muni_caseLower, muni_caseArmenian, "\xD6\x87") &&
              Converts("\xD6\x87", muni_caseFold, muni_caseArmenian, "\xD5\xA5\xD6\x82"),
          "lowercasing and folding are the default ones");
    CHECK(Converts("\xD6\x87", muni_caseUpper, muni_caseDefault, "\xD4\xB5\xD5\x92"),
          "the default rules write yiwn");
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
    CHECK(
        muniConvertCase("a", 1, muni_caseLower, 5, muni_convertStrict, output, 4, &needed).status ==
            muni_errorInvalid,
        "an unknown language");
    needed = 7;
    memcpy(output, "xxxx", 4);
    CHECK(muniConvertCase("a", 1, muni_caseLower, muni_caseDefault, 2, output, 4, &needed).status ==
                  muni_errorInvalid &&
              needed == 7 && memcmp(output, "xxxx", 4) == 0,
          "an unknown mode, and nothing is written");
    CHECK(muniConvertCase(nullptr, 1, muni_caseLower, muni_caseDefault, muni_convertStrict, output,
                          4, &needed)
                      .status == muni_errorInvalid &&
              needed == 7 && memcmp(output, "xxxx", 4) == 0,
          "no text with a length");
    CHECK(muniConvertCase("a", 1, muni_caseLower, muni_caseDefault, muni_convertStrict, nullptr, 4,
                          &needed)
                      .status == muni_errorInvalid &&
              needed == 7,
          "no output with a capacity");
    CHECK(muniConvertCase("a", 1, muni_caseLower, muni_caseDefault, muni_convertStrict, output, 4,
                          nullptr)
                      .status == muni_errorInvalid &&
              memcmp(output, "xxxx", 4) == 0,
          "no needed size out");
}

#ifdef MUNI_UCD_DATA

// The NFKC_CF values the file lists, as UTF-8: the offset of each code
// point's value in the pool, which ends it with 0, or NONE when the code
// point maps to itself.
#define POOL_SIZE (1u << 16)
#define NONE      0xFFFFFFFFu
static uint32_t s_offsets[0x110000];
static char s_pool[POOL_SIZE];

// Parses "hex..hex ; NFKC_CF; hex hex # comment" into the table.
static void ParseCasefold(const char* line, size_t* poolUsed)
{
    char* end = nullptr;
    uint32_t first = (uint32_t)strtoul(line, &end, 16);
    uint32_t last = end[0] == '.' ? (uint32_t)strtoul(end + 2, &end, 16) : first;
    const char* value = strchr(strchr(line, ';') + 1, ';') + 1;
    size_t offset = *poolUsed;
    for (;;)
    {
        uint32_t codePoint = (uint32_t)strtoul(value, &end, 16);
        if (end == value)
        {
            break;
        }
        size_t size = 0;
        if (*poolUsed + 5 > POOL_SIZE)
        {
            return;
        }
        (void)muniEncodeUtf8(codePoint, s_pool + *poolUsed, POOL_SIZE - *poolUsed, &size);
        *poolUsed += size;
        value = end;
    }
    s_pool[(*poolUsed)++] = '\0';
    for (uint32_t codePoint = first; codePoint <= last; codePoint++)
    {
        s_offsets[codePoint] = (uint32_t)offset;
    }
}

// Loads the table: code points the file does not name map to themselves.
static bool LoadCasefold(void)
{
    FILE* file = fopen(MUNI_UCD_DATA "/DerivedNormalizationProps.txt", "r");
    if (file == nullptr)
    {
        return false;
    }
    size_t poolUsed = 0;
    memset(s_offsets, 0xFF, sizeof(s_offsets));
    static char line[1024];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        if (strstr(line, "; NFKC_CF;") != nullptr && line[0] != '#')
        {
            ParseCasefold(line, &poolUsed);
        }
    }
    fclose(file);
    return true;
}

static void TestCasefoldEverywhere(void)
{
    CHECK(LoadCasefold(), "DerivedNormalizationProps.txt opens");
    int listed = 0;
    int failures = 0;
    for (uint32_t codePoint = 0; codePoint < 0x110000; codePoint++)
    {
        if (codePoint >= 0xD800 && codePoint < 0xE000)
        {
            continue;
        }
        listed += s_offsets[codePoint] != NONE ? 1 : 0;
        char input[4];
        size_t inputSize = 0;
        (void)muniEncodeUtf8(codePoint, input, sizeof(input), &inputSize);
        char output[128];
        size_t needed = 0;
        bool inFile = s_offsets[codePoint] != NONE;
        const char* expected = inFile ? s_pool + s_offsets[codePoint] : input;
        size_t expectedSize = inFile ? strlen(expected) : inputSize;
        muniTextResult result = muniGetNfkcCasefold(input, inputSize, muni_convertStrict, output,
                                                    sizeof(output), &needed);
        if ((result.status != muni_success || needed != expectedSize ||
             memcmp(output, expected, needed) != 0) &&
            failures++ < 10)
        {
            printf("NFKC_CF of U+%04X differs\n", (unsigned)codePoint);
        }
    }
    printf("NFKC_CF: %d code points listed, %d failures\n", listed, failures);
    CHECK(listed > 10000 && failures == 0, "NFKC_CF of every code point matches the UCD");
}

static bool Casefolds(const char* text, const char* expected)
{
    char output[128];
    size_t needed = 0;
    muniTextResult result = muniGetNfkcCasefold(text, strlen(text), muni_convertStrict, output,
                                                sizeof(output), &needed);
    return result.status == muni_success && needed == strlen(expected) &&
           memcmp(output, expected, needed) == 0;
}

static void TestCasefoldText(void)
{
    CHECK(Casefolds("\xEF\xBC\xA1LICE", "alice"), "a full-width A");
    CHECK(Casefolds("Stra\xC3\x9F"
                    "e",
                    "strasse"),
          "sharp s");
    CHECK(Casefolds("pay\xC2\xADpal\xE2\x80\x8B", "paypal"), "default ignorables go");
    CHECK(Casefolds("E\xCC\x81", "\xC3\xA9"), "composed across code points");
    // Long s with a dot above, then a dot below: s, dot below, dot above.
    CHECK(Casefolds("\xE1\xBA\x9B\xCC\xA3", "\xE1\xB9\xA9"), "reordered, then composed");
    CHECK(Casefolds("\xE2\x84\xAB", "\xC3\xA5"), "the Angstrom sign");
    CHECK(Casefolds("", ""), "empty text");
    char output[4];
    size_t needed = 0;
    muniTextResult result =
        muniGetNfkcCasefold("ABCDEF", 6, muni_convertStrict, output, sizeof(output), &needed);
    CHECK(result.status == muni_errorCapacity && needed == 6 && memcmp(output, "abcd", 4) == 0,
          "capacity: the bytes that fit, the size needed");
    result = muniGetNfkcCasefold("A\xFF", 2, muni_convertStrict, output, sizeof(output), &needed);
    CHECK(result.status == muni_errorUtf8Lead && result.offset == 1 && needed == 1,
          "strict mode stops at ill-formed input");
    result = muniGetNfkcCasefold("A\xFF", 2, muni_convertReplace, output, sizeof(output), &needed);
    CHECK(result.status == muni_success && needed == 4 && memcmp(output, "a\xEF\xBF\xBD", 4) == 0,
          "replace mode");
    CHECK(muniGetNfkcCasefold("a", 1, 7, output, sizeof(output), &needed).status ==
              muni_errorInvalid,
          "an unknown mode");
}

// The simple mappings and Cased of every code point against the UCD
// files, read here without the generator: UnicodeData.txt, whose
// titlecase is the uppercase when its field is empty, CaseFolding.txt's
// C and S lines, and DerivedCoreProperties.txt. A code point a file
// does not list maps to itself.
static uint8_t s_listed[0x110000];

// Counts the code points where map differs from the file: field of
// UnicodeData.txt after the code point, or folding for -1.
static int CompareSimple(int field, uint32_t (*map)(uint32_t))
{
    FILE* file = UcdOpen(field < 0 ? "CaseFolding.txt" : "UnicodeData.txt");
    if (file == nullptr)
    {
        return 1;
    }
    memset(s_listed, 0, sizeof(s_listed));
    static char buffer[4096];
    UcdLine line;
    int failures = 0;
    while (UcdNext(file, buffer, sizeof(buffer), &line))
    {
        const char* value = nullptr;
        if (field < 0)
        {
            bool simple = strcmp(line.fields[0], "C") == 0 || strcmp(line.fields[0], "S") == 0;
            value = simple ? line.fields[1] : "";
        }
        else
        {
            value =
                line.fields[field][0] != '\0' || field != 13 ? line.fields[field] : line.fields[11];
        }
        if (value[0] != '\0')
        {
            s_listed[line.first] = 1;
            if (map(line.first) != (uint32_t)strtoul(value, nullptr, 16) && failures++ < 5)
            {
                printf("the simple mapping of U+%04X differs\n", (unsigned)line.first);
            }
        }
    }
    fclose(file);
    for (uint32_t c = 0; c < UCD_CODE_POINTS; c++)
    {
        if (s_listed[c] == 0 && map(c) != c && failures++ < 5)
        {
            printf("U+%04X maps to another code point\n", (unsigned)c);
        }
    }
    return failures;
}

static void TestSimpleAgainstTheUcd(void)
{
    CHECK(CompareSimple(11, muniGetUppercase) == 0, "Simple_Uppercase_Mapping");
    CHECK(CompareSimple(12, muniGetLowercase) == 0, "Simple_Lowercase_Mapping");
    CHECK(CompareSimple(13, muniGetTitlecase) == 0, "Simple_Titlecase_Mapping");
    CHECK(CompareSimple(-1, muniFoldCase) == 0, "Simple_Case_Folding");
    memset(s_listed, 0, sizeof(s_listed));
    bool read = UcdFillBinary("DerivedCoreProperties.txt", "Cased", s_listed);
    int failures = 0;
    for (uint32_t c = 0; c < UCD_CODE_POINTS; c++)
    {
        if ((muniIsCased(c) ? 1 : 0) != s_listed[c] && failures++ < 5)
        {
            printf("Cased of U+%04X differs\n", (unsigned)c);
        }
    }
    CHECK(read && failures == 0, "Cased");
}

// A full mapping of one code point: from SpecialCasing.txt, with the
// languages and contexts it needs, or CaseFolding.txt's F and T lines.
typedef struct FullMapping
{
    uint32_t codePoint;
    muniCaseOperation operation;
    muniCaseLanguage language; // muni_caseDefault for every language
    bool alone;                // its contexts hold for a code point alone
    uint32_t to[3];
    size_t count;
} FullMapping;

static FullMapping s_full[512];
static size_t s_fullCount;
static uint8_t s_hasFull[0x110000];

static void AddFull(uint32_t codePoint, muniCaseOperation operation, muniCaseLanguage language,
                    bool alone, char* text)
{
    if (s_fullCount == sizeof(s_full) / sizeof(s_full[0]))
    {
        return;
    }
    FullMapping* mapping = &s_full[s_fullCount++];
    *mapping = (FullMapping){codePoint, operation, language, alone, {0}, 0};
    for (char* item = strtok(text, " "); item != nullptr && mapping->count < 3;
         item = strtok(nullptr, " "))
    {
        mapping->to[mapping->count++] = (uint32_t)strtoul(item, nullptr, 16);
    }
    s_hasFull[codePoint] = 1;
}

// Reads the conditions of a SpecialCasing.txt line: its language and
// whether its contexts hold for a code point with nothing around it.
// Only Not_Before_Dot does; Final_Sigma, After_Soft_Dotted, More_Above,
// Before_Dot and After_I need a neighbor. False for a condition this
// does not know.
static bool ReadConditions(char* text, muniCaseLanguage* language, bool* alone)
{
    static const char* const needNeighbor[] = {"Final_Sigma", "After_Soft_Dotted", "More_Above",
                                               "Before_Dot", "After_I"};
    *language = muni_caseDefault;
    *alone = true;
    for (char* item = strtok(text, " "); item != nullptr; item = strtok(nullptr, " "))
    {
        bool known = true;
        if (strcmp(item, "tr") == 0 || strcmp(item, "az") == 0)
        {
            *language = muni_caseTurkic;
        }
        else if (strcmp(item, "lt") == 0)
        {
            *language = muni_caseLithuanian;
        }
        else if (strcmp(item, "Not_Before_Dot") != 0)
        {
            known = false;
            for (size_t i = 0; i < sizeof(needNeighbor) / sizeof(needNeighbor[0]); i++)
            {
                known = known || strcmp(item, needNeighbor[i]) == 0;
            }
            *alone = false;
        }
        if (!known)
        {
            printf("SpecialCasing.txt: an unknown condition %s\n", item);
            return false;
        }
    }
    return true;
}

static bool LoadFull(void)
{
    s_fullCount = 0;
    memset(s_hasFull, 0, sizeof(s_hasFull));
    static char buffer[4096];
    UcdLine line;
    FILE* file = UcdOpen("SpecialCasing.txt");
    if (file == nullptr)
    {
        return false;
    }
    bool known = true;
    while (UcdNext(file, buffer, sizeof(buffer), &line))
    {
        muniCaseLanguage language = muni_caseDefault;
        bool alone = true;
        if (line.count > 3)
        {
            known = ReadConditions(line.fields[3], &language, &alone) && known;
        }
        AddFull(line.first, muni_caseLower, language, alone, line.fields[0]);
        AddFull(line.first, muni_caseTitle, language, alone, line.fields[1]);
        AddFull(line.first, muni_caseUpper, language, alone, line.fields[2]);
    }
    fclose(file);
    file = UcdOpen("CaseFolding.txt");
    if (file == nullptr)
    {
        return false;
    }
    while (UcdNext(file, buffer, sizeof(buffer), &line))
    {
        if (strcmp(line.fields[0], "F") == 0 || strcmp(line.fields[0], "T") == 0)
        {
            bool turkic = line.fields[0][0] == 'T';
            AddFull(line.first, muni_caseFold, turkic ? muni_caseTurkic : muni_caseDefault, true,
                    line.fields[1]);
        }
    }
    fclose(file);
    return known && s_fullCount > 200 && s_fullCount < sizeof(s_full) / sizeof(s_full[0]);
}

// The full mapping of a code point alone: the entry of its language
// before one for every language, else its simple mapping.
static size_t ExpectedFull(uint32_t codePoint, muniCaseOperation operation,
                           muniCaseLanguage language, uint32_t* to)
{
    const FullMapping* found = nullptr;
    for (size_t i = 0; i < s_fullCount && s_hasFull[codePoint] != 0; i++)
    {
        const FullMapping* mapping = &s_full[i];
        if (mapping->codePoint == codePoint && mapping->operation == operation && mapping->alone &&
            (mapping->language == language || mapping->language == muni_caseDefault) &&
            (found == nullptr || mapping->language != muni_caseDefault))
        {
            found = mapping;
        }
    }
    if (found != nullptr)
    {
        memcpy(to, found->to, found->count * sizeof(uint32_t));
        return found->count;
    }
    uint32_t (*const simple[])(uint32_t) = {muniGetLowercase, muniGetUppercase, muniGetTitlecase,
                                            muniFoldCase};
    to[0] = simple[operation](codePoint);
    return 1;
}

static void TestFullAgainstTheUcd(void)
{
    CHECK(LoadFull(), "SpecialCasing.txt and CaseFolding.txt read");
    int failures = 0;
    for (uint32_t c = 0; c < UCD_CODE_POINTS; c++)
    {
        if (c >= 0xD800 && c < 0xE000)
        {
            continue;
        }
        char input[4];
        size_t inputSize = 0;
        (void)muniEncodeUtf8(c, input, sizeof(input), &inputSize);
        for (int language = muni_caseDefault; language <= muni_caseLithuanian; language++)
        {
            for (int operation = muni_caseLower; operation <= muni_caseFold; operation++)
            {
                uint32_t to[3];
                size_t count = ExpectedFull(c, operation, language, to);
                char expected[12];
                size_t expectedSize = 0;
                for (size_t i = 0; i < count; i++)
                {
                    size_t size = 0;
                    (void)muniEncodeUtf8(to[i], expected + expectedSize, 4, &size);
                    expectedSize += size;
                }
                char output[64];
                size_t needed = 0;
                muniTextResult result =
                    muniConvertCase(input, inputSize, operation, language, muni_convertStrict,
                                    output, sizeof(output), &needed);
                if ((result.status != muni_success || needed != expectedSize ||
                     memcmp(output, expected, needed) != 0) &&
                    failures++ < 5)
                {
                    printf("operation %d, language %d of U+%04X differs\n", operation, language,
                           (unsigned)c);
                }
            }
        }
    }
    CHECK(failures == 0, "the full mappings of every code point alone match the UCD");
}

// What UnicodeData.txt and Scripts.txt say of the code points below
// GREEK_LIMIT: letter or not, the canonical decomposition, the simple
// uppercase, and the Greek script.
#define GREEK_LIMIT 0x2200
static uint32_t s_decompositions[GREEK_LIMIT][2];
static uint32_t s_uppers[GREEK_LIMIT];
static bool s_letters[GREEK_LIMIT];

static bool LoadGreek(void)
{
    FILE* file = UcdOpen("UnicodeData.txt");
    if (file == nullptr)
    {
        return false;
    }
    static char buffer[4096];
    UcdLine line;
    while (UcdNext(file, buffer, sizeof(buffer), &line))
    {
        if (line.first >= GREEK_LIMIT || line.count < 14)
        {
            continue;
        }
        s_letters[line.first] = line.fields[1][0] == 'L';
        s_uppers[line.first] = line.fields[11][0] != '\0'
                                   ? (uint32_t)strtoul(line.fields[11], nullptr, 16)
                                   : line.first;
        if (line.fields[4][0] != '\0' && line.fields[4][0] != '<')
        {
            char* end = nullptr;
            s_decompositions[line.first][0] = (uint32_t)strtoul(line.fields[4], &end, 16);
            s_decompositions[line.first][1] = (uint32_t)strtoul(end, nullptr, 16);
        }
    }
    fclose(file);
    memset(s_listed, 0, sizeof(s_listed));
    return UcdFillBinary("Scripts.txt", "Greek", s_listed);
}

// A Greek letter as Greek uppercasing sees it: the uppercase of its
// base and the marks its decomposition puts on it.
typedef struct GreekLetter
{
    uint32_t upper; // 0 for no Greek letter
    bool vowel;
    bool accent;
    bool dialytika;
    bool subscript;
} GreekLetter;

static GreekLetter ReadGreekLetter(uint32_t letter)
{
    GreekLetter read = {0};
    if (letter >= GREEK_LIMIT || !s_letters[letter] || s_listed[letter] == 0)
    {
        return read;
    }
    uint32_t base = letter;
    while (s_decompositions[base][0] != 0)
    {
        uint32_t mark = s_decompositions[base][1];
        read.accent = read.accent || mark == 0x0300 || mark == 0x0301 || mark == 0x0342;
        read.dialytika = read.dialytika || mark == 0x0308;
        read.subscript = read.subscript || mark == 0x0345;
        base = s_decompositions[base][0];
    }
    if (base >= 0x370 && base <= 0x3FF)
    {
        read.upper = s_uppers[base];
        uint32_t upper = read.upper;
        read.vowel = upper == 0x391 || upper == 0x395 || upper == 0x397 || upper == 0x399 ||
                     upper == 0x39F || upper == 0x3A5 || upper == 0x3A9;
    }
    return read;
}

// Appends a code point's UTF-8 to text.
static void Append(char* text, size_t* length, uint32_t codePoint)
{
    size_t size = 0;
    (void)muniEncodeUtf8(codePoint, text + *length, 4, &size);
    *length += size;
}

// What follows a Greek letter in ExpectedGreekUpper.
typedef enum After
{
    AfterNothing,
    AfterIota,  // takes a dialytika when the letter is a vowel whose accent went
    AfterAcute, // a combining acute, which goes with the letter
} After;

// The Greek uppercase of a letter with nothing before it.
static size_t ExpectedGreekUpper(GreekLetter letter, After after, char* text)
{
    size_t length = 0;
    bool accent = letter.accent || after == AfterAcute;
    if (letter.upper == 0x397 && accent && !letter.subscript && after != AfterIota)
    {
        // The disjunctive eta keeps its tonos in the form it had.
        Append(text, &length, letter.accent ? 0x389 : 0x397);
        if (!letter.accent)
        {
            Append(text, &length, 0x301);
        }
    }
    else if (letter.dialytika && (letter.upper == 0x399 || letter.upper == 0x3A5))
    {
        Append(text, &length, letter.upper == 0x399 ? 0x3AA : 0x3AB);
    }
    else
    {
        Append(text, &length, letter.upper);
        if (letter.dialytika)
        {
            Append(text, &length, 0x308);
        }
    }
    if (letter.subscript)
    {
        Append(text, &length, 0x399);
    }
    if (after == AfterIota)
    {
        Append(text, &length, letter.vowel && letter.accent && !letter.dialytika ? 0x3AA : 0x399);
    }
    return length;
}

static bool Gives(const char* text, size_t length, muniCaseOperation operation,
                  muniCaseLanguage language, const char* expected, size_t expectedLength)
{
    char output[64];
    size_t needed = 0;
    muniTextResult result = muniConvertCase(text, length, operation, language, muni_convertStrict,
                                            output, sizeof(output), &needed);
    return result.status == muni_success && needed == expectedLength &&
           memcmp(output, expected, needed) == 0;
}

// Every Greek letter, alone, before an iota and before a combining acute,
// against its base and marks in UnicodeData.txt; every other code point
// as the default rules give it, a combining acute after it too.
static void TestGreekAgainstTheUcd(void)
{
    CHECK(LoadGreek(), "UnicodeData.txt and Scripts.txt read");
    int failures = 0;
    int letters = 0;
    for (uint32_t c = 0; c < UCD_CODE_POINTS; c++)
    {
        if (c >= 0xD800 && c < 0xE000)
        {
            continue;
        }
        char input[8];
        size_t inputLength = 0;
        Append(input, &inputLength, c);
        char expected[64];
        size_t expectedLength = 0;
        GreekLetter letter = ReadGreekLetter(c);
        bool inRange = (c >= 0x370 && c <= 0x3FF) || (c >= 0x1F00 && c <= 0x1FFF) || c == 0x2126;
        if (inRange && letter.upper != 0)
        {
            letters += 1;
            bool same = true;
            for (After after = AfterNothing; after <= AfterAcute; after++)
            {
                size_t length = inputLength;
                if (after != AfterNothing)
                {
                    Append(input, &length, after == AfterIota ? 0x3B9 : 0x301);
                }
                expectedLength = ExpectedGreekUpper(letter, after, expected);
                same = same && Gives(input, length, muni_caseUpper, muni_caseGreek, expected,
                                     expectedLength);
            }
            if (!same && failures++ < 5)
            {
                printf("the Greek uppercase of U+%04X differs\n", (unsigned)c);
            }
            continue;
        }
        for (int operation = muni_caseLower; operation <= muni_caseFold; operation++)
        {
            char output[64];
            size_t needed = 0;
            muniTextResult result =
                muniConvertCase(input, inputLength, operation, muni_caseDefault, muni_convertStrict,
                                output, sizeof(output), &needed);
            bool same = result.status == muni_success &&
                        Gives(input, inputLength, operation, muni_caseGreek, output, needed);
            bool ligature =
                c == 0x587 && (operation == muni_caseUpper || operation == muni_caseTitle);
            same = same && (ligature || Gives(input, inputLength, operation, muni_caseArmenian,
                                              output, needed));
            if (!same && failures++ < 5)
            {
                printf("operation %d of U+%04X differs from the default rules\n", operation,
                       (unsigned)c);
            }
        }
        if (inRange)
        {
            Append(input, &inputLength, 0x301);
            char output[64];
            size_t needed = 0;
            muniTextResult result =
                muniConvertCase(input, inputLength, muni_caseUpper, muni_caseDefault,
                                muni_convertStrict, output, sizeof(output), &needed);
            if ((result.status != muni_success ||
                 !Gives(input, inputLength, muni_caseUpper, muni_caseGreek, output, needed)) &&
                failures++ < 5)
            {
                printf("U+%04X with an acute differs from the default rules\n", (unsigned)c);
            }
        }
    }
    CHECK(failures == 0 && letters > 300,
          "Greek letters follow UnicodeData.txt, the rest the default rules");
}

#endif

// The two text mappings, with their other arguments fixed.
typedef muniTextResult (*MapFunction)(const char* text, size_t length, char* textOut,
                                      size_t capacity, size_t* neededOut);

static muniTextResult Uppercase(const char* text, size_t length, char* textOut, size_t capacity,
                                size_t* neededOut)
{
    return muniConvertCase(text, length, muni_caseUpper, muni_caseDefault, muni_convertStrict,
                           textOut, capacity, neededOut);
}

static muniTextResult Casefold(const char* text, size_t length, char* textOut, size_t capacity,
                               size_t* neededOut)
{
    return muniGetNfkcCasefold(text, length, muni_convertStrict, textOut, capacity, neededOut);
}

// An output that overlaps the text, on either side, is refused and
// nothing is written. The text is 8 bytes and so is its mapping, and the
// output starts 0, 1 or 7 bytes after the text or 1 or 7 bytes before
// it. Right beside the text, 8 bytes after or before it, it is apart.
static bool MapsApart(MapFunction map, const char* input, const char* expected)
{
    enum
    {
        size = 8
    };
    static const ptrdiff_t shifts[] = {0, 1, size - 1, -1, -(size - 1)};
    char buffer[3 * size];
    char* text = buffer + size;
    bool right = true;
    for (size_t i = 0; i < sizeof(shifts) / sizeof(shifts[0]); i++)
    {
        memset(buffer, '#', sizeof(buffer));
        memcpy(text, input, size);
        char before[sizeof(buffer)];
        memcpy(before, buffer, sizeof(buffer));
        size_t needed = 99;
        muniTextResult result = map(text, size, text + shifts[i], size, &needed);
        right = right && result.status == muni_errorInvalid && needed == 99 &&
                memcmp(buffer, before, sizeof(buffer)) == 0;
    }
    for (int side = 0; side < 2; side++)
    {
        memset(buffer, '#', sizeof(buffer));
        memcpy(text, input, size);
        size_t needed = 0;
        char* output = side == 0 ? text + size : text - size;
        muniTextResult result = map(text, size, output, size, &needed);
        right = right && result.status == muni_success && needed == size &&
                memcmp(output, expected, size) == 0 && memcmp(text, input, size) == 0;
    }
    return right;
}

static void TestOverlap(void)
{
    CHECK(MapsApart(Uppercase, "Abcdefgh", "ABCDEFGH"),
          "muniConvertCase refuses an overlapping output and writes one beside the text");
    CHECK(MapsApart(Casefold, "ABCDEFGH", "abcdefgh"),
          "muniGetNfkcCasefold refuses an overlapping output and writes one beside the text");
}

// A kind is an int32_t, so a caller can pass a negative one; every call
// that takes a kind refuses -1 and INT32_MIN as it refuses a kind past the
// last, and leaves its outputs as they were.
static const int32_t s_negative[] = {-1, INT32_MIN};

static void TestNegativeKinds(void)
{
    bool refused = true;
    for (size_t i = 0; i < sizeof(s_negative) / sizeof(s_negative[0]); i++)
    {
        int32_t bad = s_negative[i];
        char output[4] = {7, 7, 7, 7};
        size_t needed = 99;
        refused =
            refused &&
            muniConvertCase("a", 1, bad, muni_caseDefault, muni_convertStrict, output, 4, &needed)
                    .status == muni_errorInvalid &&
            muniConvertCase("a", 1, muni_caseUpper, bad, muni_convertStrict, output, 4, &needed)
                    .status == muni_errorInvalid &&
            muniConvertCase("a", 1, muni_caseUpper, muni_caseDefault, bad, output, 4, &needed)
                    .status == muni_errorInvalid &&
            muniGetNfkcCasefold("a", 1, bad, output, 4, &needed).status == muni_errorInvalid &&
            needed == 99 && output[0] == 7;
    }
    CHECK(refused, "a negative operation, language or mode refused");
}

int main(void)
{
    TestSimple();
    TestFull();
    TestTurkic();
    TestFinalSigma();
    TestNfkcCasefoldLimit();
    TestLithuanian();
    TestGreek();
    TestArmenian();
    TestErrors();
    TestOverlap();
    TestNegativeKinds();
#ifdef MUNI_UCD_DATA
    TestCasefoldEverywhere();
    TestCasefoldText();
    TestSimpleAgainstTheUcd();
    TestFullAgainstTheUcd();
    TestGreekAgainstTheUcd();
#endif
    return s_failures == 0 ? 0 : 1;
}
