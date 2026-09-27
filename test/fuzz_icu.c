// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target that compares the library with ICU, a test-only
// oracle never linked into the library. Every input is converted to
// UTF-16 with U+FFFD for each maximal ill-formed subpart, which both
// follow the Unicode Standard on. Well-formed input whose code points
// ICU's Unicode version assigns is also normalized to the four forms,
// which the normalization stability policy keeps the same across
// versions, and lowercased, uppercased and folded, by default and
// Turkic rules, unless a mapping reaches a character added since.
//
// Such input is also segmented into grapheme clusters, words, sentences
// and lines, and laid out by bidi, when no character in it has a
// property those depend on that changed since ICU's version. The
// changes are learned once: each of our values maps to the ICU value
// most characters with it have, and a character whose ICU value is
// another has changed, as has one whose value splits one of ICU's (all
// but the largest part). Some rules changed too, and ICU tailors some:
//
// - clusters skip InCB linkers, as GB9c no longer needs a consonant
//   before one;
// - words skip Han, Kana and class SA, which ICU segments with
//   dictionaries, Hangul, whose syllables it keeps apart, and the
//   colons, which it does not count as MidLetter;
// - lines skip class SA; class GL, which LB12a now keeps after BA;
//   the hyphens and class HL of LB20a, whose context changed and which
//   ICU misses after an opening bracket and spaces; and quotation marks,
//   which LB15a to LB19a treat differently since;
// - bidi is compared as it displays, paragraph by paragraph, since ICU
//   takes shortcuts that change levels but not the display, and not in
//   text with more than 63 opening brackets, past which BD16 stops
//   pairing and ICU does not, nor where a nonspacing mark follows a
//   bracket: ICU 74 leaves such a mark out of rule N0, against
//   BidiCharacterTest.txt.
//
// A difference prints the operation and the input, then aborts, so
// libFuzzer keeps the input.

#include "maul-unicode/bidi.h"
#include "maul-unicode/case.h"
#include "maul-unicode/normalize.h"
#include "maul-unicode/properties.h"
#include "maul-unicode/segment.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <unicode/uchar.h>
#include <unicode/unorm2.h>
#include <unicode/ustring.h>

#define MAX_INPUT 512
#define MAX_UNITS (MAX_INPUT * 18 * 3)
#define MAX_BYTES (MAX_UNITS * 3)

static void Differ(const char* operation, const uint8_t* data, size_t size)
{
    fprintf(stderr, "%s differs from ICU for:", operation);
    for (size_t i = 0; i < size; i++)
    {
        fprintf(stderr, " %02X", data[i]);
    }
    fprintf(stderr, "\n");
    abort();
}

// ICU's result in UTF-16 as UTF-8, by ICU; false when it failed.
static bool IcuToUtf8(const UChar* units, int32_t length, char* bytes, int32_t* lengthOut)
{
    UErrorCode error = U_ZERO_ERROR;
    u_strToUTF8(bytes, MAX_BYTES, lengthOut, units, length, &error);
    return U_SUCCESS(error);
}

static bool Same(const char* ours, size_t ourLength, const char* theirs, int32_t theirLength)
{
    return ourLength == (size_t)theirLength && memcmp(ours, theirs, ourLength) == 0;
}

static void CompareConversion(const char* text, size_t length)
{
    static uint16_t ours[MAX_UNITS];
    static UChar theirs[MAX_UNITS];
    size_t needed = 0;
    muniTextResult result =
        muniConvertUtf8ToUtf16(text, length, ours, MAX_UNITS, muni_convertReplace, &needed);
    UErrorCode error = U_ZERO_ERROR;
    int32_t theirLength = 0;
    u_strFromUTF8WithSub(theirs, MAX_UNITS, &theirLength, text, (int32_t)length, 0xFFFD, nullptr,
                         &error);
    if (result.status != muni_success || U_FAILURE(error) || needed != (size_t)theirLength ||
        memcmp(ours, theirs, needed * sizeof(uint16_t)) != 0)
    {
        Differ("UTF-8 to UTF-16 with replacement", (const uint8_t*)text, length);
    }
}

