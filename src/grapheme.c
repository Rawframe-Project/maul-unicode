// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Extended grapheme cluster boundaries, UAX #29 rules GB3 to GB999. The
// rules look back, never ahead, so a boundary before a code point is
// decided from that code point and the state the iterator carries:
//
// - the previous code point's Grapheme_Cluster_Break;
// - whether the text since the last InCB=Linker is Linker Extend*, for GB9c;
// - whether it is Extended_Pictographic Extend* (and then ZWJ), for GB11;
// - whether an odd number of regional indicators precedes, for GB12 and
//   GB13.

#include "cursor.h"
#include "tables.h"

#include "maul-unicode/properties.h"
#include "maul-unicode/segment.h"

#include <string.h>

// Where a GB11 emoji sequence stands.
enum
{
    EmojiNone = 0,
    EmojiPictographic = 1, // Extended_Pictographic Extend*
    EmojiJoined = 2,       // Extended_Pictographic Extend* ZWJ
};

typedef struct GraphemeState
{
    muniCursor cursor;
    uint8_t previous; // the previous code point's Grapheme_Cluster_Break
    uint8_t emoji;    // one of the Emoji values
    bool linker;      // InCB=Linker followed by InCB=Extend only
    bool oddRegional; // an odd number of regional indicators precedes
    bool started;     // a code point has been read
    bool endReported; // the boundary at the end has been reported
} GraphemeState;

static_assert(sizeof(GraphemeState) <= sizeof(muniGraphemeIterator),
              "the public iterator must hold the state");

// The public iterator is opaque storage; the state moves in and out with
// memcpy, which keeps the accesses within the C aliasing rules.
static GraphemeState Load(const muniGraphemeIterator* iterator)
{
    GraphemeState state;
    memcpy(&state, iterator, sizeof(state));
    return state;
}

static void Store(muniGraphemeIterator* iterator, const GraphemeState* state)
{
    memcpy(iterator, state, sizeof(*state));
}

static bool IsControl(uint8_t value)
{
    return value == muni_gcbControl || value == muni_gcbCr || value == muni_gcbLf;
}

// GB6 to GB8: Hangul syllable sequences stay together.
static bool JoinsHangul(uint8_t before, uint8_t after)
{
    if (before == muni_gcbL)
    {
        return after == muni_gcbL || after == muni_gcbV || after == muni_gcbLv ||
               after == muni_gcbLvt;
    }
    if (before == muni_gcbLv || before == muni_gcbV)
    {
        return after == muni_gcbV || after == muni_gcbT;
    }
    return (before == muni_gcbLvt || before == muni_gcbT) && after == muni_gcbT;
}

// Whether there is a boundary between the text so far and a code point
// whose Grapheme_Cluster_Break is after.
static bool IsBoundary(const GraphemeState* state, uint8_t after, uint32_t codePoint)
{
    uint8_t before = state->previous;
    if (before == muni_gcbCr && after == muni_gcbLf)
    {
        return false; // GB3
    }
    if (IsControl(before) || IsControl(after))
    {
        return true; // GB4, GB5
    }
    if (JoinsHangul(before, after))
    {
        return false; // GB6 to GB8
    }
    if (after == muni_gcbExtend || after == muni_gcbZwj || after == muni_gcbSpacingMark ||
        before == muni_gcbPrepend)
    {
        return false; // GB9, GB9a, GB9b
    }
    if (state->linker && muniLookupIndicConjunctBreak(codePoint) == muni_incbConsonant)
    {
        return false; // GB9c
    }
    if (state->emoji == EmojiJoined && before == muni_gcbZwj &&
        muniLookupExtendedPictographic(codePoint) != 0)
    {
        return false; // GB11
    }
    if (before == muni_gcbRegionalIndicator && after == muni_gcbRegionalIndicator &&
        state->oddRegional)
    {
        return false; // GB12, GB13
    }
    return true; // GB999
}

