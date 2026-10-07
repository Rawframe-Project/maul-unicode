// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Script runs against the Script and Script_Extensions data itself
// (Scripts.txt, ScriptExtensions.txt and PropertyValueAliases.txt in
// tools/ucd/): every code point with extensions joins a run of exactly
// the scripts it lists; alone it makes a run of its own Script when the
// list holds it, else of the list's first script by tag (the library's
// order); after a neutral code point with extensions, the first real
// Script the two share names the run. Each check also counts the cases
// where a wrong choice would show, and fails when there are none.

#include "test_harness.h"

#include "maul-unicode/encoding.h"
#include "maul-unicode/script.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SCRIPTS 256
#define MAX_RANGES  512
#define NONE        0xFF

typedef struct Set
{
    uint64_t bits[4];
} Set;

typedef struct Range
{
    uint32_t first;
    uint32_t last;
    Set scripts;
} Range;

static char s_tags[MAX_SCRIPTS][5];
static char s_names[MAX_SCRIPTS][64];
static int s_scriptCount;
static uint8_t s_script[0x110000];
static Range s_ranges[MAX_RANGES];
static int s_rangeCount;
static uint32_t s_sample[MAX_SCRIPTS]; // a code point of each script, without extensions
static int s_common;
static int s_inherited;

static bool Has(const Set* set, int script)
{
    return (set->bits[script >> 6] >> (script & 63) & 1) != 0;
}

static void Add(Set* set, int script)
{
    set->bits[script >> 6] |= (uint64_t)1 << (script & 63);
}

static uint32_t TagValue(int script)
{
    const char* tag = s_tags[script];
    return (uint32_t)tag[0] << 24 | (uint32_t)tag[1] << 16 | (uint32_t)tag[2] << 8 |
           (uint32_t)tag[3];
}

static int ByTag(const char* tag)
{
    for (int i = 0; i < s_scriptCount; i++)
    {
        if (strncmp(s_tags[i], tag, 4) == 0)
        {
            return i;
        }
    }
    return -1;
}

static int ByName(const char* name, size_t length)
{
    for (int i = 0; i < s_scriptCount; i++)
    {
        if (strlen(s_names[i]) == length && strncmp(s_names[i], name, length) == 0)
        {
            return i;
        }
    }
    return -1;
}

static bool Neutral(int script)
{
    return script == s_common || script == s_inherited;
}

static const char* SkipSpaces(const char* text)
{
    while (*text == ' ' || *text == '\t')
    {
        text++;
    }
    return text;
}

// "sc ; Adlm ; Adlam" lines: the tags, kept in the order of their values
// as the library orders its scripts.
static bool LoadAliases(void)
{
    FILE* file = fopen(MUNI_UCD_DATA "/PropertyValueAliases.txt", "r");
    if (file == nullptr)
    {
        return false;
    }
    static char line[512];
    while (fgets(line, sizeof(line), file) != nullptr && s_scriptCount < MAX_SCRIPTS)
    {
        if (strncmp(line, "sc ", 3) != 0)
        {
            continue;
        }
        const char* tag = SkipSpaces(strchr(line, ';') + 1);
        const char* name = SkipSpaces(strchr(tag, ';') + 1);
        size_t length = strcspn(name, " ;#\r\n");
        memcpy(s_tags[s_scriptCount], tag, 4);
        s_tags[s_scriptCount][4] = '\0';
        memcpy(s_names[s_scriptCount], name, length < 63 ? length : 63);
        s_scriptCount++;
    }
    fclose(file);
    // Sort by tag, the library's order.
    for (int i = 1; i < s_scriptCount; i++)
    {
        for (int j = i; j > 0 && strcmp(s_tags[j - 1], s_tags[j]) > 0; j--)
        {
            char tag[5];
            char name[64];
            memcpy(tag, s_tags[j], sizeof(tag));
            memcpy(name, s_names[j], sizeof(name));
            memcpy(s_tags[j], s_tags[j - 1], sizeof(tag));
            memcpy(s_names[j], s_names[j - 1], sizeof(name));
            memcpy(s_tags[j - 1], tag, sizeof(tag));
            memcpy(s_names[j - 1], name, sizeof(name));
        }
    }
    s_common = ByTag("Zyyy");
    s_inherited = ByTag("Zinh");
    return s_common >= 0 && s_inherited >= 0;
}

