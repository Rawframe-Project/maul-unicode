// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// UTS #39: the Security table against the generator's hash; every line
// of confusables.txt, whose source and prototype must share a skeleton
// that a second pass leaves alone; the bidi skeleton example of section
// 4; the resolved script sets of table 1a; restriction levels, mixed
// numbers and the errors.

#include "tables.h"
#include "test_harness.h"

#include "maul-unicode/security.h"

#include <stdlib.h>
#include <string.h>

#define MAX_TEXT 256

static uint64_t HashSecurity(void)
{
    uint64_t hash = 0xcbf29ce484222325ull;
    for (uint32_t codePoint = 0; codePoint < 0x110000; codePoint++)
    {
        hash ^= muniLookupSecurity(codePoint);
        hash *= 0x100000001b3ull;
    }
    return hash;
}

// The skeleton of text, NUL-terminated in out, or false on an error.
static bool Skeleton(const char* text, size_t length, muniBidiDirection direction, char* out)
{
    static uint8_t workspace[2 * MAX_TEXT * 4];
    size_t needed = 0;
    muniTextResult result =
        muniGetSkeleton(text, length, direction, workspace, out, MAX_TEXT * 4 - 1, &needed);
    if (result.status != muni_success)
    {
        return false;
    }
    out[needed] = '\0';
    return true;
}

static bool SameSkeleton(const char* a, const char* b, muniBidiDirection direction)
{
    char first[MAX_TEXT * 4];
    char second[MAX_TEXT * 4];
    return Skeleton(a, strlen(a), direction, first) && Skeleton(b, strlen(b), direction, second) &&
           strcmp(first, second) == 0;
}

// Whether text displays in logical order: no right-to-left class.
static bool IsLeftToRight(const char* text)
{
    size_t offset = 0;
    size_t length = strlen(text);
    while (offset < length)
    {
        uint32_t codePoint;
        size_t size;
        (void)muniDecodeUtf8(text + offset, length - offset, &codePoint, &size);
        muniBidiClass type = muniGetBidiClass(codePoint);
        if (type == muni_bcR || type == muni_bcAl || type == muni_bcAn || type == muni_bcRle ||
            type == muni_bcRlo || type == muni_bcRli || type == muni_bcFsi)
        {
            return false;
        }
        offset += size;
    }
    return true;
}

static size_t CodePoints(const char* text)
{
    size_t count = 0;
    for (const char* byte = text; *byte != '\0'; byte++)
    {
        count += ((uint8_t)*byte & 0xC0) != 0x80 ? 1 : 0;
    }
    return count;
}

// Reads the code points of a field into UTF-8.
static void ParseField(const char* field, char* out)
{
    size_t length = 0;
    const char* cursor = field;
    while (*cursor == ' ' || *cursor == '\t' || (*cursor >= '0' && *cursor <= '9') ||
           (*cursor >= 'A' && *cursor <= 'F'))
    {
        char* end = nullptr;
        unsigned long codePoint = strtoul(cursor, &end, 16);
        if (end == cursor)
        {
            cursor += 1;
            continue;
        }
        size_t size = 0;
        (void)muniEncodeUtf8((uint32_t)codePoint, out + length, &size);
        length += size;
        cursor = end;
    }
    out[length] = '\0';
}

