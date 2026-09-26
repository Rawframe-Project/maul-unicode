// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for script runs. The first input byte picks a piece
// size; the rest is the text. Pieces must give the runs the whole text
// gives, and the runs must end in rising order on code point starts, the
// last at the length of the text.

#include "maul-unicode/encoding.h"
#include "maul-unicode/script.h"

#include <stdlib.h>
#include <string.h>

#define MAX_RUNS 4096

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

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

static size_t Chunked(const char* text, size_t length, size_t piece, muniScriptRun* runs)
{
    size_t fed = piece < length ? piece : length;
    muniScriptIterator iterator;
    Require(muniInitScriptIterator(&iterator, text, fed, fed < length) == muni_success);
    size_t count = 0;
    for (;;)
    {
        muniScriptRun run;
        muniResult status = muniNextScriptRun(&iterator, &run);
        if (status == muni_success)
        {
            Require(count < MAX_RUNS);
            runs[count++] = run;
        }
        else if (status == muni_needMoreText)
        {
            size_t next = length - fed < piece ? length - fed : piece;
            Require(muniFeedScriptIterator(&iterator, text + fed, next, fed + next < length) ==
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
    if (size == 0 || size > MAX_RUNS)
    {
        return 0;
    }
    size_t piece = (size_t)(data[0] % 8) + 1;
    const char* text = (const char*)data + 1;
    size_t length = size - 1;
    static muniScriptRun whole[MAX_RUNS];
    static muniScriptRun pieces[MAX_RUNS];
    size_t count = 0;
    Require(muniFindScriptRuns(text, length, whole, MAX_RUNS, &count) == muni_success);
    Require(length == 0 ? count == 0 : count > 0 && whole[count - 1].end == length);
    for (size_t i = 0; i < count; i++)
    {
        Require(i == 0 ? whole[i].end > 0 : whole[i].end > whole[i - 1].end);
        Require(StartsCodePoint(text, length, whole[i].end));
    }
    Require(Chunked(text, length, piece, pieces) == count);
    Require(memcmp(whole, pieces, count * sizeof(muniScriptRun)) == 0);
    return 0;
}
