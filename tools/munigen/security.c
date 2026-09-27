// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The data of UTS #39, Unicode Security Mechanisms: from the security
// files confusables.txt and IdentifierStatus.txt, with UnicodeData.txt
// and DerivedCoreProperties.txt.
//
// Confusable prototypes are found through a rank index (see munigen.h)
// whose blocks the library finds by binary search over their starts, as
// a block table would cost more than the index itself. Sources a
// skeleton never looks up are left out: those with a canonical
// decomposition, which NFD replaces first, and the default ignorables,
// which the skeleton drops before mapping. An entry is 16 bits: a
// prototype of one code point below U+8000 is that code point; any
// other is 0x8000, its length in bits 13 and 14, and its offset in a
// pool of UTF-16 units. A length above 3 is stored as 0, and the
// prototype's first unit in the pool is then its length. Prototypes
// that recur are stored once.
//
// A table gives each code point Identifier_Status=Allowed.

#include "munigen.h"

#define MAX_PROTOTYPE   18
#define MAX_CONFUSABLES 8192
#define MAX_POOL        8192
#define POOLED          0x8000u
#define SHORT_LENGTH    3

typedef struct Confusable
{
    uint32_t source;
    int length;
    uint32_t prototype[MAX_PROTOTYPE];
} Confusable;

typedef struct Security
{
    Confusable* lines; // confusables.txt, sorted by source
    int lineCount;
    uint32_t sources[MAX_CONFUSABLES]; // the sources kept
    uint16_t entries[MAX_CONFUSABLES];
    int count;
    uint16_t pool[MAX_POOL];
    int poolCount;
    int stored[MAX_CONFUSABLES]; // the line each pooled prototype came from
    int storedOffsets[MAX_CONFUSABLES];
    int storedCount;
    uint8_t* decomposes; // a canonical decomposition, Hangul syllables included
    uint8_t* ignorable;  // Default_Ignorable_Code_Point
} Security;

// The code points with a canonical decomposition.
static void LoadDecompositions(Security* data)
{
    FILE* file = OpenUcd("UnicodeData.txt");
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* fields[15];
        if (SplitFields(line, fields, 15) < 6)
        {
            continue;
        }
        uint32_t codePoint = ParseCodePoint(fields[0], "UnicodeData.txt");
        data->decomposes[codePoint] = (uint8_t)(fields[5][0] != '\0' && fields[5][0] != '<');
    }
    fclose(file);
    memset(data->decomposes + 0xAC00, 1, 0xD7A4 - 0xAC00);
}

static int CompareSources(const void* a, const void* b)
{
    uint32_t x = ((const Confusable*)a)->source;
    uint32_t y = ((const Confusable*)b)->source;
    return x < y ? -1 : x > y ? 1 : 0;
}

static void LoadConfusables(Security* data)
{
    FILE* file = OpenUcd("confusables.txt");
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* fields[3];
        if (SplitFields(line, fields, 3) < 2 || fields[0][0] == '\0')
        {
            continue;
        }
        if (data->lineCount == MAX_CONFUSABLES)
        {
            Fail("more than %d confusables", MAX_CONFUSABLES);
        }
        Confusable* confusable = &data->lines[data->lineCount++];
        confusable->source = ParseCodePoint(fields[0], "confusables.txt");
        for (char* item = strtok(fields[1], " "); item != nullptr; item = strtok(nullptr, " "))
        {
            if (confusable->length == MAX_PROTOTYPE)
            {
                Fail("U+%04X: a prototype longer than %d", confusable->source, MAX_PROTOTYPE);
            }
            confusable->prototype[confusable->length++] = ParseCodePoint(item, "confusables.txt");
        }
    }
    fclose(file);
    qsort(data->lines, (size_t)data->lineCount, sizeof(Confusable), CompareSources);
    for (int i = 1; i < data->lineCount; i++)
    {
        if (data->lines[i].source == data->lines[i - 1].source)
        {
            Fail("U+%04X is listed twice", data->lines[i].source);
        }
    }
}

static bool SamePrototype(const Confusable* a, const Confusable* b)
{
    return a->length == b->length &&
           memcmp(a->prototype, b->prototype, sizeof(uint32_t) * (size_t)a->length) == 0;
}