static bool LoadScripts(void)
{
    FILE* file = fopen(MUNI_UCD_DATA "/Scripts.txt", "r");
    if (file == nullptr)
    {
        return false;
    }
    memset(s_script, NONE, sizeof(s_script));
    static char line[512];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        if (line[0] == '#' || line[0] == '\n')
        {
            continue;
        }
        char* end = nullptr;
        uint32_t first = (uint32_t)strtoul(line, &end, 16);
        uint32_t last = end[0] == '.' ? (uint32_t)strtoul(end + 2, &end, 16) : first;
        const char* name = SkipSpaces(strchr(line, ';') + 1);
        int script = ByName(name, strcspn(name, " #\r\n"));
        for (uint32_t codePoint = first; codePoint <= last && script >= 0; codePoint++)
        {
            s_script[codePoint] = (uint8_t)script;
        }
    }
    fclose(file);
    return true;
}

static bool LoadExtensions(void)
{
    FILE* file = fopen(MUNI_UCD_DATA "/ScriptExtensions.txt", "r");
    if (file == nullptr)
    {
        return false;
    }
    static char line[1024];
    while (fgets(line, sizeof(line), file) != nullptr && s_rangeCount < MAX_RANGES)
    {
        if (line[0] == '#' || line[0] == '\n')
        {
            continue;
        }
        Range* range = &s_ranges[s_rangeCount++];
        memset(range, 0, sizeof(*range));
        char* end = nullptr;
        range->first = (uint32_t)strtoul(line, &end, 16);
        range->last = end[0] == '.' ? (uint32_t)strtoul(end + 2, &end, 16) : range->first;
        const char* tag = SkipSpaces(strchr(line, ';') + 1);
        while (*tag != '#' && *tag != '\n' && *tag != '\0')
        {
            int script = ByTag(tag);
            if (script >= 0)
            {
                Add(&range->scripts, script);
            }
            tag = SkipSpaces(tag + strcspn(tag, " #\r\n"));
        }
    }
    fclose(file);
    return true;
}

static const Range* ExtensionsOf(uint32_t codePoint)
{
    for (int i = 0; i < s_rangeCount; i++)
    {
        if (codePoint >= s_ranges[i].first && codePoint <= s_ranges[i].last)
        {
            return &s_ranges[i];
        }
    }
    return nullptr;
}

// A code point of each script with no extensions, to stand for it.
static void PickSamples(void)
{
    for (int i = 0; i < MAX_SCRIPTS; i++)
    {
        s_sample[i] = 0;
    }
    for (uint32_t codePoint = 0x41; codePoint < 0x110000; codePoint++)
    {
        uint8_t script = s_script[codePoint];
        if (script != NONE && s_sample[script] == 0 && !Neutral(script) &&
            ExtensionsOf(codePoint) == nullptr)
        {
            s_sample[script] = codePoint;
        }
    }
}

// The script of a run that starts with this code point.
static int Expected(uint32_t codePoint, const Set* scripts)
{
    int own = s_script[codePoint];
    if (!Neutral(own) && Has(scripts, own))
    {
        return own;
    }
    for (int i = 0; i < s_scriptCount; i++)
    {
        if (Has(scripts, i))
        {
            return i;
        }
    }
    return -1;
}

static int Lowest(const Set* scripts)
{
    for (int i = 0; i < s_scriptCount; i++)
    {
        if (Has(scripts, i))
        {
            return i;
        }
    }
    return -1;
}

// The runs of two code points.
static size_t RunsOf(uint32_t a, uint32_t b, muniScriptRun runs[2], size_t* firstLength)
{
    char text[8];
    size_t sizeA = 0;
    size_t sizeB = 0;
    size_t count = 0;
    if (muniEncodeUtf8(a, text, &sizeA) != muni_success ||
        muniEncodeUtf8(b, text + sizeA, &sizeB) != muni_success ||
        muniFindScriptRuns(text, sizeA + sizeB, runs, 2, &count) != muni_success)
    {
        return 0;
    }
    *firstLength = sizeA;
    return count;
}

