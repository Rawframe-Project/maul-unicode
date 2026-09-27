// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Splits text the way an editor and a layout engine see it: the
// characters a user perceives (grapheme clusters), which a caret steps
// over; words; sentences; and where a line may, or must, break. Give the
// text as the first argument; without one, a sample with an emoji
// family, a flag, a combining accent and Hindi is used.
//
// Untrusted text is validated first: the iterators take any bytes, but
// a program should know where its input stops being UTF-8.

#include "maul-unicode/encoding.h"
#include "maul-unicode/segment.h"

#include <stdio.h>
#include <string.h>

typedef muniResult InitFn(muniSegmentIterator* iterator, const char* text, size_t length,
                          bool moreFollows);

// Prints the text with a bar at every boundary an iterator finds.
static void Show(const char* title, InitFn* init, const char* text, size_t length)
{
    muniSegmentIterator iterator;
    if (init(&iterator, text, length, false) != muni_success)
    {
        return;
    }
    printf("%-10s |", title);
    size_t start = 0;
    size_t offset = 0;
    while (muniNextSegmentBreak(&iterator, &offset) == muni_success)
    {
        printf("%.*s|", (int)(offset - start), text + start);
        start = offset;
    }
    printf("\n");
}

// Prints each stretch between line break opportunities on a row of its
// own, saying whether the line must end after it.
static void ShowLines(const char* text, size_t length)
{
    muniSegmentIterator iterator;
    if (muniInitLineIterator(&iterator, text, length, false) != muni_success)
    {
        return;
    }
    printf("lines\n");
    size_t start = 0;
    size_t offset = 0;
    bool mandatory = false;
    while (muniNextLineBreak(&iterator, &offset, &mandatory) == muni_success)
    {
        // Trailing whitespace and newlines stay with the stretch before
        // the break; print them visibly.
        size_t shown = offset;
        while (shown > start && (text[shown - 1] == '\n' || text[shown - 1] == ' '))
        {
            shown -= 1;
        }
        printf("  %s \"%.*s\"\n", mandatory ? "must break after" : "may break after ",
               (int)(shown - start), text + start);
        start = offset;
    }
}

int main(int argc, char** argv)
{
    const char* text = argc > 1 ? argv[1]
                                : "Family: \xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80"
                                  "\x8D\xF0\x9F\x91\xA7 flag: \xF0\x9F\x87\xB9\xF0\x9F\x87\xB7 "
                                  "cafe\xCC\x81. \xE0\xA4\xA8\xE0\xA4\xAE\xE0\xA4\xB8\xE0\xA5"
                                  "\x8D\xE0\xA4\xA4\xE0\xA5\x87!\nNext line.";
    size_t length = strlen(text);
    muniTextResult valid = muniValidateUtf8(text, length);
    if (valid.status != muni_success)
    {
        printf("not UTF-8: %s at byte %zu\n", muniResultName(valid.status), valid.offset);
        return 1;
    }
    Show("clusters", muniInitGraphemeIterator, text, length);
    Show("words", muniInitWordIterator, text, length);
    Show("sentences", muniInitSentenceIterator, text, length);
    ShowLines(text, length);
    return 0;
}