// Whether ICU's Unicode version assigns every code point of the text.
static bool IcuKnows(const UChar* units, int32_t length)
{
    for (int32_t i = 0; i < length;)
    {
        UChar32 codePoint;
        U16_NEXT(units, i, length, codePoint);
        UVersionInfo age;
        u_charAge(codePoint, age);
        if (age[0] == 0 && age[1] == 0)
        {
            return false;
        }
    }
    return true;
}

static void CompareNormalization(const char* text, size_t length, const UChar* units,
                                 int32_t unitCount)
{
    static char ours[MAX_BYTES];
    static char theirs[MAX_BYTES];
    static UChar normalized[MAX_UNITS];
    static const char* const names[4] = {"NFC", "NFD", "NFKC", "NFKD"};
    for (muniNormalForm form = 0; form < 4; form++)
    {
        size_t needed = 0;
        muniTextResult result =
            muniNormalize(text, length, form, muni_convertStrict, ours, MAX_BYTES, &needed);
        if (result.status == muni_errorLimit)
        {
            return; // more marks than a segment holds, which ICU buffers
        }
        UErrorCode error = U_ZERO_ERROR;
        const UNormalizer2* normalizer = form == muni_nfc    ? unorm2_getNFCInstance(&error)
                                         : form == muni_nfd  ? unorm2_getNFDInstance(&error)
                                         : form == muni_nfkc ? unorm2_getNFKCInstance(&error)
                                                             : unorm2_getNFKDInstance(&error);
        int32_t count =
            unorm2_normalize(normalizer, units, unitCount, normalized, MAX_UNITS, &error);
        int32_t theirLength = 0;
        if (result.status != muni_success || U_FAILURE(error) ||
            !IcuToUtf8(normalized, count, theirs, &theirLength) ||
            !Same(ours, needed, theirs, theirLength))
        {
            Differ(names[form], (const uint8_t*)text, length);
        }
    }
}

// Whether ICU's version knows every code point of UTF-8 output: case
// mappings of old characters to new ones, such as U+019B to U+A7DC in
// Unicode 16, are changes since that version, not differences.
static bool OutputKnown(const char* bytes, size_t length)
{
    static UChar units[MAX_UNITS];
    UErrorCode error = U_ZERO_ERROR;
    int32_t count = 0;
    u_strFromUTF8(units, MAX_UNITS, &count, bytes, (int32_t)length, &error);
    return U_SUCCESS(error) && IcuKnows(units, count);
}

typedef int32_t IcuCaseFn(UChar* dest, int32_t capacity, const UChar* source, int32_t length,
                          const char* locale, UErrorCode* error);

static int32_t FoldDefault(UChar* dest, int32_t capacity, const UChar* source, int32_t length,
                           const char* locale, UErrorCode* error)
{
    (void)locale;
    return u_strFoldCase(dest, capacity, source, length, U_FOLD_CASE_DEFAULT, error);
}

static int32_t FoldTurkic(UChar* dest, int32_t capacity, const UChar* source, int32_t length,
                          const char* locale, UErrorCode* error)
{
    (void)locale;
    return u_strFoldCase(dest, capacity, source, length, U_FOLD_CASE_EXCLUDE_SPECIAL_I, error);
}