// Every mapping: the source and its prototype share a skeleton, and the
// skeleton is its own skeleton. A mapping of several code points is
// compared only when it displays in logical order, as bidi reorders a
// spelled-out prototype of right-to-left letters and not the single code
// point of its source.
static void TestConfusables(void)
{
    FILE* file = fopen(MUNI_UCD_DATA "/confusables.txt", "r");
    CHECK(file != nullptr, "confusables.txt opens");
    if (file == nullptr)
    {
        return;
    }
    static char line[1024];
    int cases = 0;
    int failures = 0;
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* semicolon = strchr(line, ';');
        if (line[0] == '#' || semicolon == nullptr)
        {
            continue;
        }
        char source[64];
        char prototype[MAX_TEXT];
        char skeleton[MAX_TEXT * 4];
        char again[MAX_TEXT * 4];
        ParseField(line, source);
        ParseField(semicolon + 1, prototype);
        bool single = CodePoints(source) == 1 && CodePoints(prototype) == 1;
        if (!single && (!IsLeftToRight(source) || !IsLeftToRight(prototype)))
        {
            continue;
        }
        cases += 1;
        bool same = SameSkeleton(source, prototype, muni_bidiLeftToRight) &&
                    Skeleton(prototype, strlen(prototype), muni_bidiLeftToRight, skeleton) &&
                    Skeleton(skeleton, strlen(skeleton), muni_bidiLeftToRight, again) &&
                    strcmp(skeleton, again) == 0;
        if (!same && failures++ < 10)
        {
            printf("confusables.txt: %s", line);
        }
    }
    fclose(file);
    printf("confusables.txt: %d cases, %d failures\n", cases, failures);
    CHECK(cases > 6000 && failures == 0, "every confusable bidi leaves in order");
}

static void TestSkeletons(void)
{
    CHECK(SameSkeleton("paypal", "p\xD0\xB0yp\xD0\xB0l", muni_bidiLeftToRight),
          "paypal with Cyrillic a");
    CHECK(SameSkeleton("scope", "\xD1\x95\xD1\x81\xD0\xBE\xD1\x80\xD0\xB5", muni_bidiLeftToRight),
          "scope in Cyrillic");
    CHECK(SameSkeleton("\xC7\x89"
                       "eto",
                       "ljeto", muni_bidiLeftToRight),
          "the lj ligature");
    CHECK(SameSkeleton("rn", "m", muni_bidiLeftToRight), "rn and m");
    CHECK(!SameSkeleton("cat", "dog", muni_bidiLeftToRight), "different words");
    CHECK(SameSkeleton("a\xE2\x80\x8D"
                       "b",
                       "ab", muni_bidiLeftToRight),
          "a zero width joiner is ignored");
    CHECK(SameSkeleton("\xC3\xA9", "e\xCC\x81", muni_bidiLeftToRight), "NFD first");
    // The example of UTS #39 section 4: an A, a one, a less-than sign and
    // a shin with a sin dot; an alpha, a shin with a holam haser, a
    // greater-than sign and a one. They look alike left to right but not
    // right to left.
    const char* first = "A1<\xD7\xA9\xD7\x82";
    const char* second = "\xCE\x91\xD7\xA9\xD6\xBA>1";
    char skeleton[MAX_TEXT * 4];
    CHECK(Skeleton(first, strlen(first), muni_bidiLeftToRight, skeleton) &&
              strcmp(skeleton, "Al<\xD7\xA9\xCC\x87") == 0,
          "the bidi skeleton of the first");
    CHECK(SameSkeleton(first, second, muni_bidiLeftToRight), "the two are LTR-confusable");
    CHECK(!SameSkeleton(first, second, muni_bidiRightToLeft), "the two are not RTL-confusable");
    CHECK(SameSkeleton("(", ")", muni_bidiRightToLeft) == false &&
              Skeleton("(", 1, muni_bidiRightToLeft, skeleton) &&
              SameSkeleton(skeleton, ")", muni_bidiLeftToRight),
          "a right-to-left paragraph mirrors");
    CHECK(SameSkeleton("\xD7\x90\xD7\x91", "\xD7\x91\xD7\x90", muni_bidiLeftToRight) == false &&
              Skeleton("\xD7\x90\xD7\x91", 4, muni_bidiLeftToRight, skeleton) &&
              strncmp(skeleton + strlen(skeleton) - 2, "\xD7\x90", 2) == 0,
          "Hebrew displays reversed");
    CHECK(SameSkeleton("abc", "abc", muni_bidiAuto) && SameSkeleton("x\xE2\x80\xAE"
                                                                    "ab",
                                                                    "xba", muni_bidiLeftToRight),
          "an override reverses");
}

static bool Resolves(const char* text, const muniScript* expected, size_t expectedCount)
{
    muniScript scripts[64];
    size_t count = 0;
    muniTextResult result = muniGetResolvedScripts(text, strlen(text), scripts, 64, &count);
    return result.status == muni_success && count == expectedCount &&
           (count == 0 || memcmp(scripts, expected, sizeof(muniScript) * count) == 0);
}

