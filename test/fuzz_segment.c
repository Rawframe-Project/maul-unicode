// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for segmentation and line breaking. The first input
// byte picks the kind and a piece size; the rest is the text. Feeding the text in pieces must
// give exactly the boundaries the whole text gives, and those must rise
// strictly, fall on code point starts and end at the length of the text.
// Word and sentence iterators are also run with class overrides for some
// of the text's own code points, each given a class the input picks, and
// line iterators at the strictness and writing system the input picks;
// strict breaks must be a subset of normal ones, and anywhere must break
// at every grapheme cluster boundary but one just before a hard line
// break that does not follow one. Line breaks must be mandatory
// exactly at the end and after BK, CR, LF and NL, and muniSegmentIterator_NextLineBreak fed
// in pieces must give the breaks and flags muniFindLineBreaks gives.
// Word and sentence boundaries need not be grapheme boundaries: the
// UAX #29 rules put a word boundary after U+0600 (Numeric, but Prepend
// to grapheme clusters) when punctuation follows, for example.

#include "maul-unicode/encoding.h"
#include "maul-unicode/properties.h"
#include "maul-unicode/segment.h"

#include <stdlib.h>
#include <string.h>

#define MAX_BREAKS 4096

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

// Whether offset starts a code point, stepping by maximal subparts.
static bool StartsCodePoint(const char* text, size_t length, size_t offset)
{
    size_t position = 0;
    while (position < offset)
    {
        uint32_t codePoint;
        size_t size;
        (void)muniDecodeUtf8(text + position, length - position, &codePoint, &size);
        position += size;
    }
    return position == offset;
}

typedef muniResult (*InitFn)(muniSegmentIterator*, const char*, size_t, bool);
typedef muniResult (*FindFn)(const char*, size_t, size_t*, size_t, size_t*);

static muniResult FindLines(const char* text, size_t length, size_t* offsets, size_t capacity,
                            size_t* countOut)
{
    return muniFindLineBreaks(text, length, offsets, nullptr, capacity, countOut);
}

// A line iterator's settings; Chunked leaves the defaults for NULL.
typedef struct LineSettings
{
    muniLineBreakStrictness strictness;
    muniWritingSystem writingSystem;
} LineSettings;

static size_t Chunked(InitFn init, const muniBreakClassOverride* overrides, size_t overrideCount,
                      const LineSettings* line, const char* text, size_t length, size_t piece,
                      size_t* breaks)
{
    size_t fed = piece < length ? piece : length;
    muniSegmentIterator iterator;
    Require(init(&iterator, text, fed, fed < length) == muni_success);
    if (overrideCount > 0)
    {
        Require(muniSegmentIterator_SetBreakClassOverrides(&iterator, overrides, overrideCount) ==
                muni_success);
    }
    if (line != nullptr)
    {
        Require(muniSegmentIterator_SetLineBreakStrictness(&iterator, line->strictness) ==
                    muni_success &&
                muniSegmentIterator_SetWritingSystem(&iterator, line->writingSystem) ==
                    muni_success);
    }
    size_t count = 0;
    for (;;)
    {
        size_t offset;
        muniResult status = muniSegmentIterator_NextBreak(&iterator, &offset);
        if (status == muni_success)
        {
            Require(count < MAX_BREAKS);
            breaks[count++] = offset;
        }
        else if (status == muni_needMoreText)
        {
            Require(fed < length);
            size_t next = length - fed < piece ? length - fed : piece;
            Require(muniSegmentIterator_Feed(&iterator, text + fed, next, fed + next < length) ==
                    muni_success);
            fed += next;
        }
        else
        {
            Require(status == muni_done);
            return count;
        }
    }
}

// A breaker that answers anything, in or out of range, the same way for
// the same run and offset.
static size_t WildBreaker(void* context, const char* run, size_t length, size_t from)
{
    (void)context;
    uint8_t byte = from < length ? (uint8_t)run[from] : 0;
    return from + (size_t)(byte % 11) - 2;
}

// With a breaker, over the whole text: boundaries still rise, fall on
// code point starts and end at the length.
static void CheckBreaker(InitFn init, const char* text, size_t length)
{
    muniSegmentIterator iterator;
    Require(init(&iterator, text, length, false) == muni_success);
    Require(muniSegmentIterator_SetComplexBreaker(&iterator, WildBreaker, nullptr) == muni_success);
    size_t previous = 0;
    size_t offset = 0;
    muniResult status;
    while ((status = muniSegmentIterator_NextBreak(&iterator, &offset)) == muni_success)
    {
        Require(offset > previous && StartsCodePoint(text, length, offset));
        previous = offset;
    }
    Require(status == muni_done && previous == length);
}

#define MAX_OVERRIDES 8