static void CompareCase(const char* text, size_t length, const UChar* units, int32_t unitCount)
{
    static char ours[MAX_BYTES];
    static char theirs[MAX_BYTES];
    static UChar mapped[MAX_UNITS];
    static const struct
    {
        const char* name;
        muniCaseOperation operation;
        muniCaseLanguage language;
        IcuCaseFn* icu;
        const char* locale;
    } s_cases[] = {
        {"lowercase", muni_caseLower, muni_caseDefault, u_strToLower, ""},
        {"uppercase", muni_caseUpper, muni_caseDefault, u_strToUpper, ""},
        {"fold", muni_caseFold, muni_caseDefault, FoldDefault, ""},
        {"Turkic lowercase", muni_caseLower, muni_caseTurkic, u_strToLower, "tr"},
        {"Turkic uppercase", muni_caseUpper, muni_caseTurkic, u_strToUpper, "tr"},
        {"Turkic fold", muni_caseFold, muni_caseTurkic, FoldTurkic, "tr"},
    };
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); i++)
    {
        size_t needed = 0;
        muniTextResult result =
            muniConvertCase(text, length, s_cases[i].operation, s_cases[i].language,
                            muni_convertStrict, ours, MAX_BYTES, &needed);
        UErrorCode error = U_ZERO_ERROR;
        int32_t count =
            s_cases[i].icu(mapped, MAX_UNITS, units, unitCount, s_cases[i].locale, &error);
        int32_t theirLength = 0;
        if (result.status == muni_success && !OutputKnown(ours, needed))
        {
            continue; // a mapping added since ICU's version
        }
        if (result.status != muni_success || U_FAILURE(error) ||
            !IcuToUtf8(mapped, count, theirs, &theirLength) ||
            !Same(ours, needed, theirs, theirLength))
        {
            Differ(s_cases[i].name, (const uint8_t*)text, length);
        }
    }
}

// The properties boundaries and bidi read, as bits of s_changed.
enum
{
    ChangedGrapheme = 1,
    ChangedWord = 2,
    ChangedSentence = 4,
    ChangedLine = 8,
    ChangedBidi = 16,
    ChangedWidth = 32,
    ChangedBracket = 64,
    ChangedPictographic = 128,
};

static const UProperty s_properties[7] = {
    UCHAR_GRAPHEME_CLUSTER_BREAK,
    UCHAR_WORD_BREAK,
    UCHAR_SENTENCE_BREAK,
    UCHAR_LINE_BREAK,
    UCHAR_BIDI_CLASS,
    UCHAR_EAST_ASIAN_WIDTH,
    UCHAR_BIDI_PAIRED_BRACKET_TYPE,
};

static int32_t OurValue(int property, uint32_t codePoint)
{
    switch (property)
    {
    case 0:
        return muniGetGraphemeBreak(codePoint);
    case 1:
        return muniGetWordBreak(codePoint);
    case 2:
        return muniGetSentenceBreak(codePoint);
    case 3:
        return muniGetLineBreak(codePoint);
    case 4:
        return muniGetBidiClass(codePoint);
    case 5:
        return muniGetEastAsianWidth(codePoint);
    default:
        return muniGetBracketType(codePoint);
    }
}

static uint8_t s_changed[0x110000];

static bool IcuAssigns(UChar32 codePoint)
{
    UVersionInfo age;
    u_charAge(codePoint, age);
    return age[0] != 0 || age[1] != 0;
}

// Marks the characters whose value of one property changed.
static void LearnProperty(int property)
{
    static uint32_t counts[64][64];
    memset(counts, 0, sizeof(counts));
    for (UChar32 c = 0; c < 0x110000; c++)
    {
        int32_t ours = OurValue(property, (uint32_t)c);
        int32_t theirs = u_getIntPropertyValue(c, s_properties[property]);
        if (IcuAssigns(c) && ours < 64 && theirs >= 0 && theirs < 64)
        {
            counts[ours][theirs] += 1;
        }
    }
    int32_t mapping[64];
    for (int ours = 0; ours < 64; ours++)
    {
        mapping[ours] = 0;
        for (int theirs = 1; theirs < 64; theirs++)
        {
            mapping[ours] =
                counts[ours][theirs] > counts[ours][mapping[ours]] ? theirs : mapping[ours];
        }
    }
    // Where several of our values map to one of ICU's, ours split it, as
    // HH split from BA in Unicode 17: all but the largest are new.
    bool split[64] = {false};
    for (int ours = 0; ours < 64; ours++)
    {
        for (int other = 0; other < 64; other++)
        {
            uint32_t mine = counts[ours][mapping[ours]];
            uint32_t theirs = counts[other][mapping[other]];
            split[ours] = split[ours] || (other != ours && mapping[other] == mapping[ours] &&
                                          (theirs > mine || (theirs == mine && other < ours)));
        }
    }
    for (UChar32 c = 0; c < 0x110000; c++)
    {
        int32_t ours = OurValue(property, (uint32_t)c);
        if (ours >= 64 || split[ours] ||
            mapping[ours] != u_getIntPropertyValue(c, s_properties[property]))
        {
            s_changed[c] |= (uint8_t)(1u << property);
        }
    }
}

