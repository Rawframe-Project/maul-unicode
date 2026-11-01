// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Text segmentation (UAX #29): the boundaries of grapheme clusters, the
// units a user perceives as one character, which caret movement,
// selection and backspace must never split; of words, for double-click
// selection, word movement and search; and of sentences. Line breaking
// (UAX #14): the positions where a line may, or must, end.
//
// An iterator walks UTF-8 text and reports each boundary after the start
// of the text, the last one being the end of the text; empty text has
// none. Offsets are in bytes from the start of the whole text. Each
// maximal ill-formed UTF-8 subpart counts as one U+FFFD. Word boundaries
// are the positions UAX #29 defines; between two of them may lie a word,
// a run of spaces or a punctuation mark, which the caller tells apart.
//
// Text may arrive in pieces. An iterator initialized with more text to
// follow stops with muni_needMoreText when the next boundary depends on
// text it has not seen; muniSegmentIterator_Feed hands it the next piece,
// which may begin in the middle of a UTF-8 sequence. Some rules look
// ahead: a word boundary may wait for the next code point, and a
// sentence boundary after an abbreviation's period for the next letter.

#ifndef MAUL_UNICODE_SEGMENT_H
#define MAUL_UNICODE_SEGMENT_H

#include "maul-unicode/base.h"
#include "maul-unicode/properties.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // An iterator over grapheme cluster, word or sentence boundaries or
    // line break opportunities. Its contents are private; it is plain data
    // the caller keeps anywhere, usually on the stack, and needs no
    // cleanup.
    typedef struct muniSegmentIterator
    {
        uint64_t opaque[16];
    } muniSegmentIterator;

    // A caller's word segmenter for Thai, Lao, Khmer and Burmese, which
    // write words without spaces (Line_Break=SA). It receives a maximal
    // run of SA text as UTF-8 and a byte offset from inside it, and
    // returns the byte offset of the first break between words after
    // from, or length when there is none. It must return the same answer
    // for the same run and offset each time it is asked. It must not
    // initialize the iterator that called it; any other call it makes on
    // that iterator returns muni_errorBusy.
    typedef size_t (*muniComplexBreakFunction)(void* context, const char* run, size_t length,
                                               size_t from);

    // How strictly a line iterator keeps text together: the levels of the
    // CSS line-break property (CSS Text 3, section 5.2). Normal, strict
    // and loose follow UAX #14 with the breaks CSS allows at each level
    // added; anywhere breaks between any two grapheme clusters. An added
    // break never splits what UAX #14 glues: a word joiner or no-break
    // space and its neighbors, an opening punctuation or quotation mark
    // and what follows, and a closing punctuation after a prefix.
    typedef int32_t muniLineBreakStrictness;

    enum
    {
        // The rules of UAX #14, with Line_Break=CJ resolved as NS. With
        // the Chinese or Japanese writing system, a line may also break
        // before U+301C and U+30A0.
        muni_lineBreakNormal = 0,
        // As normal, without the breaks before U+301C and U+30A0.
        muni_lineBreakStrict = 1,
        // As normal, with CJ (small kana and the prolonged sound mark)
        // resolved as ID, and breaks before U+2010 and U+2013 after an
        // ideograph, before the iteration marks U+3005, U+303B, U+309D,
        // U+309E, U+30FD and U+30FE, and between two inseparable
        // characters (Line_Break=IN). With the Chinese or Japanese
        // writing system, also before the centered punctuation U+30FB,
        // U+FF1A, U+FF1B, U+FF65, U+203C, U+2047, U+2048, U+2049, U+FF01
        // and U+FF1F, before a postfix (PO) and after a prefix (PR) whose
        // East_Asian_Width is Ambiguous, Fullwidth or Wide.
        muni_lineBreakLoose = 2,
        // A break between any two grapheme clusters, whatever the rules
        // of UAX #14 say, but not just before a hard line break; hard
        // line breaks stay mandatory.
        muni_lineBreakAnywhere = 3,
    };

    // The writing system of the text a line iterator breaks, for the
    // rules of muniLineBreakStrictness that CSS applies to Chinese and
    // Japanese only. The caller knows it from the text's language.
    typedef int32_t muniWritingSystem;

    enum
    {
        // Any writing system but Chinese and Japanese, or unknown.
        muni_writingOther = 0,
        // Chinese or Japanese.
        muni_writingChineseJapanese = 1,
    };

    /// Starts an iterator over the grapheme cluster boundaries of a UTF-8 text or
    /// its first piece.
    ///
    /// @param iterator     The iterator to initialize.
    /// @param text         The text. May be NULL when length is 0.
    /// @param length       The number of bytes.
    /// @param moreFollows  true when more pieces of the text will be fed.
    /// @return `muni_success`, or `muni_errorInvalid` for a NULL iterator
    ///         or a NULL text with a nonzero length.
    /// @par Thread safety
    /// Safe from any thread; an iterator is used by one thread at a time.
    MUNI_NODISCARD MUNI_API muniResult muniInitGraphemeIterator(muniSegmentIterator* iterator,
                                                                const char* text, size_t length,
                                                                bool moreFollows);

    /// Starts an iterator over the word boundaries of a UTF-8 text or
    /// its first piece.
    ///
    /// @param iterator     The iterator to initialize.
    /// @param text         The text. May be NULL when length is 0.
    /// @param length       The number of bytes.
    /// @param moreFollows  true when more pieces of the text will be fed.
    /// @return `muni_success`, or `muni_errorInvalid` for a NULL iterator
    ///         or a NULL text with a nonzero length.
    /// @par Thread safety
    /// Safe from any thread; an iterator is used by one thread at a time.
    MUNI_NODISCARD MUNI_API muniResult muniInitWordIterator(muniSegmentIterator* iterator,
                                                            const char* text, size_t length,
                                                            bool moreFollows);

    /// Starts an iterator over the sentence boundaries of a UTF-8 text or
    /// its first piece.
    ///
    /// @param iterator     The iterator to initialize.
    /// @param text         The text. May be NULL when length is 0.
    /// @param length       The number of bytes.
    /// @param moreFollows  true when more pieces of the text will be fed.
    /// @return `muni_success`, or `muni_errorInvalid` for a NULL iterator
    ///         or a NULL text with a nonzero length.
    /// @par Thread safety
    /// Safe from any thread; an iterator is used by one thread at a time.
    MUNI_NODISCARD MUNI_API muniResult muniInitSentenceIterator(muniSegmentIterator* iterator,
                                                                const char* text, size_t length,
                                                                bool moreFollows);

    /// Starts an iterator over the line break opportunities of a UTF-8
    /// text or its first piece, with the default rules of UAX #14.
    /// Thai, Lao, Khmer and Burmese letters (class SA) are resolved as rule
    /// LB1 says without a dictionary, with no opportunities between them,
    /// unless muniSegmentIterator_SetComplexBreaker supplies a word segmenter.
    ///
    /// @param iterator     The iterator to initialize.
    /// @param text         The text. May be NULL when length is 0.
    /// @param length       The number of bytes.
    /// @param moreFollows  true when more pieces of the text will be fed.
    /// @return `muni_success`, or `muni_errorInvalid` for a NULL iterator
    ///         or a NULL text with a nonzero length.
    /// @par Thread safety
    /// Safe from any thread; an iterator is used by one thread at a time.
    MUNI_NODISCARD MUNI_API muniResult muniInitLineIterator(muniSegmentIterator* iterator,
                                                            const char* text, size_t length,
                                                            bool moreFollows);

    /// Sets the word segmenter a line or word iterator hands runs of SA
    /// text to. Inside such a run its breaks replace the default ones:
    /// line breaking then allows a break between words, and word
    /// segmentation finds whole words instead of single letters. A break
    /// before a combining mark is ignored. A run lies within one piece of
    /// text; a piece boundary inside a run hands the run over in parts.
    /// Without a segmenter, SA text follows rule LB1 of UAX #14 and the
    /// default word rules.
    ///
    /// While the segmenter runs, the iterator is busy: the setters,
    /// muniSegmentIterator_Feed, muniSegmentIterator_NextBreak and
    /// muniSegmentIterator_NextLineBreak called on it return `muni_errorBusy` and change nothing,
    /// so the outer call finds the same breaks as without them. The segmenter must not initialize
    /// the iterator again.
    ///
    /// @param iterator  A line or word iterator not yet asked for a break.
    /// @param breaker   The segmenter, or NULL for the default rules.
    /// @param context   Passed to the segmenter unchanged.
    /// @return `muni_success`; `muni_errorState` for an iterator already
    ///         asked for a break; `muni_errorBusy` when called from inside
    ///         the iterator's word segmenter; `muni_errorInvalid` for a NULL
    ///         iterator or another kind of iterator. `muni_errorInvalid` is
    ///         reported before `muni_errorBusy`, and that before
    ///         `muni_errorState`.
    /// @par Thread safety
    /// Safe from any thread; an iterator is used by one thread at a time,
    /// and calls the segmenter on that thread.
    MUNI_NODISCARD MUNI_API muniResult muniSegmentIterator_SetComplexBreaker(
        muniSegmentIterator* iterator, muniComplexBreakFunction breaker, void* context);

    // A code point and the class a word or sentence iterator gives it in
    // place of its Word_Break or Sentence_Break value, as a language's
    // conventions may: Greek ends a sentence at U+003B and U+037E
    // (muni_sbSTerm), and POSIX English keeps U+002E out of words
    // (muni_wbMidNum).
    //
    // value holds a muniWordBreak or a muniSentenceBreak value, so it has
    // their common type, int32_t. The struct has no field that says which:
    // the iterator the table is given to does. A word iterator reads each
    // value as a muniWordBreak and a sentence iterator as a
    // muniSentenceBreak, and each refuses a value outside its own kind,
    // negative ones included.
    typedef struct muniBreakClassOverride
    {
        uint32_t codePoint; // at most U+10FFFF, not a surrogate
        int32_t value;      // a muniWordBreak or muniSentenceBreak value
    } muniBreakClassOverride;

    /// Gives code points other classes in a word iterator (Word_Break
    /// values) or a sentence iterator (Sentence_Break values). The rules
    /// then treat each such code point as a code point of its new class;
    /// what they read from other properties, such as Extended_Pictographic
    /// and Line_Break=SA, stays as Unicode assigns it. The table is read in
    /// place, so it must stay unchanged while the iterator is used.
    ///
    /// @param iterator   A word or sentence iterator not yet asked for a
    ///                   boundary.
    /// @param overrides  The code points and their classes, in strictly
    ///                   increasing order of code point. May be NULL when
    ///                   count is 0.
    /// @param count      The number of overrides, at most 0x10F800 (one
    ///                   per scalar value); 0 restores the classes Unicode
    ///                   assigns.
    /// @return `muni_success`; `muni_errorState` for an iterator already
    ///         asked for a boundary; `muni_errorInvalid` for a NULL
    ///         iterator, NULL overrides with a nonzero count, another kind
    ///         of iterator, a count over 0x10F800, a code point past
    ///         U+10FFFF or a surrogate, code points out of order or
    ///         repeated, or a value that is not a class of the iterator's
    ///         property; `muni_errorBusy` when called from inside the
    ///         iterator's word segmenter. `muni_errorInvalid` is reported
    ///         before `muni_errorBusy`, and that before `muni_errorState`.
    /// @par Thread safety
    /// Safe from any thread; an iterator and its overrides are used by one
    /// thread at a time.
    MUNI_NODISCARD MUNI_API muniResult muniSegmentIterator_SetBreakClassOverrides(
        muniSegmentIterator* iterator, const muniBreakClassOverride* overrides, size_t count);

    /// Sets how strictly a line iterator keeps text together. A new
    /// iterator is muni_lineBreakNormal.
    ///
    /// @param iterator    A line iterator not yet asked for a break.
    /// @param strictness  One of the muni_lineBreak values.
    /// @return `muni_success`; `muni_errorState` for an iterator already
    ///         asked for a break; `muni_errorBusy` when called from inside
    ///         the iterator's word segmenter; `muni_errorInvalid` for a NULL
    ///         iterator, another kind of iterator or an unknown strictness.
    ///         `muni_errorInvalid` is reported before `muni_errorBusy`, and
    ///         that before `muni_errorState`.
    /// @par Thread safety
    /// Safe from any thread; an iterator is used by one thread at a time.
    MUNI_NODISCARD MUNI_API muniResult muniSegmentIterator_SetLineBreakStrictness(
        muniSegmentIterator* iterator, muniLineBreakStrictness strictness);

    /// Returns how strictly a line iterator keeps text together.
    ///
    /// @param iterator  The iterator.
    /// @return The strictness muniSegmentIterator_SetLineBreakStrictness
    ///         set, muni_lineBreakNormal when none was set, and for a NULL
    ///         iterator or another kind of iterator.
    /// @par Thread safety
    /// Safe from any thread; an iterator is used by one thread at a time.
    MUNI_API muniLineBreakStrictness
    muniSegmentIterator_GetLineBreakStrictness(const muniSegmentIterator* iterator);

    /// Sets the writing system of the text a line iterator breaks. A new
    /// iterator is muni_writingOther.
    ///
    /// @param iterator       A line iterator not yet asked for a break.
    /// @param writingSystem  One of the muni_writing values.
    /// @return `muni_success`; `muni_errorState` for an iterator already
    ///         asked for a break; `muni_errorBusy` when called from inside
    ///         the iterator's word segmenter; `muni_errorInvalid` for a NULL
    ///         iterator, another kind of iterator or an unknown writing
    ///         system. `muni_errorInvalid` is reported before
    ///         `muni_errorBusy`, and that before `muni_errorState`.
    /// @par Thread safety
    /// Safe from any thread; an iterator is used by one thread at a time.
    MUNI_NODISCARD MUNI_API muniResult muniSegmentIterator_SetWritingSystem(
        muniSegmentIterator* iterator, muniWritingSystem writingSystem);

    /// Returns the writing system of the text a line iterator breaks.
    ///
    /// @param iterator  The iterator.
    /// @return The writing system muniSegmentIterator_SetWritingSystem set,
    ///         muni_writingOther when none was set, and for a NULL iterator
    ///         or another kind of iterator.
    /// @par Thread safety
    /// Safe from any thread; an iterator is used by one thread at a time.
    MUNI_API muniWritingSystem
    muniSegmentIterator_GetWritingSystem(const muniSegmentIterator* iterator);

    /// Hands an iterator the next piece of its text, after it returned
    /// muni_needMoreText. The iterator keeps no pointer to earlier pieces.
    ///
    /// @param iterator     The iterator.
    /// @param text         The next piece. May be NULL when length is 0.
    /// @param length       The number of bytes.
    /// @param moreFollows  true when still more pieces will be fed.
    /// @return `muni_success`; `muni_errorState` for an iterator that did
    ///         not ask for more text, such as one initialized without more
    ///         text to follow or one fed before it asked; `muni_errorBusy`
    ///         when called from inside the iterator's word segmenter;
    ///         `muni_errorInvalid` for a NULL argument. `muni_errorInvalid`
    ///         is reported before `muni_errorBusy`, and that before
    ///         `muni_errorState`.
    /// @par Thread safety
    /// Safe from any thread; an iterator is used by one thread at a time.
    MUNI_NODISCARD MUNI_API muniResult muniSegmentIterator_Feed(muniSegmentIterator* iterator,
                                                                const char* text, size_t length,
                                                                bool moreFollows);

    /// Finds the next boundary.
    ///
    /// @param iterator   The iterator.
    /// @param offsetOut  Receives the boundary's byte offset from the start
    ///                   of the whole text. Left as it was unless the call
    ///                   returns `muni_success`.
    /// @return `muni_success` with a boundary; `muni_done` after the last
    ///         one; `muni_needMoreText` when the next boundary depends on
    ///         text not yet fed; `muni_errorBusy` when called from inside
    ///         the iterator's word segmenter, and then the iterator is left
    ///         as it was; `muni_errorInvalid` for a NULL argument.
    /// @par Thread safety
    /// Safe from any thread; an iterator is used by one thread at a time.
    MUNI_NODISCARD MUNI_API muniResult muniSegmentIterator_NextBreak(muniSegmentIterator* iterator,
                                                                     size_t* offsetOut);

    /// Finds the next line break opportunity of a line iterator, and
    /// whether the line must end there: after a mandatory break character
    /// (BK, CR, LF or NL) and at the end of the text.
    ///
    /// @param iterator      An iterator from muniInitLineIterator.
    /// @param offsetOut     Receives the opportunity's byte offset from the
    ///                      start of the whole text. Left as it was unless
    ///                      the call returns `muni_success`.
    /// @param mandatoryOut  Receives true when the break is mandatory. It
    ///                      is false after `muni_done` and
    ///                      `muni_needMoreText`; `muni_errorBusy` and
    ///                      `muni_errorInvalid` leave it as it was.
    /// @return As muniSegmentIterator_NextBreak; `muni_errorInvalid` also for an
    ///         iterator of another kind.
    /// @par Thread safety
    /// Safe from any thread; an iterator is used by one thread at a time.
    MUNI_NODISCARD MUNI_API muniResult muniSegmentIterator_NextLineBreak(
        muniSegmentIterator* iterator, size_t* offsetOut, bool* mandatoryOut);

    /// Writes the grapheme cluster boundaries of a whole UTF-8 text into a caller
    /// array: every boundary after the start, the end included.
    ///
    /// @param text        The text. May be NULL when length is 0.
    /// @param length      The number of bytes.
    /// @param offsetsOut  The output. May be NULL when capacity is 0.
    /// @param capacity    The number of offsets the output can hold.
    /// @param countOut    Receives the number of boundaries, which may exceed
    ///                    capacity; the ones that fit are written.
    ///                    `muni_errorInvalid` leaves both as they were.
    /// @return `muni_success`, `muni_errorCapacity` when they do not all
    ///         fit, or `muni_errorInvalid` for a NULL argument.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_NODISCARD MUNI_API muniResult muniFindGraphemeBreaks(const char* text, size_t length,
                                                              size_t* offsetsOut, size_t capacity,
                                                              size_t* countOut);

    /// Writes the word boundaries of a whole UTF-8 text into a caller
    /// array: every boundary after the start, the end included.
    ///
    /// @param text        The text. May be NULL when length is 0.
    /// @param length      The number of bytes.
    /// @param offsetsOut  The output. May be NULL when capacity is 0.
    /// @param capacity    The number of offsets the output can hold.
    /// @param countOut    Receives the number of boundaries, which may exceed
    ///                    capacity; the ones that fit are written.
    ///                    `muni_errorInvalid` leaves both as they were.
    /// @return `muni_success`, `muni_errorCapacity` when they do not all
    ///         fit, or `muni_errorInvalid` for a NULL argument.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_NODISCARD MUNI_API muniResult muniFindWordBreaks(const char* text, size_t length,
                                                          size_t* offsetsOut, size_t capacity,
                                                          size_t* countOut);

    /// Writes the sentence boundaries of a whole UTF-8 text into a caller
    /// array: every boundary after the start, the end included.
    ///
    /// @param text        The text. May be NULL when length is 0.
    /// @param length      The number of bytes.
    /// @param offsetsOut  The output. May be NULL when capacity is 0.
    /// @param capacity    The number of offsets the output can hold.
    /// @param countOut    Receives the number of boundaries, which may exceed
    ///                    capacity; the ones that fit are written.
    ///                    `muni_errorInvalid` leaves both as they were.
    /// @return `muni_success`, `muni_errorCapacity` when they do not all
    ///         fit, or `muni_errorInvalid` for a NULL argument.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_NODISCARD MUNI_API muniResult muniFindSentenceBreaks(const char* text, size_t length,
                                                              size_t* offsetsOut, size_t capacity,
                                                              size_t* countOut);

    /// Writes the line break opportunities of a whole UTF-8 text into
    /// caller arrays: every opportunity after the start, the end included,
    /// and optionally whether each is mandatory.
    ///
    /// @param text          The text. May be NULL when length is 0.
    /// @param length        The number of bytes.
    /// @param offsetsOut    The output. May be NULL when capacity is 0.
    /// @param mandatoryOut  NULL, or an array of capacity flags that receive
    ///                      true for each mandatory break.
    /// @param capacity      The number of entries the outputs can hold.
    /// @param countOut      Receives the number of opportunities, which may
    ///                      exceed capacity; the ones that fit are written.
    ///                      `muni_errorInvalid` leaves the offsets, the flags
    ///                      and the count as they were.
    /// @return `muni_success`, `muni_errorCapacity` when they do not all
    ///         fit, or `muni_errorInvalid` for a NULL argument.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_NODISCARD MUNI_API muniResult muniFindLineBreaks(const char* text, size_t length,
                                                          size_t* offsetsOut, bool* mandatoryOut,
                                                          size_t capacity, size_t* countOut);

#ifdef __cplusplus
}
#endif

#endif // MAUL_UNICODE_SEGMENT_H
