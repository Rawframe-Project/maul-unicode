// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// NFKC_Casefold (UAX #44, DerivedNormalizationProps.txt): each code
// point maps to its NFKC_CF value, what compatibility decomposition,
// full case folding and removal of default ignorables leave when
// repeated until nothing changes, and the result is put in NFC. The
// mapping is found in decomposed form, which is canonically equivalent
// to the composed one of the UCD, so the normalizer's composition gives
// the same text; the test compares every code point with the UCD.

#include "case_map.h"
#include "decompose.h"
#include "encoding.h"
#include "invariant.h"
#include "normalizer.h"
#include "overlap.h"
#include "tables.h"
#include "writer.h"

#include "maul-unicode/case.h"

#include <string.h>

// The most code points a mapping holds while it is found. The longest
// NFKC_CF value has 18 code points, U+FDFA's, and no step comes near 64.
// A mapping depends on its code point alone, and test_case checks the
// NFKC_CF of every code point against the UCD in builds that keep
// MUNI_ASSERT, so every mapping fits at every step. A table of a later
// Unicode version could break that, so a mapping that would not fit
// traps where MUNI_ASSERT is kept and is refused where it is not.
#define MAX_MAPPING 64

// One round over a mapping: removes default ignorables, folds and
// decomposes the rest into to. Returns the new length, or MAX_MAPPING + 1
// when it would not fit; *changedOut tells whether anything changed.
static size_t Round(const uint32_t* from, size_t count, uint32_t* to, bool* changedOut)
{
    size_t written = 0;
    bool changed = false;
    for (size_t i = 0; i < count; i++)
    {
        if (muniLookupDefaultIgnorable(from[i]) != 0)
        {
            changed = true;
            continue;
        }
        uint32_t folded[MUNI_MAX_CASE_MAPPING];
        size_t foldedCount = muniMapCaseFully(from[i], muni_caseKindFold, folded);
        changed = changed || foldedCount != 1 || folded[0] != from[i];
        for (size_t k = 0; k < foldedCount; k++)
        {
            if (written + MUNI_MAX_DECOMPOSITION > MAX_MAPPING)
            {
                MUNI_ASSERT(false);
                return MAX_MAPPING + 1;
            }
            size_t parts = muniDecomposeFully(folded[k], true, to + written);
            changed = changed || parts != 1 || to[written] != folded[k];
            written += parts;
        }
    }
    *changedOut = changed;
    return written;
}

// Writes the NFKC_CF mapping of codePoint, fully decomposed, into out,
// which holds MAX_MAPPING code points, and returns its length; returns
// MAX_MAPPING + 1 when it would not fit.
static size_t Map(uint32_t codePoint, uint32_t* out)
{
    uint32_t other[MAX_MAPPING];
    uint32_t* buffers[2] = {out, other};
    out[0] = codePoint;
    size_t count = 1;
    size_t current = 0;
    bool changed = true;
    while (changed)
    {
        count = Round(buffers[current], count, buffers[current ^ 1], &changed);
        current ^= 1;
        if (count > MAX_MAPPING)
        {
            return count;
        }
    }
    if (current != 0)
    {
        for (size_t i = 0; i < count; i++)
        {
            out[i] = other[i];
        }
    }
    return count;
}

// Feeds the mapping of one code point to the normalizer; false when it
// outgrows a limit.
MUNI_NODISCARD static bool Feed(muniNormalizer* normalizer, uint32_t codePoint)
{
    if (codePoint < 0xA0)
    {
        // Below U+00A0 only A to Z change, and nothing is ignorable.
        uint32_t lower = codePoint - 'A' < 26 ? codePoint + 0x20 : codePoint;
        return muniNormalizerAdd(normalizer, lower);
    }
    uint32_t mapping[MAX_MAPPING];
    size_t count = Map(codePoint, mapping);
    if (count > MAX_MAPPING)
    {
        return false;
    }
    for (size_t i = 0; i < count; i++)
    {
        if (!muniNormalizerAdd(normalizer, mapping[i]))
        {
            return false;
        }
    }
    return true;
}