// Moves the state past a code point whose Grapheme_Cluster_Break is value.
static void Absorb(GraphemeState* state, uint8_t value, uint32_t codePoint)
{
    uint8_t conjunct = muniLookupIndicConjunctBreak(codePoint);
    if (conjunct == muni_incbLinker)
    {
        state->linker = true;
    }
    else if (conjunct != muni_incbExtend)
    {
        state->linker = false;
    }
    if (muniLookupExtendedPictographic(codePoint) != 0)
    {
        state->emoji = EmojiPictographic;
    }
    else if (value == muni_gcbExtend && state->emoji == EmojiPictographic)
    {
        state->emoji = EmojiPictographic;
    }
    else if (value == muni_gcbZwj && state->emoji == EmojiPictographic)
    {
        state->emoji = EmojiJoined;
    }
    else
    {
        state->emoji = EmojiNone;
    }
    state->oddRegional = value == muni_gcbRegionalIndicator && !state->oddRegional;
    state->previous = value;
    state->started = true;
}

muniResult muniInitGraphemeIterator(muniGraphemeIterator* iterator, const char* text, size_t length,
                                    bool moreFollows)
{
    if (iterator == nullptr || (text == nullptr && length != 0))
    {
        return muni_errorInvalid;
    }
    GraphemeState state;
    memset(&state, 0, sizeof(state));
    muniCursorInit(&state.cursor, text, length, moreFollows);
    memset(iterator, 0, sizeof(*iterator));
    Store(iterator, &state);
    return muni_success;
}

muniResult muniFeedGraphemeIterator(muniGraphemeIterator* iterator, const char* text, size_t length,
                                    bool moreFollows)
{
    if (iterator == nullptr || (text == nullptr && length != 0))
    {
        return muni_errorInvalid;
    }
    GraphemeState state = Load(iterator);
    muniCursorFeed(&state.cursor, text, length, moreFollows);
    Store(iterator, &state);
    return muni_success;
}

// Runs the rules from the cursor until a boundary, the end or the end of
// the piece.
static muniResult NextBreak(GraphemeState* state, size_t* offsetOut)
{
    for (;;)
    {
        uint32_t codePoint;
        size_t size;
        muniResult status = muniCursorPeek(&state->cursor, &codePoint, &size);
        if (status == muni_needMoreText)
        {
            return status;
        }
        if (status == muni_done)
        {
            if (!state->started || state->endReported)
            {
                return muni_done;
            }
            state->endReported = true;
            *offsetOut = state->cursor.offset;
            return muni_success;
        }
        uint8_t value = muniLookupGraphemeClusterBreak(codePoint);
        bool boundary = state->started && IsBoundary(state, value, codePoint);
        size_t offset = state->cursor.offset;
        Absorb(state, value, codePoint);
        muniCursorAdvance(&state->cursor, size);
        if (boundary)
        {
            *offsetOut = offset;
            return muni_success;
        }
    }
}

muniResult muniNextGraphemeBreak(muniGraphemeIterator* iterator, size_t* offsetOut)
{
    if (iterator == nullptr || offsetOut == nullptr)
    {
        return muni_errorInvalid;
    }
    GraphemeState state = Load(iterator);
    muniResult status = NextBreak(&state, offsetOut);
    Store(iterator, &state);
    return status;
}

muniResult muniFindGraphemeBreaks(const char* text, size_t length, size_t* offsets, size_t capacity,
                                  size_t* countOut)
{
    if ((offsets == nullptr && capacity != 0) || countOut == nullptr)
    {
        return muni_errorInvalid;
    }
    muniGraphemeIterator iterator;
    muniResult status = muniInitGraphemeIterator(&iterator, text, length, false);
    if (status != muni_success)
    {
        return status;
    }
    size_t count = 0;
    size_t offset;
    while (muniNextGraphemeBreak(&iterator, &offset) == muni_success)
    {
        if (count < capacity)
        {
            offsets[count] = offset;
        }
        count += 1;
    }
    *countOut = count;
    return count > capacity ? muni_errorCapacity : muni_success;
}