// Up to MAX_OVERRIDES of the text's distinct code points in increasing
// order, each with a class below classCount picked from the code point and
// salt.
static size_t BuildOverrides(const char* text, size_t length, uint8_t classCount, uint8_t salt,
                             muniBreakClassOverride* overrides)
{
    size_t count = 0;
    size_t position = 0;
    while (position < length && count < MAX_OVERRIDES)
    {
        uint32_t codePoint;
        size_t step;
        (void)muniDecodeUtf8(text + position, length - position, &codePoint, &step);
        position += step;
        size_t at = 0;
        while (at < count && overrides[at].codePoint < codePoint)
        {
            at++;
        }
        if (at < count && overrides[at].codePoint == codePoint)
        {
            continue;
        }
        memmove(overrides + at + 1, overrides + at, (count - at) * sizeof(*overrides));
        overrides[at].codePoint = codePoint;
        overrides[at].value = (uint8_t)((codePoint * 7u + salt) % classCount);
        count++;
    }
    return count;
}

// Whether the code point at offset is a hard line break, and the one
// before it is not: the anywhere level keeps a line together there
// (LB6), but a break after a hard line break stays mandatory (LB4, LB5).
static bool BeforeHardBreak(const char* text, size_t length, size_t offset)
{
    static const char* const hard[] = {"\r",       "\n",           "\v",          "\f",
                                       "\xC2\x85", "\xE2\x80\xA8", "\xE2\x80\xA9"};
    bool before = false;
    bool after = false;
    for (size_t i = 0; i < sizeof(hard) / sizeof(hard[0]); i++)
    {
        size_t size = strlen(hard[i]);
        before = before || (offset + size <= length && memcmp(text + offset, hard[i], size) == 0);
        after = after || (offset >= size && memcmp(text + offset - size, hard[i], size) == 0);
    }
    return before && !after;
}

// At the strictness and writing system the input picks: the line breaks
// fed in pieces equal the whole text's, rise, fall on code point starts
// and end at the length. Strict breaks are among the normal ones, and
// anywhere breaks are the grapheme cluster boundaries but those just
// before a hard line break that does not follow one.
static void CheckLineSettings(const char* text, size_t length, size_t piece, uint8_t pick)
{
    static size_t whole[MAX_BREAKS];
    static size_t pieces[MAX_BREAKS];
    static size_t other[MAX_BREAKS];
    LineSettings line = {pick >> 6, pick >> 3 & 1};
    size_t count = Chunked(muniInitLineIterator, nullptr, 0, &line, text, length, length, whole);
    Require(length == 0 ? count == 0 : count > 0 && whole[count - 1] == length);
    for (size_t i = 0; i < count; i++)
    {
        Require(i == 0 ? whole[i] > 0 : whole[i] > whole[i - 1]);
        Require(StartsCodePoint(text, length, whole[i]));
    }
    Require(Chunked(muniInitLineIterator, nullptr, 0, &line, text, length, piece, pieces) == count);
    Require(memcmp(whole, pieces, count * sizeof(size_t)) == 0);

    LineSettings strict = {muni_lineBreakStrict, line.writingSystem};
    LineSettings normal = {muni_lineBreakNormal, line.writingSystem};
    size_t strictCount =
        Chunked(muniInitLineIterator, nullptr, 0, &strict, text, length, length, other);
    size_t normalCount =
        Chunked(muniInitLineIterator, nullptr, 0, &normal, text, length, length, pieces);
    size_t k = 0;
    for (size_t i = 0; i < strictCount; i++)
    {
        while (k < normalCount && pieces[k] < other[i])
        {
            k++;
        }
        Require(k < normalCount && pieces[k] == other[i]);
    }

    if (line.strictness == muni_lineBreakAnywhere)
    {
        size_t graphemeCount = 0;
        Require(muniFindGraphemeBreaks(text, length, other, MAX_BREAKS, &graphemeCount) ==
                muni_success);
        size_t kept = 0;
        for (size_t i = 0; i < graphemeCount; i++)
        {
            if (!BeforeHardBreak(text, length, other[i]))
            {
                Require(kept < count && whole[kept] == other[i]);
                kept++;
            }
        }
        Require(kept == count);
    }
}

// The mandatory flags follow segment.h's rule, worked out here from the
// classes alone: a break is mandatory exactly at the end of the text and
// after a BK, CR, LF or NL (UAX #14 LB3 to LB5), and each of those ends a
// break. A CR before an LF ends none, since the pair breaks only after the
// LF. An ill-formed sequence has no class of its own and forces no break.
static void CheckMandatoryRule(const char* text, size_t length, const size_t* breaks,
                               const bool* mandatory, size_t count)
{
    size_t index = 0;
    size_t offset = 0;
    while (offset < length)
    {
        uint32_t codePoint;
        size_t size;
        bool valid =
            muniDecodeUtf8(text + offset, length - offset, &codePoint, &size) == muni_success;
        offset += size;
        muniLineBreak value = valid ? muniGetLineBreak(codePoint) : muni_lbXx;
        bool crBeforeLf = value == muni_lbCr && offset < length && text[offset] == '\n';
        bool hard = !crBeforeLf && (value == muni_lbBk || value == muni_lbCr ||
                                    value == muni_lbLf || value == muni_lbNl);
        bool isBreak = index < count && breaks[index] == offset;
        Require(!crBeforeLf || !isBreak);
        if (hard || offset == length)
        {
            Require(isBreak && mandatory[index]);
        }
        else if (isBreak)
        {
            Require(!mandatory[index]);
        }
        index += isBreak ? 1 : 0;
    }
    Require(index == count);
}

