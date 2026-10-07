// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The cursor's own feeding rules (src/cursor.h), which the iterators'
// waiting flags keep the public API from reaching: a piece is refused,
// changing nothing, when none was announced or when more than a cut-off
// sequence is unread; up to three unread bytes are kept for the next
// piece to complete.

#include "cursor.h"
#include "test_harness.h"

#include <string.h>

static void TestRefusedWithoutAnnouncedPiece(void)
{
    muniCursor cursor;
    muniCursorInit(&cursor, "ab", 2, false);
    CHECK(!muniCursorFeed(&cursor, "c", 1, false), "no piece was announced");
    CHECK(cursor.length == 2 && cursor.position == 0 && cursor.pendingCount == 0,
          "nothing changed");
}

static void TestRefusedWithACodePointUnread(void)
{
    muniCursor cursor;
    muniCursorInit(&cursor, "\xF0\x9F\x98\x80", 4, true);
    CHECK(!muniCursorFeed(&cursor, "a", 1, false), "four bytes unread");
    CHECK(cursor.length == 4 && cursor.pendingCount == 0, "nothing changed");
}

static void TestCutOffSequenceKept(void)
{
    muniCursor cursor;
    muniCursorInit(&cursor, "\xF0\x9F\x98", 3, true);
    CHECK(muniCursorFeed(&cursor, "\x80z", 2, false), "three bytes unread");
    CHECK(cursor.pendingCount == 3 && memcmp(cursor.pending, "\xF0\x9F\x98", 3) == 0 &&
              cursor.position == 0 && cursor.length == 2,
          "kept for the next piece");
    uint32_t codePoint = 0;
    size_t size = 0;
    CHECK(muniCursorPeekSlow(&cursor, &codePoint, &size) == muni_success && codePoint == 0x1F600 &&
              size == 4,
          "completed by it");
}

int main(void)
{
    TestRefusedWithoutAnnouncedPiece();
    TestRefusedWithACodePointUnread();
    TestCutOffSequenceKept();
    return s_failures == 0 ? 0 : 1;
}
