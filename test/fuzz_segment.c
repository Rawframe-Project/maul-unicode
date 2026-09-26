// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for segmentation and line breaking. The first input
// byte picks the kind and a piece size; the rest is the text. Feeding the text in pieces must
// give exactly the boundaries the whole text gives, and those must rise
// strictly, fall on code point starts and end at the length of the text.
// Word and sentence boundaries need not be grapheme boundaries: the
// UAX #29 rules put a word boundary after U+0600 (Numeric, but Prepend
// to grapheme clusters) when punctuation follows, for example.

#include "maul-unicode/encoding.h"
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

static size_t Chunked(InitFn init, const char* text, size_t length, size_t piece, size_t* breaks)
{
    size_t fed = piece < length ? piece : length;
    muniSegmentIterator iterator;
    Require(init(&iterator, text, fed, fed < length) == muni_success);
    size_t count = 0;
    for (;;)
    {
        size_t offset;
        muniResult status = muniNextSegmentBreak(&iterator, &offset);
        if (status == muni_success)
        {
            Require(count < MAX_BREAKS);
            breaks[count++] = offset;
        }
        else if (status == muni_needMoreText)
        {
            Require(fed < length);
            size_t next = length - fed < piece ? length - fed : piece;
            Require(muniFeedSegmentIterator(&iterator, text + fed, next, fed + next < length) ==
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
    Require(muniSetComplexBreaker(&iterator, WildBreaker, nullptr) == muni_success);
    size_t previous = 0;
    size_t offset = 0;
    muniResult status;
    while ((status = muniNextSegmentBreak(&iterator, &offset)) == muni_success)
    {
        Require(offset > previous && StartsCodePoint(text, length, offset));
        previous = offset;
    }
    Require(status == muni_done && previous == length);
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
    Require(Chunked(inits[kind], text, length, piece, pieces) == count);
    Require(memcmp(whole, pieces, count * sizeof(size_t)) == 0);
    if (kind == 1 || kind == 3)
    {
        CheckBreaker(inits[kind], text, length);
    }
    return 0;
}