// The pool offset of the prototype of line, added when it is new.
static int PoolOffset(Security* data, int line)
{
    const Confusable* confusable = &data->lines[line];
    for (int k = 0; k < data->storedCount; k++)
    {
        if (SamePrototype(&data->lines[data->stored[k]], confusable))
        {
            return data->storedOffsets[k];
        }
    }
    int offset = data->poolCount;
    if (offset + 1 + 2 * confusable->length > MAX_POOL)
    {
        Fail("the prototype pool is full");
    }
    if (confusable->length > SHORT_LENGTH)
    {
        data->pool[data->poolCount++] = (uint16_t)confusable->length;
    }
    for (int i = 0; i < confusable->length; i++)
    {
        uint32_t c = confusable->prototype[i];
        if (c >= 0x10000)
        {
            data->pool[data->poolCount++] = (uint16_t)(0xD800 + ((c - 0x10000) >> 10));
            data->pool[data->poolCount++] = (uint16_t)(0xDC00 + ((c - 0x10000) & 0x3FF));
        }
        else
        {
            data->pool[data->poolCount++] = (uint16_t)c;
        }
    }
    data->stored[data->storedCount] = line;
    data->storedOffsets[data->storedCount++] = offset;
    return offset;
}

static void CollectEntries(Security* data)
{
    int dropped = 0;
    for (int i = 0; i < data->lineCount; i++)
    {
        const Confusable* confusable = &data->lines[i];
        if (data->decomposes[confusable->source] || data->ignorable[confusable->source])
        {
            dropped++;
            continue;
        }
        uint32_t first = confusable->prototype[0];
        uint32_t entry = first;
        if (confusable->length > 1 || first >= POOLED)
        {
            uint32_t length = confusable->length > SHORT_LENGTH ? 0 : (uint32_t)confusable->length;
            entry = POOLED | length << 13 | (uint32_t)PoolOffset(data, i);
        }
        data->sources[data->count] = confusable->source;
        data->entries[data->count++] = (uint16_t)entry;
    }
    if (data->poolCount > 0x2000)
    {
        Fail("the prototype pool outgrows 13-bit offsets");
    }
    printf("%-24s %d confusables, %d of them never looked up\n", "Confusables", data->lineCount,
           dropped);
}

// Identifier_Status=Allowed, the General Security Profile.
static void WriteSecurityTable(void)
{
    uint8_t* values = Allocate(CODE_POINTS);
    LoadBinary("IdentifierStatus.txt", "Allowed", values);
    WriteTable(values, "security", "Security", "Identifier_Status=Allowed", "IdentifierStatus.txt");
    free(values);
}

static void WriteData(const Security* data, const Blocks* blocks)
{
    FILE* file = CreateOutput("security_data.c");
    fprintf(file, "// SPDX-License-Identifier: MIT\n// Copyright (c) 2026 Sirac Ozmen\n//\n");
    WriteComment(file, "Confusable prototypes: see src/tables.h. Generated by tools/munigen from "
                       "confusables.txt, UnicodeData.txt and DerivedCoreProperties.txt in "
                       "tools/ucd/ (Unicode 18.0.0); do not edit.");
    fprintf(file, "// clang-format off\n\n#include \"../tables.h\"\n\n");
    WriteStarts(file, "muniConfusableStarts", blocks);
    WriteRanks(file, "muniConfusable", blocks, data->sources, data->count);
    fprintf(file, "const uint16_t muniConfusables[%d] = {", data->count);
    for (int i = 0; i < data->count; i++)
    {
        fprintf(file, "%s0x%04X,", i % 8 == 0 ? "\n    " : " ", data->entries[i]);
    }
    fprintf(file, "\n};\n\nconst uint16_t muniConfusablePool[%d] = {", data->poolCount);
    for (int i = 0; i < data->poolCount; i++)
    {
        fprintf(file, "%s0x%04X,", i % 8 == 0 ? "\n    " : " ", data->pool[i]);
    }
    fprintf(file, "\n};\n\nconst uint16_t muniConfusableBlockCount = %d;\n", blocks->count);
    fclose(file);
    printf("%-24s %7d bytes  %d blocks, %d entries, %d pool units\n", "SecurityData",
           blocks->count * 12 + 2 + data->count * 2 + data->poolCount * 2, blocks->count,
           data->count, data->poolCount);
}

void WriteSecurity(void)
{
    static Security data;
    data.lines = Allocate(sizeof(Confusable) * MAX_CONFUSABLES);
    data.decomposes = Allocate(CODE_POINTS);
    data.ignorable = Allocate(CODE_POINTS);
    LoadDecompositions(&data);
    LoadBinary("DerivedCoreProperties.txt", "Default_Ignorable_Code_Point", data.ignorable);
    LoadConfusables(&data);
    CollectEntries(&data);
    WriteSecurityTable();
    static Blocks blocks;
    AddBlocks(&blocks, data.sources, data.count);
    WriteData(&data, &blocks);
    free(data.lines);
    free(data.decomposes);
    free(data.ignorable);
}