static void TestScripts(void)
{
    const muniScript latin[] = {MUNI_SCRIPT('H', 'n', 't', 'l'), MUNI_SCRIPT('L', 'a', 't', 'n')};
    const muniScript cyrillic[] = {MUNI_SCRIPT('C', 'y', 'r', 'l')};
    const muniScript all[] = {MUNI_SCRIPT_COMMON};
    const muniScript han[] = {MUNI_SCRIPT('H', 'a', 'n', 'b'), MUNI_SCRIPT('H', 'a', 'n', 'i'),
                              MUNI_SCRIPT('H', 'n', 't', 'l'), MUNI_SCRIPT('J', 'p', 'a', 'n'),
                              MUNI_SCRIPT('K', 'o', 'r', 'e')};
    const muniScript japanese[] = {MUNI_SCRIPT('J', 'p', 'a', 'n')};
    CHECK(Resolves("Circle", latin, 2), "Circle");
    CHECK(Resolves("\xD0\xA1\xD1\x96\xD0\xB3\xD1\x81\xD3\x80\xD0\xB5", cyrillic, 1), "Cyrillic");
    CHECK(Resolves("\xD0\xA1ir\xD1\x81l\xD0\xB5", nullptr, 0), "mixed Latin and Cyrillic");
    CHECK(Resolves("Circ1e", latin, 2), "a digit fits any script");
    CHECK(Resolves("C\xF0\x9D\x97\x82\xF0\x9D\x97\x8B", latin, 2), "math letters are Common");
    CHECK(Resolves("\xF0\x9D\x96\xA2\xF0\x9D\x97\x82", all, 1), "Common alone is every script");
    CHECK(Resolves("", all, 1), "empty text is every script");
    CHECK(Resolves("\xE3\x80\x86\xE5\x88\x87", han, 5), "Han with its writing systems");
    CHECK(Resolves("\xE3\x81\xAD\xE3\x82\xAC", japanese, 1), "Hiragana and Katakana are Jpan");
    muniScript one[1];
    size_t count = 0;
    CHECK(muniGetResolvedScripts("abc", 3, one, 1, &count).status == muni_errorCapacity &&
              count == 2,
          "the scripts do not fit");
    CHECK(muniGetResolvedScripts("\xD0\xA1i", 3, nullptr, 0, &count).status == muni_success &&
              count == 0,
          "no output for mixed scripts");
    size_t most = 0;
    muniScript scripts[64];
    for (uint32_t codePoint = 0; codePoint < 0x110000; codePoint += 1)
    {
        char text[4];
        size_t size = 0;
        if (muniEncodeUtf8(codePoint, text, &size) == muni_success &&
            muniGetResolvedScripts(text, size, scripts, 64, &count).status == muni_success &&
            count > most)
        {
            most = count;
        }
    }
    printf("the largest resolved script set has %zu scripts\n", most);
    CHECK(most <= 64, "64 scripts are always enough");
}

static muniRestrictionLevel Level(const char* text)
{
    muniRestrictionLevel level = 0;
    muniTextResult result = muniGetRestrictionLevel(text, strlen(text), &level);
    return result.status == muni_success ? level : 0;
}

static void TestLevels(void)
{
    CHECK(Level("") == muni_restrictionAsciiOnly, "empty text");
    CHECK(Level("user_name") == muni_restrictionAsciiOnly, "ASCII");
    CHECK(Level("a!") == muni_restrictionUnrestricted, "! is outside the profile");
    CHECK(Level("\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82") == muni_restrictionSingleScript,
          "Cyrillic");
    CHECK(Level("caf\xC3\xA9") == muni_restrictionSingleScript, "Latin beyond ASCII");
    CHECK(Level("abc\xE3\x81\xAD\xE5\x88\x87") == muni_restrictionHighlyRestrictive,
          "Latin, Hiragana and Han");
    CHECK(Level("a\xE5\x88\x87\xED\x95\x9C") == muni_restrictionHighlyRestrictive,
          "Latin, Han and Hangul");
    CHECK(Level("abc\xD8\xA8") == muni_restrictionModeratelyRestrictive, "Latin and Arabic");
    CHECK(Level("abc\xCE\xB1") == muni_restrictionMinimallyRestrictive, "Latin and Greek");
    CHECK(Level("p\xD0\xB0ypal") == muni_restrictionMinimallyRestrictive, "Latin and Cyrillic");
    CHECK(Level("\xD7\x90\xD8\xA8") == muni_restrictionMinimallyRestrictive, "Hebrew and Arabic");
    CHECK(muniIsIdentifierAllowed('a') && !muniIsIdentifierAllowed('!') &&
              muniIsIdentifierAllowed(0x0430) && !muniIsIdentifierAllowed(0x00AD) &&
              !muniIsIdentifierAllowed(0x110000),
          "Identifier_Status");
}