static void Learn(void)
{
    for (int property = 0; property < 7; property++)
    {
        LearnProperty(property);
    }
    for (UChar32 c = 0; c < 0x110000; c++)
    {
        if (muniIsExtendedPictographic((uint32_t)c) !=
            (u_hasBinaryProperty(c, UCHAR_EXTENDED_PICTOGRAPHIC) != 0))
        {
            s_changed[c] |= ChangedPictographic;
        }
    }
}

// The code points rule X9 removes, whose levels UAX #9 leaves open.
static bool IsRemoved(muniBidiClass type)
{
    return type == muni_bcBn || (type >= muni_bcLre && type <= muni_bcPdf);
}

// What the text's characters changed, whether it has a script whose
// words ICU tailors or characters of line class SA, and whether it has
// an InCB linker: rule GB9c no longer needs a consonant before one, as
// it did in Unicode 15.1.
typedef struct Survey
{
    uint8_t changed;
    bool dictionary;
    bool complex;
    bool linker;
    bool glue;          // line class GL
    bool hebrew;        // line class HL
    bool hyphen;        // line class HY or HH
    bool quote;         // line class QU
    int32_t openings;   // opening paired brackets
    bool markedBracket; // a paired bracket followed by a nonspacing mark
} Survey;

static Survey SurveyText(const UChar* units, int32_t count)
{
    Survey survey = {0};
    bool afterBracket = false; // a bracket, then nonspacing marks only
    for (int32_t i = 0; i < count;)
    {
        UChar32 c;
        U16_NEXT(units, i, count, c);
        survey.changed |= s_changed[c];
        muniScript script = muniGetScript((uint32_t)c);
        survey.dictionary = survey.dictionary || script == MUNI_SCRIPT('H', 'a', 'n', 'i') ||
                            script == MUNI_SCRIPT('H', 'i', 'r', 'a') ||
                            script == MUNI_SCRIPT('K', 'a', 'n', 'a') ||
                            script == MUNI_SCRIPT('H', 'a', 'n', 'g') || c == ':' || c == 0xFE55 ||
                            c == 0xFF1A || muniGetLineBreak((uint32_t)c) == muni_lbSa;
        survey.complex = survey.complex || muniGetLineBreak((uint32_t)c) == muni_lbSa;
        survey.linker = survey.linker || muniGetIndicConjunctBreak((uint32_t)c) == muni_incbLinker;
        survey.glue = survey.glue || muniGetLineBreak((uint32_t)c) == muni_lbGl;
        survey.hebrew = survey.hebrew || muniGetLineBreak((uint32_t)c) == muni_lbHl;
        survey.hyphen = survey.hyphen || muniGetLineBreak((uint32_t)c) == muni_lbHy ||
                        muniGetLineBreak((uint32_t)c) == muni_lbHh;
        survey.quote = survey.quote || muniGetLineBreak((uint32_t)c) == muni_lbQu;
        survey.openings += muniGetBracketType((uint32_t)c) == muni_bracketOpen ? 1 : 0;
        muniBidiClass type = muniGetBidiClass((uint32_t)c);
        bool mark = type == muni_bcNsm;
        survey.markedBracket = survey.markedBracket || (mark && afterBracket);
        // Code points rule X9 removes leave the sequence as it was.
        afterBracket = IsRemoved(type) ? afterBracket
                                       : muniGetBracketType((uint32_t)c) != muni_bracketNone ||
                                             (mark && afterBracket);
    }
    return survey;
}

// The UTF-16 offset of each UTF-8 offset that starts a code point.
static void MapOffsets(const char* text, size_t length, int32_t* unitAt)
{
    int32_t unit = 0;
    for (size_t offset = 0; offset < length; offset++)
    {
        uint8_t byte = (uint8_t)text[offset];
        unitAt[offset] = unit;
        unit += (byte & 0xC0) == 0x80 ? 0 : byte >= 0xF0 ? 2 : 1;
    }
    unitAt[length] = unit;
}

