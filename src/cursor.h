// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A reading position in UTF-8 text that may arrive in pieces. It decodes
// one code point at a time, keeps the bytes of a sequence that a piece
// cuts off until the next piece completes it, and counts offsets from
// the start of the whole text.

#ifndef MAUL_UNICODE_SRC_CURSOR_H
#define MAUL_UNICODE_SRC_CURSOR_H

#include "maul-unicode/base.h"

typedef struct muniCursor
{
    const uint8_t* text;  // the current piece
    size_t length;        // its length in bytes
    size_t position;      // the next unread byte of the piece
    size_t offset;        // the offset in the whole text of the next code point
    uint8_t pending[4];   // the start of a sequence the previous piece cut off
    uint8_t pendingCount; // how many bytes of it there are
    bool moreFollows;     // whether more pieces will come
} muniCursor;

void muniCursorInit(muniCursor* cursor, const char* text, size_t length, bool moreFollows);

// Hands the cursor its next piece. Bytes still unread in the current
// piece are kept as pending; there are at most three.
void muniCursorFeed(muniCursor* cursor, const char* text, size_t length, bool moreFollows);

// Decodes the next code point without consuming it. Returns muni_success
// with the code point and its size in bytes, muni_done at the end of the
// whole text, or muni_needMoreText when the piece ends before the code
// point does and more pieces will come. An ill-formed sequence decodes as
// U+FFFD over its maximal subpart.
muniResult muniCursorPeek(const muniCursor* cursor, uint32_t* codePointOut, size_t* sizeOut);

// Consumes the code point the last successful peek decoded.
void muniCursorAdvance(muniCursor* cursor, size_t size);

#endif // MAUL_UNICODE_SRC_CURSOR_H
