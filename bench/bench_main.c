// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Throughput of validation, segmentation, line breaking, bidi, script
// runs, normalization, case and the security checks over a mixed text: English, Turkish,
// Hindi, Japanese, emoji, Hebrew and Arabic, repeated to 4 MiB. Prints
// the best of five runs in MiB per second. Given a baseline file, such
// as bench/baseline.txt, it prints each speed's ratio to the recorded
// one as well.

#include "maul-unicode/bidi.h"
#include "maul-unicode/case.h"
#include "maul-unicode/encoding.h"
#include "maul-unicode/normalize.h"
#include "maul-unicode/script.h"
#include "maul-unicode/security.h"
#include "maul-unicode/segment.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TEXT_BYTES (4u << 20)

static char s_text[TEXT_BYTES];
static uint8_t s_levels[TEXT_BYTES];
static uint8_t s_workspace[TEXT_BYTES];
static char s_normalized[TEXT_BYTES * 3];
static uint8_t s_skeletonWorkspace[TEXT_BYTES * 2];

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

static size_t ConvertCase(muniCaseOperation operation)
{
    size_t needed = 0;
    muniTextResult result =
        muniConvertCase(s_text, TEXT_BYTES, operation, muni_caseDefault, muni_convertReplace,
                        s_normalized, sizeof(s_normalized), &needed);
    return result.status == muni_success ? needed : 0;
}

static size_t Lowercase(InitFn unused)
{
    (void)unused;
    return ConvertCase(muni_caseLower);
}

static size_t Fold(InitFn unused)
{
    (void)unused;
    return ConvertCase(muni_caseFold);
}

static size_t Skeleton(InitFn unused)
{
    (void)unused;
    size_t needed = 0;
    muniTextResult result =
        muniGetSkeleton(s_text, TEXT_BYTES, muni_bidiLeftToRight, s_skeletonWorkspace, s_normalized,
                        sizeof(s_normalized), &needed);
    return result.status == muni_success ? needed : 0;
}

static size_t RestrictionLevel(InitFn unused)
{
    (void)unused;
    muniRestrictionLevel level = 0;
    (void)muniGetRestrictionLevel(s_text, TEXT_BYTES, &level);
    return level;
}

static size_t Validate(InitFn unused)
{
    (void)unused;
    return muniValidateUtf8(s_text, TEXT_BYTES).offset;
}

#define NAME_WIDTH   26
#define MAX_BASELINE 32

typedef struct Recorded
{
    char name[NAME_WIDTH + 1];
    double speed;
} Recorded;

static Recorded s_baseline[MAX_BASELINE];
static int s_baselineCount = 0;

// Reads a baseline: lines of this program's own output; lines that start
// with # are notes on where the numbers come from.
static void LoadBaseline(const char* path)
{
    FILE* file = fopen(path, "r");
    if (file == nullptr)
    {
        fprintf(stderr, "cannot read %s\n", path);
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), file) != nullptr && s_baselineCount < MAX_BASELINE)
    {
        if (line[0] == '#' || strlen(line) <= NAME_WIDTH)
        {
            continue;
        }
        Recorded* recorded = &s_baseline[s_baselineCount];
        memcpy(recorded->name, line, NAME_WIDTH);
        size_t end = NAME_WIDTH;
        while (end > 0 && recorded->name[end - 1] == ' ')
        {
            end -= 1;
        }
        recorded->name[end] = '\0';
        recorded->speed = strtod(line + NAME_WIDTH, nullptr);
        s_baselineCount += recorded->speed > 0 ? 1 : 0;
    }
    fclose(file);
}

static double BaselineOf(const char* name)
{
    for (int i = 0; i < s_baselineCount; i++)
    {
        if (strcmp(s_baseline[i].name, name) == 0)
        {
            return s_baseline[i].speed;
        }
    }
    return 0;
}

static void Report(const char* name, double best, size_t result)
{
    double speed = (double)TEXT_BYTES / (1024.0 * 1024.0) / best;
    double recorded = BaselineOf(name);
    if (recorded > 0)
    {
        printf("%-26s %9.1f MiB/s  %5.2fx  (%zu)\n", name, speed, speed / recorded, result);
    }
    else
    {
        printf("%-26s %9.1f MiB/s  (%zu)\n", name, speed, result);
    }
}

static void RunFind(const char* name, FindFn find)
{
    double best = 1e30;
    size_t result = CountFound(find); // untimed, to warm up
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
    size_t result = work(init); // untimed, to warm up
    for (int run = 0; run < 5; run++)
    {
        double start = Seconds();
        result = work(init);
        double elapsed = Seconds() - start;
        best = elapsed < best ? elapsed : best;
    }
    Report(name, best, result);
}

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        LoadBaseline(argv[1]);
    }
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
    Run("lowercase", Lowercase, nullptr);
    Run("case fold", Fold, nullptr);
    Run("skeleton", Skeleton, nullptr);
    Run("restriction level", RestrictionLevel, nullptr);
    RunFind("grapheme boundaries, find", muniFindGraphemeBreaks);
    RunFind("word boundaries, find", muniFindWordBreaks);
    RunFind("sentence boundaries, find", muniFindSentenceBreaks);
    RunFind("line breaks, find", FindLines);
    return 0;
}