static bool Mixed(const char* text)
{
    bool mixed = false;
    return muniCheckMixedNumbers(text, strlen(text), &mixed).status == muni_success && mixed;
}

static void TestNumbers(void)
{
    CHECK(!Mixed("abc123"), "ASCII digits");
    CHECK(Mixed("1\xE0\xA7\xA8"
                "3"),
          "ASCII and Bengali");
    CHECK(!Mixed("\xE0\xA7\xA7\xE0\xA7\xA8"), "Bengali alone");
    CHECK(Mixed("\xD9\xA0\xDB\xB0"), "Arabic-Indic and extended Arabic-Indic zeros");
    CHECK(Mixed("\xF0\x9D\x9F\x8E\xF0\x9D\x9F\x98"), "bold and double-struck math digits");
    CHECK(!Mixed("\xF0\x9D\x9F\x8F\xF0\x9D\x9F\x97"), "bold math digits alone");
}

static void TestErrors(void)
{
    uint8_t workspace[256];
    char output[256];
    size_t needed = 0;
    muniTextResult result =
        muniGetSkeleton("ab\xFF", 3, muni_bidiLeftToRight, workspace, output, 256, &needed);
    CHECK(result.status == muni_errorUtf8Lead && result.offset == 2, "ill-formed text");
    char marks[1 + 40 * 2];
    marks[0] = 'a';
    for (int i = 0; i < 40; i++)
    {
        marks[1 + 2 * i] = (char)0xCC;
        marks[2 + 2 * i] = (char)0x81;
    }
    result = muniGetSkeleton(marks, sizeof(marks), muni_bidiLeftToRight, workspace, output, 256,
                             &needed);
    CHECK(result.status == muni_errorLimit && result.offset > 0, "too many marks");
    result = muniGetSkeleton("paypal", 6, muni_bidiLeftToRight, workspace, output, 2, &needed);
    CHECK(result.status == muni_errorCapacity && needed == 6 && memcmp(output, "pa", 2) == 0,
          "a short output");
    CHECK(muniGetSkeleton("a", 1, 3, workspace, output, 256, &needed).status == muni_errorInvalid,
          "an unknown direction");
    CHECK(muniGetSkeleton("a", 1, muni_bidiAuto, nullptr, output, 256, &needed).status ==
              muni_errorInvalid,
          "no workspace");
    CHECK(muniGetSkeleton(nullptr, 0, muni_bidiAuto, nullptr, nullptr, 0, &needed).status ==
                  muni_success &&
              needed == 0,
          "empty text");
    muniRestrictionLevel level = 0;
    result = muniGetRestrictionLevel("a\xC0", 2, &level);
    CHECK(result.status != muni_success && result.offset == 1, "ill-formed text has no level");
    bool mixed = false;
    CHECK(muniCheckMixedNumbers("1", 1, nullptr).status == muni_errorInvalid &&
              muniCheckMixedNumbers("\xFF", 1, &mixed).status == muni_errorUtf8Lead,
          "mixed number errors");
}

int main(void)
{
    CHECK(HashSecurity() == muniSecurityHash, "the Security table matches the generator");
    TestConfusables();
    TestSkeletons();
    TestScripts();
    TestLevels();
    TestNumbers();
    TestErrors();
    return s_failures == 0 ? 0 : 1;
}