static void TestJoinsExactlyItsScripts(void)
{
    int failures = 0;
    for (int i = 0; i < s_rangeCount; i++)
    {
        for (uint32_t codePoint = s_ranges[i].first; codePoint <= s_ranges[i].last; codePoint++)
        {
            const Set* scripts = &s_ranges[i].scripts;
            for (int t = 0; t < s_scriptCount; t++)
            {
                if (s_sample[t] == 0)
                {
                    continue;
                }
                muniScriptRun runs[2];
                size_t first = 0;
                size_t count = RunsOf(s_sample[t], codePoint, runs, &first);
                bool joined = count == 1 && runs[0].script == TagValue(t);
                bool split = count == 2 && runs[0].end == first && runs[0].script == TagValue(t) &&
                             runs[1].script == TagValue(Expected(codePoint, scripts));
                if (Has(scripts, t) ? !joined : !split)
                {
                    if (failures++ < 5)
                    {
                        printf("U+%04X after %s: %zu runs\n", (unsigned)codePoint, s_tags[t],
                               count);
                    }
                }
            }
        }
    }
    CHECK(s_rangeCount > 0 && failures == 0, "a code point joins exactly the scripts it lists");
}

static void TestAloneTakesItsScript(void)
{
    int failures = 0;
    int ownNotLowest = 0;
    int listOnly = 0;
    for (int i = 0; i < s_rangeCount; i++)
    {
        for (uint32_t codePoint = s_ranges[i].first; codePoint <= s_ranges[i].last; codePoint++)
        {
            const Set* scripts = &s_ranges[i].scripts;
            int expected = Expected(codePoint, scripts);
            ownNotLowest += expected == s_script[codePoint] && expected != Lowest(scripts);
            listOnly += Neutral(s_script[codePoint]);
            char text[4];
            size_t size = 0;
            muniScriptRun run;
            size_t count = 0;
            if (muniEncodeUtf8(codePoint, text, &size) != muni_success ||
                muniFindScriptRuns(text, size, &run, 1, &count) != muni_success || count != 1 ||
                run.script != TagValue(expected))
            {
                failures++;
            }
        }
    }
    CHECK(failures == 0, "alone, its own Script when listed, else the first listed");
    CHECK(ownNotLowest > 0 && listOnly > 0, "cases where the choice shows");
}

static void TestFirstSharedScriptNamesTheRun(void)
{
    int failures = 0;
    int shows = 0;
    for (int i = 0; i < s_rangeCount; i++)
    {
        uint32_t neutral = s_ranges[i].first;
        if (!Neutral(s_script[neutral]))
        {
            continue;
        }
        for (int j = 0; j < s_rangeCount; j++)
        {
            uint32_t real = s_ranges[j].first;
            int own = s_script[real];
            Set shared;
            int members = 0;
            for (int w = 0; w < 4; w++)
            {
                shared.bits[w] = s_ranges[i].scripts.bits[w] & s_ranges[j].scripts.bits[w];
            }
            for (int s = 0; s < s_scriptCount; s++)
            {
                members += Has(&shared, s);
            }
            if (Neutral(own) || !Has(&shared, own) || members < 2)
            {
                continue;
            }
            shows += Lowest(&shared) != own;
            muniScriptRun runs[2];
            size_t first = 0;
            if (RunsOf(neutral, real, runs, &first) != 1 || runs[0].script != TagValue(own))
            {
                failures++;
            }
        }
    }
    CHECK(failures == 0, "after a neutral code point, the first real Script names the run");
    CHECK(shows > 0, "cases where it is not the first shared script by tag");
}

int main(void)
{
    bool loaded = LoadAliases() && LoadScripts() && LoadExtensions();
    CHECK(loaded, "the Unicode data files open");
    if (loaded)
    {
        PickSamples();
        TestJoinsExactlyItsScripts();
        TestAloneTakesItsScript();
        TestFirstSharedScriptNamesTheRun();
    }
    return s_failures == 0 ? 0 : 1;
}
