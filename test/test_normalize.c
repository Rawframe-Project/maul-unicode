// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Normalization against NormalizationTest.txt: every invariant of its
// five columns under the four forms, and, as its Part 1 asks, every code
// point it does not list left alone by all four. Quick checks are held
// to the results: a Yes must leave the text alone and a No must change
// it. The decomposition and composition pairs of every code point
// against the UCD files.

#include "test_harness.h"

#include "maul-unicode/encoding.h"
#include "maul-unicode/normalize.h"

#ifdef MUNI_UCD_DATA
#include "ucd.h"
#endif

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
        if (muniEncodeUtf8((uint32_t)codePoint, out->bytes + out->length, MAX_TEXT - out->length,
                           &size) != muni_success)
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
    FILE* file = fopen(MUNI_UCD_DATA "/NormalizationTest.txt", "r");
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
        (void)muniEncodeUtf8(codePoint, single.bytes, sizeof(single.bytes), &single.length);
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

// muniCheckNormalization refuses a NULL text with a length, a NULL
// answer and an unknown form, and writes no answer.
static void TestCheckArguments(void)
{
    muniQuickCheck answer = muni_quickCheckMaybe;
    muniTextResult result = muniCheckNormalization(nullptr, 1, muni_nfc, &answer);
    CHECK(result.status == muni_errorInvalid && result.offset == 0 &&
              answer == muni_quickCheckMaybe,
          "no text with a length");
    CHECK(muniCheckNormalization("a", 1, muni_nfc, nullptr).status == muni_errorInvalid,
          "no answer");
    result = muniCheckNormalization("a", 1, (muniNormalForm)9, &answer);
    CHECK(result.status == muni_errorInvalid && answer == muni_quickCheckMaybe, "an unknown form");
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
    // A segment holds the starter and MUNI_MAX_NORMALIZATION_SEGMENT - 1
    // marks; the next mark, at byte 1 + 2 * 31, is the first refused.
    size_t fits = 1 + 2 * (MUNI_MAX_NORMALIZATION_SEGMENT - 1);
    result = muniNormalize(marks, fits, muni_nfd, muni_convertStrict, output, 256, &needed);
    CHECK(result.status == muni_success && needed == fits, "a full segment");
    result = muniNormalize(marks, fits + 2, muni_nfd, muni_convertStrict, output, 256, &needed);
    CHECK(result.status == muni_errorLimit && result.offset == fits, "one mark past the segment");
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
    size_t count = 9;
    CHECK(muniDecomposePair(0x00E9, &first, &second, &count) == muni_success && count == 2 &&
              first == 'e' && second == 0x0301,
          "e acute decomposes");
    CHECK(muniDecomposePair(0xAC01, &first, &second, &count) == muni_success && count == 2 &&
              first == 0xAC00 && second == 0x11A8,
          "a Hangul LVT syllable splits into LV and T");
    CHECK(muniDecomposePair(0x212B, &first, &second, &count) == muni_success && count == 1 &&
              first == 0x00C5 && second == 0,
          "the angstrom sign is a singleton");
    CHECK(muniDecomposePair('a', &first, &second, &count) == muni_success && count == 0 &&
              first == 0 && second == 0,
          "a does not decompose");
    count = 9;
    CHECK(muniDecomposePair(0x00E9, &first, nullptr, &count) == muni_errorInvalid && count == 9,
          "a NULL output");
    CHECK(muniDecomposePair(0x00E9, &first, &second, nullptr) == muni_errorInvalid, "no count");
    CHECK(muniComposePair('e', 0x0301) == 0x00E9, "e and acute compose");
    CHECK(muniComposePair(0x1100, 0x1161) == 0xAC00, "Hangul L and V compose");
    CHECK(muniComposePair(0x0915, 0x093C) == 0, "U+0958 is excluded from composition");
    for (int i = 0; i < 2; i++)
    {
        uint32_t past = i == 0 ? 0x110000 : UINT32_MAX;
        first = 1;
        second = 1;
        count = 9;
        CHECK(muniDecomposePair(past, &first, &second, &count) == muni_success && count == 0 &&
                  first == 0 && second == 0,
              "past U+10FFFF does not decompose");
        CHECK(muniComposePair(past, 0x0301) == 0 && muniComposePair('e', past) == 0 &&
                  muniComposePair(0x1100, past) == 0 && muniComposePair(0xAC00, past) == 0 &&
                  muniComposePair(past, 0x1161) == 0,
              "past U+10FFFF composes with nothing, Hangul included");
    }
}

