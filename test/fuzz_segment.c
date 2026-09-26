// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for segmentation. The first input byte picks a piece
// size; the rest is the text. Feeding the text in pieces must give exactly
// the boundaries the whole text gives, and those must rise strictly, fall
// on code point starts and end at the length of the text.

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

static size_t Chunked(const char* text, size_t length, size_t piece, size_t* breaks)
{
    size_t fed = piece < length ? piece : length;
    muniGraphemeIterator iterator;
    Require(muniInitGraphemeIterator(&iterator, text, fed, fed < length) == muni_success);
    size_t count = 0;
    for (;;)
    {
        size_t offset;
        muniResult status = muniNextGraphemeBreak(&iterator, &offset);
        if (status == muni_success)
        {
            Require(count < MAX_BREAKS);
            breaks[count++] = offset;
        }
        else if (status == muni_needMoreText)
        {
            Require(fed < length);
            size_t next = length - fed < piece ? length - fed : piece;
            Require(muniFeedGraphemeIterator(&iterator, text + fed, next, fed + next < length) ==
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

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size == 0 || size > MAX_BREAKS)
    {
        return 0;
    }
    size_t piece = (size_t)(data[0] % 8) + 1;
    const char* text = (const char*)data + 1;
    size_t length = size - 1;
    static size_t whole[MAX_BREAKS];
    static size_t pieces[MAX_BREAKS];
    size_t count = 0;
    Require(muniFindGraphemeBreaks(text, length, whole, MAX_BREAKS, &count) == muni_success);
    Require(length == 0 ? count == 0 : count > 0 && whole[count - 1] == length);
    for (size_t i = 0; i < count; i++)
    {
        Require(i == 0 ? whole[i] > 0 : whole[i] > whole[i - 1]);
        Require(StartsCodePoint(text, length, whole[i]));
    }
    Require(Chunked(text, length, piece, pieces) == count);
    Require(memcmp(whole, pieces, count * sizeof(size_t)) == 0);
    return 0;
}