typedef muniResult FindFn(const char* text, size_t length, size_t* offsets, size_t capacity,
                          size_t* countOut);

static muniResult FindLines(const char* text, size_t length, size_t* offsets, size_t capacity,
                            size_t* countOut)
{
    return muniFindLineBreaks(text, length, offsets, nullptr, capacity, countOut);
}

static void CompareBreaks(const char* name, FindFn* find, UBreakIteratorType type, const char* text,
                          size_t length, const UChar* units, int32_t count, const int32_t* unitAt)
{
    static size_t ours[MAX_INPUT + 1];
    static UBreakIterator* iterators[4];
    size_t ourCount = 0;
    UErrorCode error = U_ZERO_ERROR;
    if (iterators[type] == nullptr)
    {
        iterators[type] = ubrk_open(type, "", nullptr, 0, &error);
    }
    ubrk_setText(iterators[type], units, count, &error);
    bool same = find(text, length, ours, MAX_INPUT + 1, &ourCount) == muni_success &&
                U_SUCCESS(error) && ubrk_first(iterators[type]) == 0;
    size_t k = 0;
    for (int32_t at = ubrk_next(iterators[type]); same && at != UBRK_DONE;
         at = ubrk_next(iterators[type]))
    {
        same = k < ourCount && unitAt[ours[k]] == at;
        k += 1;
    }
    if (!same || k != ourCount)
    {
        Differ(name, (const uint8_t*)text, length);
    }
}

// Our levels after rule L1, one per byte, paragraph by paragraph, and
// where each paragraph ends.
static bool OurLevels(const char* text, size_t length, uint8_t* lineLevels, size_t* ends,
                      size_t* endCount)
{
    static uint8_t levels[MAX_INPUT];
    static uint8_t workspace[MAX_INPUT];
    static muniBidiRun runs[MAX_INPUT];
    size_t start = 0;
    *endCount = 0;
    while (start < length)
    {
        size_t paragraphLength = 0;
        uint8_t paragraphLevel = 0;
        size_t count = 0;
        if (muniResolveBidi(text + start, length - start, muni_bidiAuto, levels, workspace,
                            &paragraphLength, &paragraphLevel) != muni_success ||
            muniReorderBidiLine(text + start, levels, paragraphLength, paragraphLevel, runs,
                                MAX_INPUT, &count) != muni_success)
        {
            return false;
        }
        for (size_t i = 0; i < count; i++)
        {
            memset(lineLevels + start + runs[i].start, runs[i].level, runs[i].length);
        }
        start += paragraphLength;
        ends[(*endCount)++] = start;
    }
    return true;
}

// Whether two paragraphs of levels, for the same code points, display
// alike: each code point on the same side, and in the same visual order.
static bool DisplayAlike(const uint8_t* ours, const uint8_t* theirs, size_t count)
{
    static size_t ourOrder[MAX_INPUT];
    static size_t theirOrder[MAX_INPUT];
    bool alike = muniReorderBidiLevels(ours, count, ourOrder) == muni_success &&
                 muniReorderBidiLevels(theirs, count, theirOrder) == muni_success;
    for (size_t i = 0; alike && i < count; i++)
    {
        alike = (ours[i] & 1) == (theirs[i] & 1) && ourOrder[i] == theirOrder[i];
    }
    return alike;
}

// ICU's levels for one paragraph on its own: across paragraphs, ICU
// gives one without a strong character the level of the one before,
// where rule P3 gives it 0.
static const UBiDiLevel* IcuLevels(const UChar* units, int32_t count)
{
    static UBiDi* bidi;
    UErrorCode error = U_ZERO_ERROR;
    if (bidi == nullptr)
    {
        bidi = ubidi_open();
    }
    ubidi_setPara(bidi, units, count, UBIDI_DEFAULT_LTR, nullptr, &error);
    const UBiDiLevel* levels = ubidi_getLevels(bidi, &error);
    return U_SUCCESS(error) ? levels : nullptr;
}