// An output that overlaps the text, on either side, is refused and
// nothing is written. The text is 8 bytes and so is its normal form, and
// the output starts 0, 1 or 7 bytes after the text or 1 or 7 bytes before
// it. Right beside the text, 8 bytes after or before it, it is apart.
static void TestOverlap(void)
{
    enum
    {
        size = 8
    };
    static const char input[size + 1] = "Abcdefgh";
    static const ptrdiff_t shifts[] = {0, 1, size - 1, -1, -(size - 1)};
    char buffer[3 * size];
    char* text = buffer + size;
    bool refused = true;
    for (size_t i = 0; i < sizeof(shifts) / sizeof(shifts[0]); i++)
    {
        memset(buffer, '#', sizeof(buffer));
        memcpy(text, input, size);
        char before[sizeof(buffer)];
        memcpy(before, buffer, sizeof(buffer));
        size_t needed = 99;
        muniTextResult result = muniNormalize(text, size, muni_nfc, muni_convertStrict,
                                              text + shifts[i], size, &needed);
        refused = refused && result.status == muni_errorInvalid && needed == 99 &&
                  memcmp(buffer, before, sizeof(buffer)) == 0;
    }
    CHECK(refused, "an output that overlaps the text refused, nothing written");
    bool apart = true;
    for (int side = 0; side < 2; side++)
    {
        memset(buffer, '#', sizeof(buffer));
        memcpy(text, input, size);
        size_t needed = 0;
        char* output = side == 0 ? text + size : text - size;
        muniTextResult result =
            muniNormalize(text, size, muni_nfc, muni_convertStrict, output, size, &needed);
        apart = apart && result.status == muni_success && needed == size &&
                memcmp(output, input, size) == 0 && memcmp(text, input, size) == 0;
    }
    CHECK(apart, "an output right beside the text, on either side, written");
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
    // U+0340 is NFC_Quick_Check=No, and U+0316 below U+0301 is out of
    // order; either settles the answer, and the fault after it still counts.
    result = muniCheckNormalization("a\xCD\x80"
                                    "b\xFF",
                                    5, muni_nfc, &answer);
    CHECK(result.status == muni_errorUtf8Lead && result.offset == 4 && answer == muni_quickCheckYes,
          "a fault after a No code point");
    result = muniCheckNormalization("a\xCC\x81\xCC\x96\xFF", 6, muni_nfd, &answer);
    CHECK(result.status == muni_errorUtf8Lead && result.offset == 5 && answer == muni_quickCheckYes,
          "a fault after marks out of order");
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
        if (muniEncodeUtf8(codePoint, text, sizeof(text), &size) != muni_success)
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

// The pairs of every code point against UnicodeData.txt and
// DerivedNormalizationProps.txt, read here without the generator: each
// canonical decomposition of one or two code points is what
// muniDecomposePair gives, a pair composes back unless the code point
// has Full_Composition_Exclusion, and a code point without one does not
// decompose. Hangul syllables split as Unicode's arithmetic says.
static void TestPairsAgainstTheUcd(void)
{
    static uint8_t excluded[0x110000];
    static uint8_t decomposes[0x110000];
    memset(excluded, 0, sizeof(excluded));
    memset(decomposes, 0, sizeof(decomposes));
    bool read =
        UcdFillBinary("DerivedNormalizationProps.txt", "Full_Composition_Exclusion", excluded);
    FILE* file = UcdOpen("UnicodeData.txt");
    CHECK(read && file != nullptr, "UnicodeData.txt and DerivedNormalizationProps.txt read");
    if (file == nullptr)
    {
        return;
    }
    static char buffer[4096];
    UcdLine line;
    int failures = 0;
    while (UcdNext(file, buffer, sizeof(buffer), &line))
    {
        const char* mapping = line.fields[4];
        if (mapping[0] == '\0' || mapping[0] == '<')
        {
            continue;
        }
        char* end = nullptr;
        uint32_t want[2] = {(uint32_t)strtoul(mapping, &end, 16), 0};
        size_t wantCount = 1;
        if (*end != '\0')
        {
            want[1] = (uint32_t)strtoul(end, nullptr, 16);
            wantCount = 2;
        }
        decomposes[line.first] = 1;
        uint32_t first = 0;
        uint32_t second = 0;
        size_t count = 0;
        bool same = muniDecomposePair(line.first, &first, &second, &count) == muni_success &&
                    count == wantCount && first == want[0] && second == want[1];
        uint32_t composite = wantCount == 2 && excluded[line.first] == 0 ? line.first : 0;
        same = same && (wantCount == 1 || muniComposePair(want[0], want[1]) == composite);
        if (!same && failures++ < 5)
        {
            printf("the pair of U+%04X differs\n", (unsigned)line.first);
        }
    }
    fclose(file);
    for (uint32_t c = 0; c < 0x110000; c++)
    {
        uint32_t want[2] = {0, 0};
        size_t wantCount = 0;
        if (c >= 0xAC00 && c <= 0xD7A3)
        {
            uint32_t index = c - 0xAC00;
            want[0] = index % 28 != 0 ? c - index % 28 : 0x1100 + index / 588;
            want[1] = index % 28 != 0 ? 0x11A7 + index % 28 : 0x1161 + index % 588 / 28;
            wantCount = 2;
        }
        uint32_t first = 1;
        uint32_t second = 1;
        size_t count = 9;
        bool same =
            decomposes[c] != 0 || (muniDecomposePair(c, &first, &second, &count) == muni_success &&
                                   count == wantCount && first == want[0] && second == want[1] &&
                                   (wantCount == 0 || muniComposePair(want[0], want[1]) == c));
        if (!same && failures++ < 5)
        {
            printf("the pair of U+%04X differs\n", (unsigned)c);
        }
    }
    CHECK(failures == 0, "the pairs of every code point match the UCD");
}

#endif

// A kind is an int32_t, so a caller can pass a negative one; every call
// that takes a kind refuses -1 and INT32_MIN as it refuses a kind past the
// last, and leaves its outputs as they were.
static const int32_t s_negative[] = {-1, INT32_MIN};

static void TestNegativeKinds(void)
{
    bool refused = true;
    for (size_t i = 0; i < sizeof(s_negative) / sizeof(s_negative[0]); i++)
    {
        char output[4] = {7, 7, 7, 7};
        size_t needed = 99;
        muniQuickCheck answer = muni_quickCheckMaybe;
        refused =
            refused &&
            muniNormalize("a", 1, s_negative[i], muni_convertStrict, output, 4, &needed).status ==
                muni_errorInvalid &&
            muniNormalize("a", 1, muni_nfc, s_negative[i], output, 4, &needed).status ==
                muni_errorInvalid &&
            muniCheckNormalization("a", 1, s_negative[i], &answer).status == muni_errorInvalid &&
            needed == 99 && output[0] == 7 && answer == muni_quickCheckMaybe;
    }
    CHECK(refused, "a negative form or mode refused");
}

int main(void)
{
    TestCheckArguments();
    TestConformance();
    TestLimitsAndErrors();
    TestEdges();
    TestOverlap();
    TestNegativeKinds();
#ifdef MUNI_UCD_DATA
    TestQuickCheckEverywhere();
    TestPairsAgainstTheUcd();
#endif
    return s_failures == 0 ? 0 : 1;
}