muniTextResult muniGetNfkcCasefold(const char* text, size_t length, muniConvertMode mode,
                                   char* textOut, size_t capacity, size_t* neededOut)
{
    if ((text == nullptr && length != 0) || (textOut == nullptr && capacity != 0) ||
        neededOut == nullptr || (mode != muni_convertStrict && mode != muni_convertReplace) ||
        muniOverlaps(text, length, textOut, capacity, 1))
    {
        return (muniTextResult){muni_errorInvalid, 0};
    }
    muniNormalizer normalizer;
    muniNormalizerInit(&normalizer, true, textOut, capacity);
    const uint8_t* bytes = (const uint8_t*)text;
    size_t offset = 0;
    while (offset < length)
    {
        uint32_t codePoint;
        size_t size;
        muniResult status = muniStepUtf8(bytes + offset, length - offset, &codePoint, &size);
        if (status != muni_success && mode == muni_convertStrict)
        {
            muniNormalizerFlush(&normalizer);
            *neededOut = normalizer.writer.needed;
            return (muniTextResult){status, offset};
        }
        if (!Feed(&normalizer, codePoint))
        {
            *neededOut = normalizer.writer.needed;
            return (muniTextResult){muni_errorLimit, offset};
        }
        offset += size;
    }
    muniNormalizerFlush(&normalizer);
    *neededOut = normalizer.writer.needed;
    return (muniTextResult){!muniWriterFits(&normalizer.writer) ? muni_errorCapacity : muni_success,
                            length};
}

// Caseless comparison. Each text becomes a stream of its folded form as
// UTF-8, made a little at a time into small buffers, and the two streams
// are compared byte by byte, which is code point order, up to the first
// difference. Every stage holds a bounded amount: a code point's mapping
// waits in the queue and enters the last normalizer one code point at a
// time, and a stream stops as soon as that normalizer has written
// anything, which is at most one segment.

// A code point's NFKC_CF mapping, or three folded code points, each
// fully decomposed.
#define QUEUE         MAX_MAPPING
#define SEGMENT_BYTES (MUNI_MAX_NORMALIZATION_SEGMENT * 4)
// What one code point of input can make the first normalizer write: the
// segment it held and the code point's decomposition.
#define FIRST_BYTES ((MUNI_MAX_NORMALIZATION_SEGMENT + MUNI_MAX_DECOMPOSITION) * 4)

typedef struct Stream
{
    const uint8_t* text;
    size_t length;
    size_t offset;
    muniCaselessMatch match;
    uint32_t queue[QUEUE];
    size_t queued;
    size_t taken;
    // Canonical matching: the NFD of the text before folding, and how
    // much of it has been folded.
    muniNormalizer first;
    char firstBytes[FIRST_BYTES];
    size_t firstTaken;
    bool firstFlushed;
    // The folded form: NFD again after folding, NFC of the NFKC_CF
    // mappings, or the folded code points themselves; and how much of
    // it has been compared.
    muniNormalizer last;
    char out[SEGMENT_BYTES];
    size_t outTaken;
    bool lastFlushed;
} Stream;

static void StreamInit(Stream* stream, const char* text, size_t length, muniCaselessMatch match)
{
    stream->text = (const uint8_t*)text;
    stream->length = length;
    stream->offset = 0;
    stream->match = match;
    stream->queued = 0;
    stream->taken = 0;
    muniNormalizerInit(&stream->first, false, stream->firstBytes, sizeof(stream->firstBytes));
    stream->firstTaken = 0;
    stream->firstFlushed = false;
    muniNormalizerInit(&stream->last, match == muni_caselessIdentifier, stream->out,
                       sizeof(stream->out));
    stream->outTaken = 0;
    stream->lastFlushed = false;
}

// Queues the full case folding of a code point, each code point of it
// fully decomposed when decompose is set.
static void QueueFolded(Stream* stream, uint32_t codePoint, bool decompose)
{
    uint32_t folded[MUNI_MAX_CASE_MAPPING];
    size_t count = muniMapCaseFully(codePoint, muni_caseKindFold, folded);
    for (size_t k = 0; k < count; k++)
    {
        if (decompose)
        {
            MUNI_ASSERT(stream->queued + MUNI_MAX_DECOMPOSITION <= QUEUE);
            stream->queued += muniDecomposeFully(folded[k], false, stream->queue + stream->queued);
        }
        else
        {
            stream->queue[stream->queued++] = folded[k];
        }
    }
}

// Takes the next code point of input into the stream's first stage;
// false when a segment outgrows its limit.
MUNI_NODISCARD static bool TakeInput(Stream* stream)
{
    uint32_t codePoint;
    size_t size;
    (void)muniStepUtf8(stream->text + stream->offset, stream->length - stream->offset, &codePoint,
                       &size);
    stream->offset += size;
    switch (stream->match)
    {
    case muni_caselessCanonical:
    {
        uint32_t parts[MUNI_MAX_DECOMPOSITION];
        size_t count = muniDecomposeFully(codePoint, false, parts);
        stream->first.writer.needed = 0;
        stream->firstTaken = 0;
        for (size_t i = 0; i < count; i++)
        {
            if (!muniNormalizerAdd(&stream->first, parts[i]))
            {
                return false;
            }
        }
        MUNI_ASSERT(muniWriterFits(&stream->first.writer));
        return true;
    }
    case muni_caselessIdentifier:
        if (codePoint < 0xA0)
        {
            // Below U+00A0 only A to Z change, as in Feed.
            stream->queue[0] = codePoint - 'A' < 26 ? codePoint + 0x20 : codePoint;
            stream->queued = 1;
            return true;
        }
        stream->queued = Map(codePoint, stream->queue);
        return stream->queued <= MAX_MAPPING;
    default:
        QueueFolded(stream, codePoint, false);
        return true;
    }
}

