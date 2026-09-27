// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Case mapping and folding (Unicode chapter 3.13). The simple mappings
// come from each code point's record; the full ones from the special
// table when the record flags one. Two kinds of rule need context and
// live here: Final_Sigma, and the Turkic rules for I, İ and the dot above
// (SpecialCasing.txt, tr and az). Titlecasing walks the text's word
// boundaries (UAX #29).

#include "maul-unicode/case.h"

#include "case_map.h"
#include "encoding.h"
#include "tables.h"
#include "writer.h"

#include "maul-unicode/segment.h"

#define CAPITAL_I         0x0049u
#define SMALL_I           0x0069u
#define CAPITAL_I_DOT     0x0130u
#define SMALL_DOTLESS_I   0x0131u
#define COMBINING_DOT     0x0307u
#define CAPITAL_SIGMA     0x03A3u
#define SMALL_SIGMA       0x03C3u
#define SMALL_FINAL_SIGMA 0x03C2u

static uint32_t Simple(uint32_t codePoint, int kind)
{
    return (uint32_t)((int32_t)codePoint + muniCaseDeltas[muniCaseRecord(codePoint)[kind]]);
}

uint32_t muniToLower(uint32_t codePoint)
{
    return Simple(codePoint, muni_caseKindLower);
}

uint32_t muniToUpper(uint32_t codePoint)
{
    return Simple(codePoint, muni_caseKindUpper);
}

uint32_t muniToTitle(uint32_t codePoint)
{
    return Simple(codePoint, muni_caseKindTitle);
}

uint32_t muniFoldCase(uint32_t codePoint)
{
    return Simple(codePoint, muni_caseKindFold);
}

bool muniIsCased(uint32_t codePoint)
{
    return (muniCaseRecord(codePoint)[4] & muni_caseFlagCased) != 0;
}

// Writes the full mapping of a code point that needs no context.
static void PutMapping(muniWriter* writer, uint32_t codePoint, int kind)
{
    uint32_t mapping[MUNI_MAX_CASE_MAPPING];
    size_t count = muniMapCaseFully(codePoint, kind, mapping);
    for (size_t i = 0; i < count; i++)
    {
        muniWriterPut(writer, mapping[i]);
    }
}

// The text being converted and the context the conditional rules track.
typedef struct Converter
{
    const uint8_t* text;
    size_t length;
    bool turkic;
    bool casedBefore; // a cased letter, then only case-ignorables (Final_Sigma)
    bool afterI;      // an I, then nothing of class 0 or 230 (After_I)
    muniWriter writer;
} Converter;

static uint32_t Decode(const Converter* converter, size_t offset, size_t* sizeOut)
{
    uint32_t codePoint;
    (void)muniStepUtf8(converter->text + offset, converter->length - offset, &codePoint, sizeOut);
    return codePoint;
}

// Final_Sigma after the sigma: no cased letter follows past case-ignorables.
static bool EndsWord(const Converter* converter, size_t offset)
{
    while (offset < converter->length)
    {
        size_t size;
        uint8_t flags = muniCaseRecord(Decode(converter, offset, &size))[4];
        if ((flags & muni_caseFlagCased) != 0)
        {
            return false;
        }
        if ((flags & muni_caseFlagIgnorable) == 0)
        {
            return true;
        }
        offset += size;
    }
    return true;
}

// Before_Dot: a U+0307 follows, past marks of classes other than 0 and 230.
static bool BeforeDot(const Converter* converter, size_t offset)
{
    while (offset < converter->length)
    {
        size_t size;
        uint32_t codePoint = Decode(converter, offset, &size);
        if (codePoint == COMBINING_DOT)
        {
            return true;
        }
        uint8_t markClass = muniLookupCombiningClass(codePoint);
        if (markClass == 0 || markClass == 230)
        {
            return false;
        }
        offset += size;
    }
    return false;
}

// Lowercases one code point at offset, whose encoding ends at next.
static void PutLower(Converter* converter, uint32_t codePoint, size_t next)
{
    if (codePoint == CAPITAL_SIGMA)
    {
        bool final = converter->casedBefore && EndsWord(converter, next);
        muniWriterPut(&converter->writer, final ? SMALL_FINAL_SIGMA : SMALL_SIGMA);
        return;
    }
    if (converter->turkic)
    {
        if (codePoint == CAPITAL_I_DOT)
        {
            muniWriterPut(&converter->writer, SMALL_I);
            return;
        }
        if (codePoint == CAPITAL_I)
        {
            muniWriterPut(&converter->writer,
                          BeforeDot(converter, next) ? SMALL_I : SMALL_DOTLESS_I);
            return;
        }
        if (codePoint == COMBINING_DOT && converter->afterI)
        {
            return; // the dot of an I, which lowercasing made an i
        }
    }
    PutMapping(&converter->writer, codePoint, muni_caseKindLower);
}

