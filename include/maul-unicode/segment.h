// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Text segmentation (UAX #29): the boundaries of grapheme clusters, the
// units a user perceives as one character, which caret movement,
// selection and backspace must never split.
//
// An iterator walks UTF-8 text and reports each boundary after the start
// of the text, the last one being the end of the text; empty text has
// none. Offsets are in bytes from the start of the whole text. Each
// maximal ill-formed UTF-8 subpart counts as one U+FFFD.
//
// Text may arrive in pieces. An iterator initialized with more text to
// follow stops with muni_needMoreText when the next boundary depends on
// text it has not seen; muniFeedGraphemeIterator hands it the next piece,
// which may begin in the middle of a UTF-8 sequence.

#ifndef MAUL_UNICODE_SEGMENT_H
#define MAUL_UNICODE_SEGMENT_H

#include "maul-unicode/base.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // An iterator over grapheme cluster boundaries. Its contents are
    // private; it is plain data the caller keeps anywhere, usually on the
    // stack, and needs no cleanup.
    typedef struct muniGraphemeIterator
    {
        uint64_t opaque[8];
    } muniGraphemeIterator;

    /// Starts an iterator over a UTF-8 text or its first piece.
    ///
    /// @param iterator     The iterator to initialize.
    /// @param text         The text. May be NULL when length is 0.
    /// @param length       The number of bytes.
    /// @param moreFollows  true when more pieces of the text will be fed.
    /// @return `muni_success`, or `muni_errorInvalid` for a NULL iterator
    ///         or a NULL text with a nonzero length.
    /// @par Thread safety
    /// Safe from any thread; an iterator is used by one thread at a time.
    MUNI_NODISCARD MUNI_API muniResult muniInitGraphemeIterator(muniGraphemeIterator* iterator,
                                                                const char* text, size_t length,
                                                                bool moreFollows);

    /// Hands an iterator the next piece of its text, after it returned
    /// muni_needMoreText. The iterator keeps no pointer to earlier pieces.
    ///
    /// @param iterator     The iterator.
    /// @param text         The next piece. May be NULL when length is 0.
    /// @param length       The number of bytes.
    /// @param moreFollows  true when still more pieces will be fed.
    /// @return `muni_success`, or `muni_errorInvalid` for a NULL argument.
    /// @par Thread safety
    /// Safe from any thread; an iterator is used by one thread at a time.
    MUNI_NODISCARD MUNI_API muniResult muniFeedGraphemeIterator(muniGraphemeIterator* iterator,
                                                                const char* text, size_t length,
                                                                bool moreFollows);

    /// Finds the next grapheme cluster boundary.
    ///
    /// @param iterator   The iterator.
    /// @param offsetOut  Receives the boundary's byte offset from the start
    ///                   of the whole text.
    /// @return `muni_success` with a boundary; `muni_done` after the last
    ///         one; `muni_needMoreText` when the next boundary depends on
    ///         text not yet fed; `muni_errorInvalid` for a NULL argument.
    /// @par Thread safety
    /// Safe from any thread; an iterator is used by one thread at a time.
    MUNI_NODISCARD MUNI_API muniResult muniNextGraphemeBreak(muniGraphemeIterator* iterator,
                                                             size_t* offsetOut);

    /// Writes the grapheme cluster boundaries of a whole UTF-8 text into a
    /// caller array: every boundary after the start, the end included.
    ///
    /// @param text         The text. May be NULL when length is 0.
    /// @param length       The number of bytes.
    /// @param offsets      The output. May be NULL when capacity is 0.
    /// @param capacity     The number of offsets the output can hold.
    /// @param countOut     Receives the number of boundaries, which may
    ///                     exceed capacity; the ones that fit are written.
    /// @return `muni_success`, `muni_errorCapacity` when they do not all
    ///         fit, or `muni_errorInvalid` for a NULL argument.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_NODISCARD MUNI_API muniResult muniFindGraphemeBreaks(const char* text, size_t length,
                                                              size_t* offsets, size_t capacity,
                                                              size_t* countOut);

#ifdef __cplusplus
}
#endif

#endif // MAUL_UNICODE_SEGMENT_H
