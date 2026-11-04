// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the state the iterators keep in caller memory. The
// first input byte picks an iterator and its settings, the second how
// many calls it answers first, and each pair of bytes after them sets one
// byte of its state to a value. The pointers, and the lengths and counts
// that go with them (the text, the class overrides, the caller's breaker
// and the SA run it is given), are left alone, and so is the kind of an
// iterator with class overrides, since it says where their count is: an
// iterator forged there is outside the contract. Every call on what is left must succeed or
// refuse, without reading or writing outside the state, the text or the
// overrides, without asking the breaker about an offset outside its run,
// and without tripping an assertion.

#include "script_state.h"
#include "segmenter.h"

#include "maul-unicode/script.h"
#include "maul-unicode/segment.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Thai for the breaker, a CR LF, an emoji sequence, brackets around
// Greek, Han, digits and spaces.
static const char s_text[] =
    "\xE0\xB8\xAA\xE0\xB8\xA7\xE0\xB8\xB1\xE0\xB8\xAA\xE0\xB8\x94\xE0\xB8\xB5"
    " ab\r\n\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x92\xBB (\xCE\xB1\xCE\xB2) "
    "\xE4\xBA\xBA 1,5% x";

static const muniBreakClassOverride s_overrides[] = {{'_', muni_wbExtendNumLet},
                                                     {'-', muni_wbMidLetter}};

typedef struct Range
{
    size_t start;
    size_t size;
} Range;

// Breaks every three bytes, and traps when asked about an offset outside
// the run, which segment.h promises never happens.
static size_t Breaker(void* context, const char* run, size_t length, size_t from)
{
    (void)context;
    (void)run;
    if (from >= length)
    {
        __builtin_trap();
    }
    return from + 3 < length ? from + 3 : length;
}

static bool Inside(const Range* ranges, size_t count, size_t offset)
{
    for (size_t i = 0; i < count; i++)
    {
        if (offset - ranges[i].start < ranges[i].size)
        {
            return true;
        }
    }
    return false;
}

// Sets the state's bytes the input names, outside the ranges given.
static void Overwrite(unsigned char* state, size_t size, const Range* ranges, size_t count,
                      const uint8_t* data, size_t length)
{
    for (size_t i = 0; i + 1 < length; i += 2)
    {
        size_t offset = data[i] % size;
        if (!Inside(ranges, count, offset))
        {
            state[offset] = data[i + 1];
        }
    }
}

static void Script(uint8_t steps, const uint8_t* data, size_t length)
{
    muniScriptIterator iterator;
    muniScriptRun run;
    if (muniInitScriptIterator(&iterator, s_text, sizeof(s_text) - 1, false) != muni_success)
    {
        return;
    }
    for (uint8_t i = 0; i < steps % 8; i++)
    {
        (void)muniScriptIterator_NextRun(&iterator, &run);
    }
    static const Range kept[] = {
        {offsetof(ScriptState, cursor) + offsetof(muniCursor, text), sizeof(const uint8_t*)},
        {offsetof(ScriptState, cursor) + offsetof(muniCursor, length), sizeof(size_t)},
    };
    Overwrite((unsigned char*)&iterator, sizeof(ScriptState), kept, sizeof(kept) / sizeof(kept[0]),
              data, length);
    for (int i = 0; i < 64; i++)
    {
        muniResult status = muniScriptIterator_NextRun(&iterator, &run);
        if (status == muni_needMoreText)
        {
            status = muniScriptIterator_Feed(&iterator, "", 0, false);
        }
        if (status != muni_success)
        {
            break;
        }
    }
}

static void Segment(uint8_t choice, uint8_t steps, const uint8_t* data, size_t length)
{
    muniSegmentIterator iterator;
    size_t offset;
    bool mandatory;
    bool more = (choice & 8) != 0;
    muniResult status;
    switch (choice % 4)
    {
    case 0:
        status = muniInitGraphemeIterator(&iterator, s_text, sizeof(s_text) - 1, more);
        break;
    case 1:
        status = muniInitWordIterator(&iterator, s_text, sizeof(s_text) - 1, more);
        if (status == muni_success && (choice & 16) != 0)
        {
            status = muniSegmentIterator_SetBreakClassOverrides(&iterator, s_overrides, 2);
        }
        break;
    case 2:
        status = muniInitSentenceIterator(&iterator, s_text, sizeof(s_text) - 1, more);
        break;
    default:
        status = muniInitLineIterator(&iterator, s_text, sizeof(s_text) - 1, more);
        if (status == muni_success)
        {
            status = muniSegmentIterator_SetLineBreakStrictness(&iterator, (choice >> 5) & 3);
        }
        break;
    }
    if (status != muni_success)
    {
        return;
    }
    if ((choice & 32) != 0)
    {
        (void)muniSegmentIterator_SetComplexBreaker(&iterator, Breaker, nullptr);
    }
    for (uint8_t i = 0; i < steps % 8; i++)
    {
        (void)muniSegmentIterator_NextBreak(&iterator, &offset);
    }
    // The override count shares the rules' storage, and the kind says
    // which bytes of it hold the count, so both are kept where the
    // iterator has overrides. Without them the pointer is null, and any
    // kind and count are input every call must refuse or handle.
    Range kept[] = {
        {offsetof(muniSegmenter, cursor) + offsetof(muniCursor, text), sizeof(const uint8_t*)},
        {offsetof(muniSegmenter, cursor) + offsetof(muniCursor, length), sizeof(size_t)},
        {offsetof(muniSegmenter, complex.breaker), sizeof(muniComplexBreakFunction)},
        {offsetof(muniSegmenter, complex.context), sizeof(void*)},
        {offsetof(muniSegmenter, complex.text), sizeof(const uint8_t*)},
        {offsetof(muniSegmenter, complex.length), sizeof(size_t)},
        {offsetof(muniSegmenter, overrides), sizeof(const muniBreakClassOverride*)},
        {0, 0},
        {0, 0},
    };
    if (choice % 4 == 1 && (choice & 16) != 0)
    {
        kept[7] = (Range){offsetof(muniSegmenter, kind), 1};
        kept[8] = (Range){offsetof(muniSegmenter, rules.word.overrideCount), sizeof(uint32_t)};
    }
    Overwrite((unsigned char*)&iterator, sizeof(muniSegmenter), kept,
              sizeof(kept) / sizeof(kept[0]), data, length);
    muniWordKind kind;
    (void)muniSegmentIterator_GetWordKind(&iterator, &kind);
    (void)muniSegmentIterator_GetLineBreakStrictness(&iterator);
    (void)muniSegmentIterator_GetWritingSystem(&iterator);
    for (int i = 0; i < 64; i++)
    {
        status = (choice & 64) != 0
                     ? muniSegmentIterator_NextLineBreak(&iterator, &offset, &mandatory)
                     : muniSegmentIterator_NextBreak(&iterator, &offset);
        if (status == muni_needMoreText)
        {
            status = muniSegmentIterator_Feed(&iterator, "", 0, false);
        }
        if (status != muni_success)
        {
            break;
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 2)
    {
        return 0;
    }
    if ((data[0] & 128) != 0)
    {
        Script(data[1], data + 2, size - 2);
    }
    else
    {
        Segment(data[0], data[1], data + 2, size - 2);
    }
    return 0;
}
