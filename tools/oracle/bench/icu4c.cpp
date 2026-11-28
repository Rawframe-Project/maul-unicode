// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C's side of the instruction count comparison: icu4c OPERATION
// FILE PASSES does the work of ours.c with ICU4C 79.1, on UTF-8 where
// ICU takes it (break iterators over a UTF-8 UText, normalizeUTF8 and
// ucasemap_utf8*). Bidi and script runs take UTF-16, converted once
// before the passes. Script runs come from ScriptRun, in ICU's
// source/extra/scrptrun, since ICU's libraries have none. Each break
// iterator, normalizer and case map is made once, before the passes.

#include "scrptrun.h"

#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <unicode/ucasemap.h>
#include <unicode/normalizer2.h>
#include <unicode/bytestream.h>
#include <unicode/ustring.h>
#include <unicode/utext.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace icu;

int main(int argc, char** argv)
{
    if (argc != 4)
        return 2;
    FILE* f = fopen(argv[2], "rb");
    if (!f)
        return 2;
    std::vector<char> text;
    char buffer[65536];
    size_t got;
    while ((got = fread(buffer, 1, sizeof buffer, f)) > 0)
        text.insert(text.end(), buffer, buffer + got);
    fclose(f);
    const char* op = argv[1];
    int reps = atoi(argv[3]);
    int32_t length = (int32_t)text.size();
    std::vector<char> out(text.size() * 4 + 64);
    UErrorCode status = U_ZERO_ERROR;
    size_t sum = 0;

    UBreakIteratorType type = !strcmp(op, "grapheme") ? UBRK_CHARACTER
                              : !strcmp(op, "word")   ? UBRK_WORD
                              : !strcmp(op, "sentence") ? UBRK_SENTENCE
                                                        : UBRK_LINE;
    bool breaks = !strcmp(op, "grapheme") || !strcmp(op, "word") || !strcmp(op, "sentence") ||
                  !strcmp(op, "line");
    if (breaks)
    {
        UBreakIterator* bi = ubrk_open(type, "en", nullptr, 0, &status);
        UText* ut = utext_openUTF8(nullptr, text.data(), length, &status);
        for (int i = 0; i < reps; i++)
        {
            ubrk_setUText(bi, ut, &status);
            while (ubrk_next(bi) != UBRK_DONE)
                sum += 1;
        }
        utext_close(ut);
        ubrk_close(bi);
    }
    else if (!strcmp(op, "nfc") || !strcmp(op, "nfd") || !strcmp(op, "nfkc"))
    {
        const Normalizer2* n = !strcmp(op, "nfc")   ? Normalizer2::getNFCInstance(status)
                               : !strcmp(op, "nfd") ? Normalizer2::getNFDInstance(status)
                                                    : Normalizer2::getNFKCInstance(status);
        for (int i = 0; i < reps; i++)
        {
            CheckedArrayByteSink sink(out.data(), (int32_t)out.size());
            n->normalizeUTF8(0, StringPiece(text.data(), length), sink, nullptr, status);
            sum += (size_t)sink.NumberOfBytesWritten();
        }
    }
    else if (!strcmp(op, "lower") || !strcmp(op, "upper") || !strcmp(op, "fold"))
    {
        UCaseMap* map = ucasemap_open("", 0, &status);
        for (int i = 0; i < reps; i++)
        {
            int32_t n = !strcmp(op, "lower")
                            ? ucasemap_utf8ToLower(map, out.data(), (int32_t)out.size(), text.data(), length, &status)
                        : !strcmp(op, "upper")
                            ? ucasemap_utf8ToUpper(map, out.data(), (int32_t)out.size(), text.data(), length, &status)
                            : ucasemap_utf8FoldCase(map, out.data(), (int32_t)out.size(), text.data(), length, &status);
            sum += (size_t)n;
        }
        ucasemap_close(map);
    }
    else if (!strcmp(op, "bidi") || !strcmp(op, "scripts"))
    {
        std::vector<UChar> u16(text.size() + 1);
        int32_t u16Length = 0;
        u_strFromUTF8(u16.data(), (int32_t)u16.size(), &u16Length, text.data(), length, &status);
        if (!strcmp(op, "bidi"))
        {
            UBiDi* bidi = ubidi_openSized(u16Length, 0, &status);
            for (int i = 0; i < reps; i++)
            {
                ubidi_setPara(bidi, u16.data(), u16Length, UBIDI_DEFAULT_LTR, nullptr, &status);
                sum += (size_t)ubidi_countRuns(bidi, &status);
            }
            ubidi_close(bidi);
        }
        else
        {
            for (int i = 0; i < reps; i++)
            {
                ScriptRun run(u16.data(), 0, u16Length);
                while (run.next())
                    sum += 1;
            }
        }
    }
    else
        return 2;
    if (U_FAILURE(status))
    {
        fprintf(stderr, "%s\n", u_errorName(status));
        return 1;
    }
    printf("%zu\n", sum);
    return 0;
}
