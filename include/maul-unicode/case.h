// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Case mapping and case folding (Unicode chapter 3.13): lowercase,
// uppercase and titlecase, and folding for comparing text without
// regard to case.
//
// Per code point, the simple mappings give one code point for one. Over
// text, the full mappings may change the length ("ß" uppercases to
// "SS"), a final sigma lowercases to "ς", and titlecasing starts each
// word (UAX #29) with a capital. Turkish and Azerbaijani, which pair a
// dotted and a dotless i, take muni_caseTurkic; Lithuanian, which keeps
// the dot of an i under other accents, takes muni_caseLithuanian; Greek,
// whose capitals drop their accents, takes muni_caseGreek; Armenian,
// whose ligature "և" uppercases to "ԵՎ", takes muni_caseArmenian.

#ifndef MAUL_UNICODE_CASE_H
#define MAUL_UNICODE_CASE_H

#include "maul-unicode/encoding.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // A case operation over text.
    typedef int32_t muniCaseOperation;

    enum
    {
        muni_caseLower = 0,
        muni_caseUpper = 1,
        // The first cased character of each word to titlecase, the rest
        // to lowercase.
        muni_caseTitle = 2,
        // Full case folding, for caseless comparison.
        muni_caseFold = 3,
    };

    // Language rules for case operations.
    typedef int32_t muniCaseLanguage;

    enum
    {
        // The default rules, for every language but those below.
        muni_caseDefault = 0,
        // Turkish and Azerbaijani: "i" uppercases to "İ" and "I"
        // lowercases to "ı".
        muni_caseTurkic = 1,
        // Lithuanian: an i keeps its dot under other accents above, as
        // "i̇̀"; lowercasing writes the dot out and uppercasing drops it.
        muni_caseLithuanian = 2,
        // Greek: uppercasing drops the accents and breathings, keeps a
        // dialytika and adds one where an accent removed from the first
        // vowel of a diphthong kept the vowels apart ("άι" to "ΑΪ"), and
        // keeps the accent of the disjunctive eta "ή" standing alone; an
        // iota subscript becomes a capital iota. The eta stands alone when
        // no cased letter precedes or follows it past case-ignorable
        // characters, the context of the final sigma; a character both
        // cased and case-ignorable, such as U+0345 or U+037A, counts as a
        // cased letter there, where ICU skips it as case-ignorable.
        // Lowercasing, titlecasing and folding follow the default rules.
        muni_caseGreek = 3,
        // Armenian: the ligature "և" (U+0587) uppercases to "ԵՎ" and
        // titlecases to "Եվ", where the default rules give "ԵՒ" and "Եւ".
        muni_caseArmenian = 4,
    };

    /// Returns the simple lowercase mapping of a code point.
    ///
    /// @param codePoint  Any value.
    /// @return The lowercase code point, or codePoint when it has none.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_API uint32_t muniGetLowercase(uint32_t codePoint);

    /// Returns the simple uppercase mapping of a code point.
    ///
    /// @param codePoint  Any value.
    /// @return The uppercase code point, or codePoint when it has none.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_API uint32_t muniGetUppercase(uint32_t codePoint);

    /// Returns the simple titlecase mapping of a code point, which differs
    /// from the uppercase one for digraphs such as "ǆ", which titlecases
    /// to "ǅ".
    ///
    /// @param codePoint  Any value.
    /// @return The titlecase code point, or codePoint when it has none.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_API uint32_t muniGetTitlecase(uint32_t codePoint);

    /// Returns the simple case folding of a code point (statuses C and S
    /// of CaseFolding.txt).
    ///
    /// @param codePoint  Any value.
    /// @return The folded code point, or codePoint when it folds to itself.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_API uint32_t muniFoldCase(uint32_t codePoint);

    /// Returns whether a code point is Cased (a letter with case, or one
    /// such as "ª" that counts as one).
    ///
    /// @param codePoint  Any value.
    /// @return true when it is Cased.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_API bool muniIsCased(uint32_t codePoint);

    /// Applies a full case operation to UTF-8 text.
    ///
    /// @param text       The text. May be NULL when length is 0.
    /// @param length     The number of bytes.
    /// @param operation  muni_caseLower, muni_caseUpper, muni_caseTitle or
    ///                   muni_caseFold.
    /// @param language   muni_caseDefault, muni_caseTurkic,
    ///                   muni_caseLithuanian, muni_caseGreek or
    ///                   muni_caseArmenian.
    /// @param mode       What to do with ill-formed input.
    /// @param textOut    The output. May be NULL when capacity is 0.
    /// @param capacity   The number of bytes textOut can hold.
    /// @param neededOut  Receives the number of bytes the whole result needs,
    ///                   which may exceed capacity, or SIZE_MAX when that does
    ///                   not fit in a size_t. On another error, the bytes
    ///                   before it, which textOut holds as far as they fit.
    ///                   `muni_errorInvalid` leaves both as they were.
    /// @return `muni_success` and the length; `muni_errorCapacity` when
    ///         the result does not fit (the bytes that fit are written); in
    ///         strict mode the first UTF-8 error and its offset;
    ///         `muni_errorInvalid` for a NULL argument, an unknown
    ///         operation, language or mode, or a textOut that overlaps
    ///         text.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_NODISCARD MUNI_API muniTextResult muniConvertCase(const char* text, size_t length,
                                                           muniCaseOperation operation,
                                                           muniCaseLanguage language,
                                                           muniConvertMode mode, char* textOut,
                                                           size_t capacity, size_t* neededOut);

    /// Maps UTF-8 text to NFKC_Casefold: compatibility forms to their
    /// plain selves, every case to one, default ignorables removed, and
    /// the result in NFC. Two strings match without regard to case or
    /// compatibility forms when their NFKC_Casefold texts are equal;
    /// UAX #31 uses it to compare identifiers. It needs both the case
    /// and the normalization components.
    ///
    /// @param text       The text. May be NULL when length is 0.
    /// @param length     The number of bytes.
    /// @param mode       What to do with ill-formed input.
    /// @param textOut    The output. May be NULL when capacity is 0.
    /// @param capacity   The number of bytes textOut can hold.
    /// @param neededOut  Receives the number of bytes the whole result needs,
    ///                   which may exceed capacity, or SIZE_MAX when that does
    ///                   not fit in a size_t. On a UTF-8 error, the bytes
    ///                   before it, which textOut holds as far as they fit. On
    ///                   `muni_errorLimit`, the same for the text before the
    ///                   starter whose run of combining marks hit the limit.
    ///                   `muni_errorInvalid` leaves both as they were.
    /// @return `muni_success` and the length; `muni_errorCapacity` when
    ///         the result does not fit (the bytes that fit are written); in
    ///         strict mode the first UTF-8 error and its offset;
    ///         `muni_errorLimit` and the offset of the code point that
    ///         makes a segment longer than MUNI_MAX_NORMALIZATION_SEGMENT,
    ///         as in muniNormalize; `muni_errorInvalid` for a NULL argument,
    ///         an unknown mode or a textOut that overlaps text.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_NODISCARD MUNI_API muniTextResult muniGetNfkcCasefold(const char* text, size_t length,
                                                               muniConvertMode mode, char* textOut,
                                                               size_t capacity, size_t* neededOut);

    // What two texts must share to match when case is ignored: the
    // caseless matches of the Unicode Standard (section 3.13).
    typedef int32_t muniCaselessMatch;

    enum
    {
        // Default caseless matching, full case folding alone: "Straße"
        // matches "STRASSE", but a composed "é" does not match "e" and a
        // combining acute accent.
        muni_caselessDefault = 0,
        // Canonical caseless matching: the NFD of the folded NFD, so
        // canonically equivalent texts match too.
        muni_caselessCanonical = 1,
        // Identifier caseless matching: NFKC_Casefold, which also ignores
        // compatibility differences (a fullwidth "Ａ" and "a", "①" and
        // "1") and default ignorables, as keys from muniGetNfkcCasefold
        // compare.
        muni_caselessIdentifier = 2,
    };

    /// Compares two UTF-8 texts ignoring case, without folding them into
    /// buffers: the call folds both a segment at a time and stops at the
    /// first difference. To look a text up among many, store keys from
    /// muniGetNfkcCasefold or muniConvertCase instead. Needs the case and
    /// the normalization components.
    ///
    /// @param first         The first text. May be NULL when firstLength is
    ///                      0. Each maximal ill-formed subpart reads as
    ///                      U+FFFD.
    /// @param firstLength   Its length in bytes.
    /// @param second        The second text, read the same way. May be NULL
    ///                      when secondLength is 0.
    /// @param secondLength  Its length in bytes.
    /// @param match         The caseless match, a muni_caseless value.
    /// @param orderOut      Receives 0 when the texts match, and otherwise
    ///                      -1 or 1 as the folded form of the first comes
    ///                      before or after that of the second in code point
    ///                      order, a form that is a prefix of the other
    ///                      first. Left as it was unless the call returns
    ///                      `muni_success`.
    /// @return `muni_success`; `muni_errorLimit` when, in a normalizing
    ///         match, a segment of either text grows past
    ///         MUNI_MAX_NORMALIZATION_SEGMENT code points, as in
    ///         muniNormalize, among the segments read up to the first
    ///         difference, the one that holds it included;
    ///         `muni_errorInvalid` for a NULL
    ///         text with a nonzero length, a NULL orderOut or an unknown
    ///         match.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_NODISCARD MUNI_API muniResult muniCompareCaseless(const char* first, size_t firstLength,
                                                           const char* second, size_t secondLength,
                                                           muniCaselessMatch match,
                                                           int32_t* orderOut);

#ifdef __cplusplus
}
#endif

#endif // MAUL_UNICODE_CASE_H
