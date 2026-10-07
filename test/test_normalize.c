// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Normalization against NormalizationTest.txt: every invariant of its
// five columns under the four forms, and, as its Part 1 asks, every code
// point it does not list left alone by all four. Quick checks are held
// to the results: a Yes must leave the text alone and a No must change
// it.

#include "test_harness.h"

#include "maul-unicode/encoding.h"
#include "maul-unicode/normalize.h"

#include <stdlib.h>
#include <string.h>

#define MAX_TEXT 512

typedef struct Text
{
    char bytes[MAX_TEXT];
    size_t length;
} Text;

static bool ParseColumn(const char* field, Text* out)
{
    out->length = 0;
    const char* cursor = field;
    while (*cursor != '\0' && *cursor != ';')
    {
        char* end = nullptr;
        unsigned long codePoint = strtoul(cursor, &end, 16);
        if (end == cursor)
        {
            cursor += 1;
            continue;
        }
        size_t size = 0;
        if (out->length + 4 > MAX_TEXT ||
            muniEncodeUtf8((uint32_t)codePoint, out->bytes + out->length, &size) != muni_success)
        {
            return false;
        }
        out->length += size;
        cursor = end;
    }
    return true;
}

// Whether the form of source is expected, and the quick check agrees.
static bool Normalizes(const Text* source, muniNormalForm form, const Text* expected)
{
    static char output[MAX_TEXT * 18];
    size_t needed = 0;
    muniTextResult result = muniNormalize(source->bytes, source->length, form, muni_convertStrict,
                                          output, sizeof(output), &needed);
    if (result.status != muni_success || needed != expected->length ||
        memcmp(output, expected->bytes, needed) != 0)
    {
        return false;
    }
    muniQuickCheck answer = muni_quickCheckMaybe;
    if (muniCheckNormalization(source->bytes, source->length, form, &answer).status != muni_success)
    {
        return false;
    }
    bool same = needed == source->length && memcmp(output, source->bytes, needed) == 0;
    return answer == muni_quickCheckMaybe || (answer == muni_quickCheckYes) == same;
}

// The invariants of one line: c2 = NFC(c1..c3), c4 = NFC(c4, c5),
// c3 = NFD(c1..c3), c5 = NFD(c4, c5), c4 = NFKC(c1..c5), c5 = NFKD(c1..c5).
static bool CheckLine(const Text* c)
{
    for (int i = 0; i < 5; i++)
    {
        const Text* composed = i < 3 ? &c[1] : &c[3];
        const Text* decomposed = i < 3 ? &c[2] : &c[4];
        if (!Normalizes(&c[i], muni_nfc, composed) || !Normalizes(&c[i], muni_nfd, decomposed) ||
            !Normalizes(&c[i], muni_nfkc, &c[3]) || !Normalizes(&c[i], muni_nfkd, &c[4]))
        {
            return false;
        }
    }
    return true;
}

static void TestConformance(void)
{
    FILE* file = fopen(MUNI_TEST_DATA "/NormalizationTest.txt", "r");
    CHECK(file != nullptr, "NormalizationTest.txt opens");
    if (file == nullptr)
    {
        return;
    }
    static uint8_t listed[0x110000];
    static char line[4096];
    static Text columns[5];
    int part = -1;
    int cases = 0;
    int failures = 0;
    int lineNumber = 0;
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        lineNumber += 1;
        if (strncmp(line, "@Part", 5) == 0)
        {
            part = atoi(line + 5);
            continue;
        }
        if (line[0] == '#' || line[0] == '\n')
        {
            continue;
        }
        const char* field = line;
        bool parsed = true;
        for (int i = 0; i < 5 && parsed; i++)
        {
            parsed = ParseColumn(field, &columns[i]);
            field = strchr(field, ';') + 1;
        }
        if (part == 1)
        {
            listed[strtoul(line, nullptr, 16)] = 1;
        }
        cases += 1;
        if (!parsed || !CheckLine(columns))
        {
            if (failures < 10)
            {
                printf("NormalizationTest.txt:%d: %s", lineNumber, line);
            }
            failures += 1;
        }
    }
    fclose(file);
    printf("NormalizationTest.txt: %d cases, %d failures\n", cases, failures);
    CHECK(cases > 19000 && failures == 0, "every NormalizationTest case");
    int unchanged = 0;
    for (uint32_t codePoint = 0; codePoint < 0x110000; codePoint++)
    {
        if (listed[codePoint] || (codePoint >= 0xD800 && codePoint <= 0xDFFF))
        {
            continue;
        }
        Text single;
        (void)muniEncodeUtf8(codePoint, single.bytes, &single.length);
        bool same = true;
        for (muniNormalForm form = 0; form < 4 && same; form++)
        {
            same = Normalizes(&single, form, &single);
        }
        unchanged += same ? 0 : 1;
        if (!same && unchanged <= 5)
        {
            printf("U+%04X changes but is not in Part 1\n", codePoint);
        }
    }
    CHECK(unchanged == 0, "code points outside Part 1 stay the same");
}

