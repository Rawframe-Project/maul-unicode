// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The paragraph and explicit phases of the bidi algorithm: rules P1 to
// P3 and X1 to X9.

#ifndef MAUL_UNICODE_SRC_BIDI_EXPLICIT_H
#define MAUL_UNICODE_SRC_BIDI_EXPLICIT_H

#include "bidi_core.h"

// Rules P2 and P3 from index, within the paragraph: 1 when the first
// strong class outside isolates is R or AL, 0 when it is L, and
// fallback when there is none. With stopAtPdi, the search also ends at
// the PDI that closes an isolate starting just before index. A search
// also ends at a paragraph separator, so a paragraph of the whole text
// gives the answer for its first paragraph. offsetOut, when not NULL,
// receives where the search ended: the strong character's offset, or
// that of the PDI or separator it stopped at, or the length.
uint8_t muniBidiFirstStrongLevel(const muniBidiParagraph* paragraph, size_t index, bool stopAtPdi,
                                 uint8_t fallback, size_t* offsetOut);

// Rules X1 to X9 over the first paragraph of the text paragraph holds:
// writes each code point's explicit level and class, and marks
// continuation bytes and removed code points. The paragraph ends after
// its first paragraph separator, a CR LF pair counting as one (rule P1),
// and paragraph->length is set to its length. Returns false when the
// implicit phase can only give level 0 everywhere: the paragraph level is
// 0 and no code point is R, AL, AN or an explicit formatting character,
// so every class resolves to L. Such a paragraph is found without the
// explicit rules, and nothing is written for it.
MUNI_NODISCARD bool muniBidiResolveExplicit(muniBidiParagraph* paragraph);

#endif // MAUL_UNICODE_SRC_BIDI_EXPLICIT_H