// Uppercases or titlecases one code point.
static void PutUpper(Converter* converter, uint32_t codePoint, int kind)
{
    if (converter->turkic && codePoint == SMALL_I)
    {
        muniWriterPut(&converter->writer, CAPITAL_I_DOT);
        return;
    }
    PutMapping(&converter->writer, codePoint, kind);
}

static void PutFold(Converter* converter, uint32_t codePoint)
{
    if (converter->turkic && (codePoint == CAPITAL_I || codePoint == CAPITAL_I_DOT))
    {
        muniWriterPut(&converter->writer, codePoint == CAPITAL_I ? SMALL_DOTLESS_I : SMALL_I);
        return;
    }
    PutMapping(&converter->writer, codePoint, muni_caseKindFold);
}

// Moves the context past a code point.
static void Track(Converter* converter, uint32_t codePoint)
{
    uint8_t flags = muniCaseRecord(codePoint)[4];
    if ((flags & muni_caseFlagCased) != 0)
    {
        converter->casedBefore = true;
    }
    else if ((flags & muni_caseFlagIgnorable) == 0)
    {
        converter->casedBefore = false;
    }
    uint8_t markClass = codePoint < 0x300 ? 0 : muniLookupCombiningClass(codePoint);
    if (codePoint == CAPITAL_I)
    {
        converter->afterI = true;
    }
    else if (markClass == 0 || markClass == 230)
    {
        converter->afterI = false;
    }
}

// Converts text[start, end): with titleFirst, the first cased code point
// is titlecased and the rest lowercased. Returns the offset of the first
// ill-formed sequence in strict mode, or end.
static size_t Convert(Converter* converter, size_t start, size_t end, muniCaseOperation operation,
                      bool strict, muniResult* statusOut)
{
    bool titleDone = false;
    size_t offset = start;
    while (offset < end)
    {
        uint32_t codePoint;
        size_t size;
        muniResult status =
            muniStepUtf8(converter->text + offset, converter->length - offset, &codePoint, &size);
        if (status != muni_success && strict)
        {
            *statusOut = status;
            return offset;
        }
        if (operation == muni_caseUpper)
        {
            PutUpper(converter, codePoint, muni_caseKindUpper);
        }
        else if (operation == muni_caseFold)
        {
            PutFold(converter, codePoint);
        }
        else if (operation == muni_caseTitle && !titleDone && muniIsCased(codePoint))
        {
            PutUpper(converter, codePoint, muni_caseKindTitle);
            titleDone = true;
        }
        else
        {
            PutLower(converter, codePoint, offset + size);
        }
        Track(converter, codePoint);
        offset += size;
    }
    return end;
}

muniTextResult muniConvertCase(const char* text, size_t length, muniCaseOperation operation,
                               muniCaseLanguage language, muniConvertMode mode, char* output,
                               size_t capacity, size_t* neededOut)
{
    if ((text == nullptr && length != 0) || (output == nullptr && capacity != 0) ||
        neededOut == nullptr || operation > muni_caseFold || language > muni_caseTurkic ||
        (mode != muni_convertStrict && mode != muni_convertReplace))
    {
        return (muniTextResult){muni_errorInvalid, 0};
    }
    Converter converter = {(const uint8_t*)text,
                           length,
                           language == muni_caseTurkic,
                           false,
                           false,
                           (muniWriter){output, capacity, 0}};
    bool strict = mode == muni_convertStrict;
    muniResult status = muni_success;
    size_t stop = length;
    if (operation != muni_caseTitle)
    {
        stop = Convert(&converter, 0, length, operation, strict, &status);
    }
    else
    {
        // Each word, from one boundary to the next.
        muniSegmentIterator words;
        size_t start = 0;
        size_t end = 0;
        bool started = muniInitWordIterator(&words, text, length, false) == muni_success;
        while (started && status == muni_success &&
               muniNextSegmentBreak(&words, &end) == muni_success)
        {
            stop = Convert(&converter, start, end, operation, strict, &status);
            start = end;
        }
    }
    *neededOut = converter.writer.needed;
    if (status != muni_success)
    {
        return (muniTextResult){status, stop};
    }
    return (muniTextResult){converter.writer.needed > capacity ? muni_errorCapacity : muni_success,
                            length};
}