// The line breaks and their mandatory flags muniFindLineBreaks gives for
// the whole text follow the rule above, and equal those muniSegmentIterator_NextLineBreak
// gives fed in pieces.
static void CheckMandatory(const char* text, size_t length, size_t piece)
{
    static size_t whole[MAX_BREAKS];
    static bool wholeMandatory[MAX_BREAKS];
    size_t count = 0;
    Require(muniFindLineBreaks(text, length, whole, wholeMandatory, MAX_BREAKS, &count) ==
            muni_success);
    CheckMandatoryRule(text, length, whole, wholeMandatory, count);
    size_t fed = piece < length ? piece : length;
    muniSegmentIterator iterator;
    Require(muniInitLineIterator(&iterator, text, fed, fed < length) == muni_success);
    size_t index = 0;
    for (;;)
    {
        size_t offset;
        bool mandatory = !wholeMandatory[index < count ? index : 0];
        muniResult status = muniSegmentIterator_NextLineBreak(&iterator, &offset, &mandatory);
        if (status == muni_success)
        {
            Require(index < count && offset == whole[index] && mandatory == wholeMandatory[index]);
            index++;
        }
        else if (status == muni_needMoreText)
        {
            Require(fed < length && !mandatory);
            size_t next = length - fed < piece ? length - fed : piece;
            Require(muniSegmentIterator_Feed(&iterator, text + fed, next, fed + next < length) ==
                    muni_success);
            fed += next;
        }
        else
        {
            Require(status == muni_done && index == count && !mandatory);
            return;
        }
    }
}

// With class overrides: the boundaries fed in pieces equal the whole
// text's, rise, fall on code point starts and end at the length.
static void CheckOverrides(InitFn init, uint8_t classCount, uint8_t salt, const char* text,
                           size_t length, size_t piece)
{
    static size_t whole[MAX_BREAKS];
    static size_t pieces[MAX_BREAKS];
    muniBreakClassOverride overrides[MAX_OVERRIDES];
    size_t overrideCount = BuildOverrides(text, length, classCount, salt, overrides);
    if (overrideCount == 0)
    {
        return;
    }
    size_t count = Chunked(init, overrides, overrideCount, nullptr, text, length, length, whole);
    Require(length == 0 ? count == 0 : count > 0 && whole[count - 1] == length);
    for (size_t i = 0; i < count; i++)
    {
        Require(i == 0 ? whole[i] > 0 : whole[i] > whole[i - 1]);
        Require(StartsCodePoint(text, length, whole[i]));
    }
    Require(Chunked(init, overrides, overrideCount, nullptr, text, length, piece, pieces) == count);
    Require(memcmp(whole, pieces, count * sizeof(size_t)) == 0);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    static const InitFn inits[4] = {muniInitGraphemeIterator, muniInitWordIterator,
                                    muniInitSentenceIterator, muniInitLineIterator};
    static const FindFn finds[4] = {muniFindGraphemeBreaks, muniFindWordBreaks,
                                    muniFindSentenceBreaks, FindLines};
    if (size == 0 || size > MAX_BREAKS)
    {
        return 0;
    }
    // Bits 4 and 5 pick the kind and bits 0 to 2 the piece size; for a
    // line iterator, bits 6 and 7 pick the strictness and bit 3 the
    // writing system.
    size_t kind = (size_t)(data[0] >> 4) % 4;
    size_t piece = (size_t)(data[0] % 8) + 1;
    const char* text = (const char*)data + 1;
    size_t length = size - 1;
    static size_t whole[MAX_BREAKS];
    static size_t pieces[MAX_BREAKS];
    size_t count = 0;
    Require(finds[kind](text, length, whole, MAX_BREAKS, &count) == muni_success);
    Require(length == 0 ? count == 0 : count > 0 && whole[count - 1] == length);
    for (size_t i = 0; i < count; i++)
    {
        Require(i == 0 ? whole[i] > 0 : whole[i] > whole[i - 1]);
        Require(StartsCodePoint(text, length, whole[i]));
    }
    Require(Chunked(inits[kind], nullptr, 0, nullptr, text, length, piece, pieces) == count);
    Require(memcmp(whole, pieces, count * sizeof(size_t)) == 0);
    if (kind == 1 || kind == 3)
    {
        CheckBreaker(inits[kind], text, length);
    }
    if (kind == 3)
    {
        CheckLineSettings(text, length, piece, data[0]);
        CheckMandatory(text, length, piece);
    }
    if (kind == 1 || kind == 2)
    {
        uint8_t classCount = kind == 1 ? muni_wbWSegSpace + 1 : muni_sbClose + 1;
        CheckOverrides(inits[kind], classCount, data[0], text, length, piece);
    }
    return 0;
}
