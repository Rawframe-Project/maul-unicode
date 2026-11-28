// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The library's side of the instruction count comparison with ICU4C
// (icu4c.cpp) and ICU4X (icu4x/): ours OPERATION FILE PASSES runs one
// operation over the text of FILE PASSES times and prints the sum of
// what each pass counted, which the three programs agree on.
// measure.sh says how it is built and run.

#include "maul-unicode/bidi.h"
#include "maul-unicode/case.h"
#include "maul-unicode/normalize.h"
#include "maul-unicode/script.h"
#include "maul-unicode/segment.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BREAKS (1u << 17)
#define MAX_RUNS   4096u

static char* s_text;
static size_t s_length;
static char* s_out;
static size_t s_outSize;
static uint8_t* s_levels;
static uint8_t* s_workspace;
static size_t s_offsets[MAX_BREAKS];
static bool s_mandatory[MAX_BREAKS];

typedef muniResult (*InitFn)(muniSegmentIterator*, const char*, size_t, bool);

// Counts the boundaries one iterator reports, a call each.
static size_t Iterate(InitFn init)
{
    muniSegmentIterator iterator;
    if (init(&iterator, s_text, s_length, false) != muni_success)
    {
        return 0;
    }
    size_t count = 0;
    size_t offset;
    while (muniSegmentIterator_NextBreak(&iterator, &offset) == muni_success)
    {
        count += 1;
    }
    return count;
}

static size_t Graphemes(void)
{
    return Iterate(muniInitGraphemeIterator);
}

static size_t Words(void)
{
    return Iterate(muniInitWordIterator);
}

static size_t Sentences(void)
{
    return Iterate(muniInitSentenceIterator);
}

static size_t Lines(void)
{
    muniSegmentIterator iterator;
    if (muniInitLineIterator(&iterator, s_text, s_length, false) != muni_success)
    {
        return 0;
    }
    size_t count = 0;
    size_t offset;
    bool mandatory;
    while (muniSegmentIterator_NextLineBreak(&iterator, &offset, &mandatory) == muni_success)
    {
        count += 1;
    }
    return count;
}

typedef muniResult (*FindFn)(const char*, size_t, size_t*, size_t, size_t*);

// Counts the boundaries an array convenience writes, all in one call.
static size_t Find(FindFn find)
{
    size_t count = 0;
    return find(s_text, s_length, s_offsets, MAX_BREAKS, &count) == muni_success ? count : 0;
}

static size_t FoundGraphemes(void)
{
    return Find(muniFindGraphemeBreaks);
}

static size_t FoundWords(void)
{
    return Find(muniFindWordBreaks);
}

static size_t FoundSentences(void)
{
    return Find(muniFindSentenceBreaks);
}

static size_t FoundLines(void)
{
    size_t count = 0;
    return muniFindLineBreaks(s_text, s_length, s_offsets, s_mandatory, MAX_BREAKS, &count) ==
                   muni_success
               ? count
               : 0;
}

static size_t Normalize(muniNormalForm form)
{
    size_t needed = 0;
    muniTextResult result =
        muniNormalize(s_text, s_length, form, muni_convertReplace, s_out, s_outSize, &needed);
    return result.status == muni_success ? needed : 0;
}

static size_t Nfc(void)
{
    return Normalize(muni_nfc);
}

static size_t Nfd(void)
{
    return Normalize(muni_nfd);
}

static size_t Nfkc(void)
{
    return Normalize(muni_nfkc);
}

static size_t ConvertCase(muniCaseOperation operation)
{
    size_t needed = 0;
    muniTextResult result = muniConvertCase(s_text, s_length, operation, muni_caseDefault,
                                            muni_convertReplace, s_out, s_outSize, &needed);
    return result.status == muni_success ? needed : 0;
}

static size_t Lower(void)
{
    return ConvertCase(muni_caseLower);
}

static size_t Upper(void)
{
    return ConvertCase(muni_caseUpper);
}

static size_t Fold(void)
{
    return ConvertCase(muni_caseFold);
}

// Resolves each paragraph and orders it as one line; counts the runs.
static size_t Bidi(void)
{
    static muniBidiRun runs[MAX_RUNS];
    size_t total = 0;
    size_t offset = 0;
    while (offset < s_length)
    {
        size_t length = 0;
        size_t count = 0;
        uint8_t level = 0;
        if (muniResolveBidi(s_text + offset, s_length - offset, muni_bidiAuto, s_levels + offset,
                            s_workspace, &length, &level) != muni_success ||
            muniReorderBidiLine(s_text + offset, s_levels + offset, length, level, runs, MAX_RUNS,
                                &count) != muni_success)
        {
            return 0;
        }
        total += count;
        offset += length;
    }
    return total;
}

// Counts the runs with no room for them, which still finds them all.
static size_t Scripts(void)
{
    size_t count = 0;
    muniResult status = muniFindScriptRuns(s_text, s_length, nullptr, 0, &count);
    return status == muni_success || status == muni_errorCapacity ? count : 0;
}

typedef struct Operation
{
    const char* name;
    size_t (*run)(void);
} Operation;

static const Operation s_operations[] = {
    {"grapheme", Graphemes},
    {"word", Words},
    {"sentence", Sentences},
    {"line", Lines},
    {"grapheme-find", FoundGraphemes},
    {"word-find", FoundWords},
    {"sentence-find", FoundSentences},
    {"line-find", FoundLines},
    {"nfc", Nfc},
    {"nfd", Nfd},
    {"nfkc", Nfkc},
    {"lower", Lower},
    {"upper", Upper},
    {"fold", Fold},
    {"bidi", Bidi},
    {"scripts", Scripts},
};

static bool ReadText(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        return false;
    }
    bool read = fseek(file, 0, SEEK_END) == 0;
    long size = read ? ftell(file) : -1;
    read = size >= 0 && fseek(file, 0, SEEK_SET) == 0;
    s_length = read ? (size_t)size : 0;
    s_text = read ? malloc(s_length + 1) : nullptr;
    read = s_text != nullptr && fread(s_text, 1, s_length, file) == s_length;
    fclose(file);
    return read;
}

int main(int argc, char** argv)
{
    const Operation* operation = nullptr;
    for (size_t i = 0; argc == 4 && i < sizeof(s_operations) / sizeof(s_operations[0]); i++)
    {
        operation = strcmp(argv[1], s_operations[i].name) == 0 ? &s_operations[i] : operation;
    }
    if (operation == nullptr || !ReadText(argv[2]))
    {
        fprintf(stderr, "usage: ours OPERATION FILE PASSES\n");
        return 2;
    }
    s_outSize = s_length * 4 + 64;
    s_out = malloc(s_outSize);
    s_levels = malloc(s_length + 1);
    s_workspace = malloc(s_length + 1);
    if (s_out == nullptr || s_levels == nullptr || s_workspace == nullptr)
    {
        return 2;
    }
    long passes = strtol(argv[3], nullptr, 10);
    size_t sum = 0;
    for (long i = 0; i < passes; i++)
    {
        sum += operation->run();
    }
    printf("%zu\n", sum);
    return 0;
}
