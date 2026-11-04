// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Line breaking with the default settings, normal for any writing system
// but Chinese and Japanese, which every new line iterator has: the rules
// of UAX #14 in line_rules.h, which read no setting here.
// segment.c sends the other settings to line_tailored.c and
// line_anywhere.c, once per call.

#include "grapheme.h"
#include "line_rules.h"

static uint8_t Lookup(const muniSegmenter* segmenter, uint32_t codePoint)
{
    return muniLineLookup(segmenter, codePoint, false);
}

static muniDecision Decide(const muniSegmenter* segmenter, uint8_t value, uint32_t codePoint)
{
    return muniLineDecide(segmenter, value, codePoint, false);
}

muniResult muniNextLineSegment(muniSegmenter* segmenter, size_t* offsetOut)
{
    return muniSegmenterNext(segmenter, offsetOut, Lookup, Decide, Resolve, Absorb, nullptr);
}

// The rules of every setting, checked here beside the default ones:
// breaking anywhere keeps the grapheme rules where the others keep the
// classes before the position.
bool muniLineRulesValid(const muniSegmenter* segmenter, const void* raw)
{
    const muniLineRules* state = &segmenter->rules.line;
    if (state->actual > muni_lbHh)
    {
        return false;
    }
    if (muniLineStrictness(segmenter) == muni_lineBreakAnywhere)
    {
        return segmenter->holdKind == 0 &&
               muniGraphemeRulesValid(&state->grapheme, raw,
                                      offsetof(muniSegmenter, rules.line.grapheme));
    }
    return state->previous <= muni_lbHh && state->before <= muni_lbHh && state->lead <= muni_lbHh &&
           state->number <= NumberClosed && segmenter->holdKind <= HoldAksara &&
           muniIsFlagByte(raw, offsetof(muniSegmenter, rules.line.leadInitialQuote)) &&
           muniIsFlagByte(raw, offsetof(muniSegmenter, rules.line.sawInfix)) &&
           muniIsFlagByte(raw, offsetof(muniSegmenter, rules.line.oddRegional));
}