// Makes more of the folded form: afterwards the output holds bytes not
// yet compared, unless the text is used up. Returns muni_errorLimit when
// a segment outgrows its limit, and muni_success otherwise.
static muniResult Pull(Stream* stream)
{
    muniWriter* writer = &stream->last.writer;
    muniWriter* firstWriter = &stream->first.writer;
    writer->needed = 0;
    stream->outTaken = 0;
    while (writer->needed == 0)
    {
        if (stream->taken < stream->queued)
        {
            uint32_t codePoint = stream->queue[stream->taken++];
            if (stream->match == muni_caselessDefault)
            {
                muniWriterPut(writer, codePoint);
            }
            else if (!muniNormalizerAdd(&stream->last, codePoint))
            {
                return muni_errorLimit;
            }
            continue;
        }
        stream->queued = 0;
        stream->taken = 0;
        if (stream->firstTaken < firstWriter->needed)
        {
            // The NFD of the input, one code point at a time, folded and
            // decomposed again.
            uint32_t codePoint;
            size_t size;
            (void)muniStepUtf8((const uint8_t*)stream->firstBytes + stream->firstTaken,
                               firstWriter->needed - stream->firstTaken, &codePoint, &size);
            stream->firstTaken += size;
            QueueFolded(stream, codePoint, true);
            continue;
        }
        if (stream->offset < stream->length)
        {
            if (!TakeInput(stream))
            {
                return muni_errorLimit;
            }
            continue;
        }
        if (stream->match == muni_caselessCanonical && !stream->firstFlushed)
        {
            firstWriter->needed = 0;
            stream->firstTaken = 0;
            muniNormalizerFlush(&stream->first);
            stream->firstFlushed = true;
            continue;
        }
        if (stream->lastFlushed)
        {
            break;
        }
        muniNormalizerFlush(&stream->last);
        stream->lastFlushed = true;
    }
    MUNI_ASSERT(muniWriterFits(writer));
    return muni_success;
}

// Makes more of the folded form when all of it has been compared, and
// gives how much is left to compare, 0 at the end of the text.
static muniResult Available(Stream* stream, size_t* leftOut)
{
    if (stream->outTaken == stream->last.writer.needed)
    {
        muniResult status = Pull(stream);
        if (status != muni_success)
        {
            return status;
        }
    }
    *leftOut = stream->last.writer.needed - stream->outTaken;
    return muni_success;
}

// Compares the folded forms of two streams up to their first difference.
static muniResult Compare(Stream* streams, int32_t* orderOut)
{
    for (;;)
    {
        size_t left[2];
        muniResult status = Available(&streams[0], &left[0]);
        status = status == muni_success ? Available(&streams[1], &left[1]) : status;
        if (status != muni_success)
        {
            return status;
        }
        if (left[0] == 0 || left[1] == 0)
        {
            *orderOut = left[0] == left[1] ? 0 : left[0] == 0 ? -1 : 1;
            return muni_success;
        }
        size_t common = left[0] < left[1] ? left[0] : left[1];
        int order = memcmp(streams[0].out + streams[0].outTaken,
                           streams[1].out + streams[1].outTaken, common);
        if (order != 0)
        {
            *orderOut = order < 0 ? -1 : 1;
            return muni_success;
        }
        streams[0].outTaken += common;
        streams[1].outTaken += common;
    }
}

muniResult muniCompareCaseless(const char* first, size_t firstLength, const char* second,
                               size_t secondLength, muniCaselessMatch match, int32_t* orderOut)
{
    if ((first == nullptr && firstLength != 0) || (second == nullptr && secondLength != 0) ||
        (uint32_t)match > muni_caselessIdentifier || orderOut == nullptr)
    {
        return muni_errorInvalid;
    }
    Stream streams[2];
    StreamInit(&streams[0], first, firstLength, match);
    StreamInit(&streams[1], second, secondLength, match);
    return Compare(streams, orderOut);
}
