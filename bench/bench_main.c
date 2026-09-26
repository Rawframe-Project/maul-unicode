// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Throughput of validation, segmentation, line breaking, bidi, script
// runs and normalization over a mixed text: English, Turkish, Hindi,
// Japanese, emoji, Hebrew and Arabic, repeated to 4 MiB. Prints the best
// of five runs in MiB per second.

#include "maul-unicode/bidi.h"
#include "maul-unicode/encoding.h"
#include "maul-unicode/normalize.h"
#include "maul-unicode/script.h"
#include "maul-unicode/segment.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define TEXT_BYTES (4u << 20)

static char s_text[TEXT_BYTES];
static uint8_t s_levels[TEXT_BYTES];
static uint8_t s_workspace[TEXT_BYTES];
static char s_normalized[TEXT_BYTES * 3];

static const char s_sample[] =
    "The quick brown fox can't jump 3.14 metres, etc. and so on. Next sentence! "
    "\xC4\xB0stanbul'da ya\xC4\x9Fmur ya\xC4\x9F\xC4\xB1yor. "
    "\xE0\xA4\xA8\xE0\xA4\xAE\xE0\xA4\xB8\xE0\xA5\x8D\xE0\xA4\xA4\xE0\xA5\x87 "
    "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7\xE0\xA4\xBF\xE0\xA4\xA4\xE0\xA4\xBF\xE0\xA5\xA4 "
    "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\xE3\x81\xAE\xE6\x96\x87\xE3\x80\x82 "
    "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7 "
    "\xF0\x9F\x87\xB9\xF0\x9F\x87\xB7 "
    "\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D (\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x8A 42).\n";

static double Seconds(void)
{
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
}

typedef muniResult (*InitFn)(muniSegmentIterator*, const char*, size_t, bool);

static size_t CountBreaks(InitFn init)
{
    muniSegmentIterator iterator;
    if (init(&iterator, s_text, TEXT_BYTES, false) != muni_success)
    {
        return 0;
    }
    size_t count = 0;
    size_t offset;
    while (muniNextSegmentBreak(&iterator, &offset) == muni_success)
    {
        count += 1;
    }
    return count;
}

typedef muniResult (*FindFn)(const char*, size_t, size_t*, size_t, size_t*);

// The array conveniences, measured through their count: with no room for
// offsets they still find every boundary.
static size_t CountFound(FindFn find)
{
    size_t count = 0;
    (void)find(s_text, TEXT_BYTES, nullptr, 0, &count);
    return count;
}

// Resolves every paragraph and orders each as one line; returns the
// number of runs.
static size_t ResolveBidi(InitFn unused)
{
    (void)unused;
    static muniBidiRun runs[4096];
    size_t total = 0;
    size_t offset = 0;
    while (offset < TEXT_BYTES)
    {
        size_t length = 0;
        uint8_t level = 0;
        if (muniResolveBidi(s_text + offset, TEXT_BYTES - offset, muni_bidiAuto, s_levels + offset,
                            s_workspace, &length, &level) != muni_success)
        {
            return 0;
        }
        size_t count = 0;
        if (muniReorderBidiLine(s_text + offset, s_levels + offset, length, level, runs, 4096,
                                &count) != muni_success)
        {
            return 0;
        }
        total += count;
        offset += length;
    }
    return total;
}

static size_t CountScriptRuns(InitFn unused)
{
    (void)unused;
    size_t count = 0;
    (void)muniFindScriptRuns(s_text, TEXT_BYTES, nullptr, 0, &count);
    return count;
}

static size_t Normalize(muniNormalForm form)
{
    size_t needed = 0;
    muniTextResult result = muniNormalize(s_text, TEXT_BYTES, form, muni_convertReplace,
                                          s_normalized, sizeof(s_normalized), &needed);
    return result.status == muni_success ? needed : 0;
}

static size_t NormalizeNfc(InitFn unused)
{
    (void)unused;
    return Normalize(muni_nfc);
}

static size_t NormalizeNfd(InitFn unused)
{
    (void)unused;
    return Normalize(muni_nfd);
}

static size_t CheckNfc(InitFn unused)
{
    (void)unused;
    muniQuickCheck answer = muni_quickCheckNo;
    (void)muniCheckNormalization(s_text, TEXT_BYTES, muni_nfc, &answer);
    return answer;
}

static size_t Validate(InitFn unused)
{
    (void)unused;
    return muniValidateUtf8(s_text, TEXT_BYTES).offset;
}

static void Report(const char* name, double best, size_t result)
{
    double mib = (double)TEXT_BYTES / (1024.0 * 1024.0);
    printf("%-26s %9.1f MiB/s  (%zu)\n", name, mib / best, result);
}

static void RunFind(const char* name, FindFn find)
{
    double best = 1e30;
    size_t result = 0;
    for (int run = 0; run < 5; run++)
    {
        double start = Seconds();
        result = CountFound(find);
        double elapsed = Seconds() - start;
        best = elapsed < best ? elapsed : best;
    }
    Report(name, best, result);
}

static muniResult FindLines(const char* text, size_t length, size_t* offsets, size_t capacity,
                            size_t* countOut)
{
    return muniFindLineBreaks(text, length, offsets, nullptr, capacity, countOut);
}

static void Run(const char* name, size_t (*work)(InitFn), InitFn init)
{
    double best = 1e30;
    size_t result = 0;
    for (int run = 0; run < 5; run++)
    {
        double start = Seconds();
        result = work(init);
        double elapsed = Seconds() - start;
        best = elapsed < best ? elapsed : best;
    }
    Report(name, best, result);
}

int main(void)
{
    size_t sampleLength = sizeof(s_sample) - 1;
    size_t filled = 0;
    while (filled + sampleLength <= TEXT_BYTES)
    {
        memcpy(s_text + filled, s_sample, sampleLength);
        filled += sampleLength;
    }
    memset(s_text + filled, ' ', TEXT_BYTES - filled);
    Run("validate UTF-8", Validate, nullptr);
    Run("grapheme boundaries", CountBreaks, muniInitGraphemeIterator);
    Run("word boundaries", CountBreaks, muniInitWordIterator);
    Run("sentence boundaries", CountBreaks, muniInitSentenceIterator);
    Run("line breaks", CountBreaks, muniInitLineIterator);
    Run("bidi, resolve and order", ResolveBidi, nullptr);
    Run("script runs, find", CountScriptRuns, nullptr);
    Run("NFC quick check", CheckNfc, nullptr);
    Run("NFC", NormalizeNfc, nullptr);
    Run("NFD", NormalizeNfd, nullptr);
    RunFind("grapheme boundaries, find", muniFindGraphemeBreaks);
    RunFind("word boundaries, find", muniFindWordBreaks);
    RunFind("sentence boundaries, find", muniFindSentenceBreaks);
    RunFind("line breaks, find", FindLines);
    return 0;
}