static void TestLimitsAndErrors(void)
{
    char output[256];
    size_t needed = 0;
    // "a" and 40 combining acute accents: more marks than a segment holds.
    char marks[1 + 40 * 2];
    marks[0] = 'a';
    for (int i = 0; i < 40; i++)
    {
        marks[1 + 2 * i] = (char)0xCC;
        marks[2 + 2 * i] = (char)0x81;
    }
    muniTextResult result =
        muniNormalize(marks, sizeof(marks), muni_nfc, muni_convertStrict, output, 256, &needed);
    CHECK(result.status == muni_errorLimit && result.offset > 0, "too many marks is refused");
    result = muniNormalize("e\xCC\x81", 3, muni_nfc, muni_convertStrict, output, 1, &needed);
    CHECK(result.status == muni_errorCapacity && needed == 2, "e and acute need two bytes");
    result = muniNormalize("a\xFF", 2, muni_nfc, muni_convertStrict, output, 256, &needed);
    CHECK(result.status == muni_errorUtf8Lead && result.offset == 1 && needed == 1,
          "ill-formed input in strict mode");
    result = muniNormalize("a\xFF", 2, muni_nfc, muni_convertReplace, output, 256, &needed);
    CHECK(result.status == muni_success && needed == 4, "ill-formed input replaced");
    CHECK(muniNormalize("a", 1, 7, muni_convertStrict, output, 256, &needed).status ==
              muni_errorInvalid,
          "an unknown form");
    uint32_t first = 0;
    uint32_t second = 0;
    CHECK(muniDecomposePair(0x00E9, &first, &second) && first == 'e' && second == 0x0301,
          "e acute decomposes");
    CHECK(muniDecomposePair(0xAC01, &first, &second) && first == 0xAC00 && second == 0x11A8,
          "a Hangul LVT syllable splits into LV and T");
    CHECK(!muniDecomposePair('a', &first, &second), "a does not decompose");
    CHECK(muniComposePair('e', 0x0301) == 0x00E9, "e and acute compose");
    CHECK(muniComposePair(0x1100, 0x1161) == 0xAC00, "Hangul L and V compose");
    CHECK(muniComposePair(0x0915, 0x093C) == 0, "U+0958 is excluded from composition");
}

// More of the limits: an output that fits exactly, the quick check of
// ill-formed text, and the trailing consonants a Hangul LV syllable
// takes, U+11A8 to U+11C2 and no further.
static void TestEdges(void)
{
    char output[4];
    size_t needed = 0;
    muniTextResult result =
        muniNormalize("e\xCC\x81", 3, muni_nfc, muni_convertStrict, output, 2, &needed);
    CHECK(result.status == muni_success && needed == 2 && memcmp(output, "\xC3\xA9", 2) == 0,
          "two bytes into two");
    muniQuickCheck answer = muni_quickCheckYes;
    result = muniCheckNormalization("a\xFF", 2, muni_nfc, &answer);
    CHECK(result.status == muni_errorUtf8Lead && result.offset == 1, "the check finds the fault");
    CHECK(muniComposePair(0xAC00, 0x11C2) == 0xAC1B, "the last trailing consonant composes");
    CHECK(muniComposePair(0xAC00, 0x11C3) == 0, "the next code point does not");
}

#ifdef MUNI_UCD_DATA
// The quick check property of each form, from DerivedNormalizationProps.txt:
// Yes unless listed.
static uint8_t s_quickCheck[4][0x110000];

static bool LoadQuickChecks(void)
{
    FILE* file = fopen(MUNI_UCD_DATA "/DerivedNormalizationProps.txt", "r");
    if (file == nullptr)
    {
        return false;
    }
    static const struct
    {
        const char* name;
        muniNormalForm form;
    } properties[] = {{"; NFC_QC; ", muni_nfc},
                      {"; NFD_QC; ", muni_nfd},
                      {"; NFKC_QC; ", muni_nfkc},
                      {"; NFKD_QC; ", muni_nfkd}};
    static char line[1024];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        for (size_t p = 0; p < 4; p++)
        {
            const char* at = strstr(line, properties[p].name);
            if (line[0] == '#' || at == nullptr)
            {
                continue;
            }
            char value = at[strlen(properties[p].name)];
            char* end = nullptr;
            uint32_t first = (uint32_t)strtoul(line, &end, 16);
            uint32_t last = end[0] == '.' ? (uint32_t)strtoul(end + 2, &end, 16) : first;
            for (uint32_t codePoint = first; codePoint <= last; codePoint++)
            {
                s_quickCheck[properties[p].form][codePoint] =
                    value == 'N' ? muni_quickCheckNo : muni_quickCheckMaybe;
            }
        }
    }
    fclose(file);
    return true;
}

// Each code point alone gets its quick check property in every form.
static void TestQuickCheckEverywhere(void)
{
    CHECK(LoadQuickChecks(), "DerivedNormalizationProps.txt opens");
    int failures = 0;
    for (uint32_t codePoint = 0; codePoint < 0x110000; codePoint++)
    {
        char text[4];
        size_t size = 0;
        if (muniEncodeUtf8(codePoint, text, &size) != muni_success)
        {
            continue;
        }
        for (muniNormalForm form = 0; form < 4; form++)
        {
            muniQuickCheck answer = muni_quickCheckYes;
            muniTextResult result = muniCheckNormalization(text, size, form, &answer);
            if (result.status != muni_success || answer != s_quickCheck[form][codePoint])
            {
                if (failures++ < 5)
                {
                    printf("U+%04X form %d: %d, not %d\n", (unsigned)codePoint, (int)form,
                           (int)answer, (int)s_quickCheck[form][codePoint]);
                }
            }
        }
    }
    CHECK(failures == 0, "the quick check of every code point");
}
#endif

int main(void)
{
    TestConformance();
    TestLimitsAndErrors();
    TestEdges();
#ifdef MUNI_UCD_DATA
    TestQuickCheckEverywhere();
#endif
    return s_failures == 0 ? 0 : 1;
}