// Bidi as it displays. ICU takes shortcuts that change levels but not
// the display: text with nothing right to left keeps the paragraph
// level, where rule I1 lifts numbers by two, and text with nothing left
// to right gets one odd level, where an embedding would add two. So each
// paragraph's code points, but those rule X9 removes, must show on the
// same side and in the same order, reordered from either's levels.
static void CompareBidi(const char* text, size_t length, const UChar* units, const int32_t* unitAt)
{
    static uint8_t lineLevels[MAX_INPUT];
    static size_t ends[MAX_INPUT];
    static uint8_t ours[MAX_INPUT];
    static uint8_t theirs[MAX_INPUT];
    size_t endCount = 0;
    bool same = OurLevels(text, length, lineLevels, ends, &endCount);
    size_t offset = 0;
    for (size_t paragraph = 0; same && paragraph < endCount; paragraph++)
    {
        int32_t first = unitAt[offset];
        const UBiDiLevel* icuLevels = IcuLevels(units + first, unitAt[ends[paragraph]] - first);
        size_t visible = 0;
        while (icuLevels != nullptr && offset < ends[paragraph])
        {
            uint32_t codePoint = 0;
            size_t size = 1;
            (void)muniDecodeUtf8(text + offset, length - offset, &codePoint, &size);
            if (!IsRemoved(muniGetBidiClass(codePoint)))
            {
                ours[visible] = lineLevels[offset];
                theirs[visible] = icuLevels[unitAt[offset] - first];
                visible += 1;
            }
            offset += size;
        }
        same = icuLevels != nullptr && DisplayAlike(ours, theirs, visible);
    }
    if (!same)
    {
        Differ("bidi display", (const uint8_t*)text, length);
    }
}

static void CompareLayout(const char* text, size_t length, const UChar* units, int32_t count)
{
    static int32_t unitAt[MAX_INPUT + 1];
    static bool learned;
    if (!learned)
    {
        Learn();
        learned = true;
    }
    MapOffsets(text, length, unitAt);
    Survey survey = SurveyText(units, count);
    if ((survey.changed & (ChangedGrapheme | ChangedPictographic)) == 0 && !survey.linker)
    {
        CompareBreaks("grapheme boundaries", muniFindGraphemeBreaks, UBRK_CHARACTER, text, length,
                      units, count, unitAt);
    }
    if ((survey.changed & (ChangedWord | ChangedPictographic)) == 0 && !survey.dictionary)
    {
        CompareBreaks("word boundaries", muniFindWordBreaks, UBRK_WORD, text, length, units, count,
                      unitAt);
    }
    if ((survey.changed & ChangedSentence) == 0)
    {
        CompareBreaks("sentence boundaries", muniFindSentenceBreaks, UBRK_SENTENCE, text, length,
                      units, count, unitAt);
    }
    if ((survey.changed & (ChangedLine | ChangedWidth | ChangedPictographic)) == 0 &&
        !survey.complex && !survey.glue && !survey.hebrew && !survey.hyphen && !survey.quote)
    {
        CompareBreaks("line breaks", FindLines, UBRK_LINE, text, length, units, count, unitAt);
    }
    if ((survey.changed & (ChangedBidi | ChangedBracket)) == 0 && survey.openings <= 63 &&
        !survey.markedBracket)
    {
        CompareBidi(text, length, units, unitAt);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size > MAX_INPUT)
    {
        return 0;
    }
    const char* text = (const char*)data;
    CompareConversion(text, size);
    static UChar units[MAX_UNITS];
    UErrorCode error = U_ZERO_ERROR;
    int32_t unitCount = 0;
    u_strFromUTF8(units, MAX_UNITS, &unitCount, text, (int32_t)size, &error);
    if (U_FAILURE(error) || !IcuKnows(units, unitCount))
    {
        return 0;
    }
    CompareNormalization(text, size, units, unitCount);
    CompareCase(text, size, units, unitCount);
    if (size > 0)
    {
        CompareLayout(text, size, units, unitCount);
    }
    return 0;
}
