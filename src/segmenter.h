// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The engine the three UAX #29 segmenters share. It walks the text with a
// cursor and asks a rule set, for each code point, whether there is a
// boundary before it. A rule that needs to see further text answers
// "hold": the engine keeps the undecided boundary, reads on, and asks the
// rule set about each following code point until the hold resolves.
//
// The engine is a static inline function taking the rules as function
// arguments; each rule set calls it with its own functions, so the
// compiler builds one loop per rule set with the rules inlined.

#ifndef MAUL_UNICODE_SRC_SEGMENTER_H
#define MAUL_UNICODE_SRC_SEGMENTER_H

#include "cursor.h"

#include "maul-unicode/segment.h"

// Which segmentation an iterator performs.
enum
{
    muni_segmentGrapheme = 1,
    muni_segmentWord = 2,
    muni_segmentSentence = 3,
};

// A rule set's answer about the position before a code point.
typedef enum muniDecision
{
    muni_decideBreak,    // a boundary
    muni_decideJoin,     // no boundary
    muni_decideHold,     // undecided until later text is seen
    muni_decideContinue, // while holding: still undecided, read on
} muniDecision;

// The state of each rule set, beyond the previous code point.
typedef struct muniGraphemeRules
{
    uint8_t previous; // the previous code point's Grapheme_Cluster_Break
    uint8_t emoji;    // where a GB11 emoji sequence stands
    bool linker;      // InCB=Linker followed by InCB=Extend only, for GB9c
    bool oddRegional; // an odd number of regional indicators precedes
} muniGraphemeRules;

typedef struct muniWordRules
{
    uint8_t actual;   // the previous code point's Word_Break
    uint8_t previous; // the previous one after rule WB4, which skips Extend
    uint8_t before;   // the one before that, after rule WB4
    bool oddRegional; // an odd number of regional indicators precedes
} muniWordRules;

typedef struct muniSentenceRules
{
    uint8_t actual;   // the previous code point's Sentence_Break
    uint8_t previous; // the previous one after rule SB5, which skips Extend
    uint8_t before;   // the one before that, after rule SB5
    uint8_t ending;   // where a sentence ending "SATerm Close* Sp*" stands
    bool aterm;       // the ending's terminator is an ATerm
} muniSentenceRules;

typedef struct muniSegmenter
{
    muniCursor cursor;
    size_t heldOffset; // the offset of the undecided boundary
    uint8_t kind;      // a muni_segment value
    bool started;      // a code point has been read
    bool endReported;  // the boundary at the end has been reported
    bool holding;      // a boundary at heldOffset is undecided
    bool waiting;      // the last call returned muni_needMoreText

    union
    {
        muniGraphemeRules grapheme;
        muniWordRules word;
        muniSentenceRules sentence;
    } rules;
} muniSegmenter;

static_assert(sizeof(muniSegmenter) <= sizeof(muniSegmentIterator),
              "the public iterator must hold the segmenter");

// The rules as functions of the segmenter and the next code point: its
// property value from lookup, the decision about the boundary before it,
// the decision while a hold is open, and the update after it is read.
typedef uint8_t (*muniLookupRule)(uint32_t codePoint);
typedef muniDecision (*muniDecideRule)(const muniSegmenter* segmenter, uint8_t value,
                                       uint32_t codePoint);
typedef void (*muniAbsorbRule)(muniSegmenter* segmenter, uint8_t value, uint32_t codePoint);

// Finds the next boundary: muni_success and its offset, muni_done after
// the end, or muni_needMoreText.
static inline muniResult muniSegmenterNext(muniSegmenter* segmenter, size_t* offsetOut,
                                           muniLookupRule lookup, muniDecideRule decide,
                                           muniDecideRule resolve, muniAbsorbRule absorb)
{
    for (;;)
    {
        uint32_t codePoint;
        size_t size;
        muniResult status = muniCursorPeek(&segmenter->cursor, &codePoint, &size);
        if (status == muni_needMoreText)
        {
            return status;
        }
        if (status == muni_done)
        {
            if (segmenter->holding)
            {
                segmenter->holding = false; // the end is never what a hold waits for
                *offsetOut = segmenter->heldOffset;
                return muni_success;
            }
            if (!segmenter->started || segmenter->endReported)
            {
                return muni_done;
            }
            segmenter->endReported = true;
            *offsetOut = segmenter->cursor.offset;
            return muni_success;
        }
        uint8_t value = lookup(codePoint);
        muniDecision decision = muni_decideJoin;
        if (segmenter->holding)
        {
            decision = resolve(segmenter, value, codePoint);
            if (decision == muni_decideBreak)
            {
                // The hold failed: report it, and decide about this code
                // point on the next call.
                segmenter->holding = false;
                *offsetOut = segmenter->heldOffset;
                return muni_success;
            }
            segmenter->holding = decision == muni_decideContinue;
        }
        if (!segmenter->holding && segmenter->started)
        {
            decision = decide(segmenter, value, codePoint);
        }
        size_t offset = segmenter->cursor.offset;
        if (decision == muni_decideHold)
        {
            segmenter->holding = true;
            segmenter->heldOffset = offset;
        }
        absorb(segmenter, value, codePoint);
        segmenter->started = true;
        muniCursorAdvance(&segmenter->cursor, size);
        if (decision == muni_decideBreak)
        {
            *offsetOut = offset;
            return muni_success;
        }
    }
}

// The engine's entry for each rule set, defined in grapheme.c, word.c and
// sentence.c.
muniResult muniNextGraphemeSegment(muniSegmenter* segmenter, size_t* offsetOut);
muniResult muniNextWordSegment(muniSegmenter* segmenter, size_t* offsetOut);
muniResult muniNextSentenceSegment(muniSegmenter* segmenter, size_t* offsetOut);

#endif // MAUL_UNICODE_SRC_SEGMENTER_H
