// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The normalization data (UAX #15), from UnicodeData.txt and
// DerivedNormalizationProps.txt.
//
// Code points with a decomposition are found through a rank index: a
// table gives each 64-code-point block that holds any a block number,
// and per block a 64-bit map marks which code points have one; the
// count of marked code points before a code point, plus the block's
// rank, is its entry's position. Entries sit in code point order.
//
// A canonical decomposition is one or two code points. A pair is stored
// as its first code point and the index of its second among the few
// distinct seconds ("marks"), in 32 bits; a list of the pairs that
// compose, sorted by first code point and mark, serves composition. A
// singleton is stored as its 16 low bits, with a bit map of those in
// plane 2. Compatibility decompositions are sequences in a pool of
// 16-bit units, UTF-16 style, each led by its length; each block
// records where its first sequence starts. Every mapping is one level
// deep, as the UCD gives it; the library applies them recursively.
// Hangul syllables are algorithmic.
//
// The generator derives the four quick-check properties from these
// tables and fails unless they match DerivedNormalizationProps.txt for
// every code point, so the library can answer them without tables of
// their own.

#include "munigen.h"

#define MAX_MAPPING  18
#define MAX_MAPPINGS 8192
#define MAX_PAIRS    2048
#define MAX_SINGLES  2048
#define MAX_MARKS    127
#define MAX_POOL     16384
#define EXCLUDED     0x10000000u

typedef struct Mapping
{
    uint8_t length;
    bool compatibility;
    uint32_t codePoints[MAX_MAPPING];
} Mapping;

typedef struct Normalization
{
    Mapping* list;      // the decompositions UnicodeData.txt lists
    uint16_t* mappings; // per code point: 0, or 1 plus its index in list
    int listCount;
    uint8_t* excluded; // Full_Composition_Exclusion
    uint32_t marks[MAX_MARKS];
    int markCount;
    uint32_t pairs[MAX_PAIRS]; // first << 7 | mark index, EXCLUDED when excluded
    uint32_t pairSources[MAX_PAIRS];
    int pairCount;
    uint16_t order[MAX_PAIRS]; // the pairs that compose, by first and mark
    int orderCount;
    uint32_t singles[MAX_SINGLES];
    uint32_t singleSources[MAX_SINGLES];
    int singleCount;
    uint32_t compatibilitySources[MAX_MAPPINGS];
    int compatibilityCount;
    uint16_t pool[MAX_POOL];
    int poolCount;
} Normalization;

static const Mapping s_none = {0};

static const Mapping* MappingOf(const Normalization* data, uint32_t codePoint)
{
    uint16_t index = data->mappings[codePoint];
    return index == 0 ? &s_none : &data->list[index - 1];
}

static bool IsHangulSyllable(uint32_t codePoint)
{
    return codePoint >= 0xAC00 && codePoint <= 0xD7A3;
}

static void LoadMappings(Normalization* data)
{
    FILE* file = OpenUcd("UnicodeData.txt");
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* fields[15];
        if (SplitFields(line, fields, 15) < 6 || fields[5][0] == '\0')
        {
            continue;
        }
        uint32_t codePoint = ParseCodePoint(fields[0], "UnicodeData.txt");
        if (data->listCount == MAX_MAPPINGS)
        {
            Fail("more than %d decompositions", MAX_MAPPINGS);
        }
        Mapping* mapping = &data->list[data->listCount++];
        data->mappings[codePoint] = (uint16_t)data->listCount;
        char* cursor = fields[5];
        if (*cursor == '<')
        {
            mapping->compatibility = true;
            cursor = strchr(cursor, '>') + 1;
        }
        for (char* item = strtok(cursor, " "); item != nullptr; item = strtok(nullptr, " "))
        {
            if (mapping->length == MAX_MAPPING)
            {
                Fail("U+%04X: a decomposition longer than %d", codePoint, MAX_MAPPING);
            }
            mapping->codePoints[mapping->length++] = ParseCodePoint(item, "UnicodeData.txt");
        }
    }
    fclose(file);
}

// Reads the code points of one binary or enumerated property of
// DerivedNormalizationProps.txt with the given value (NULL for binary).
static void LoadDerived(const char* name, const char* value, uint8_t* flags)
{
    FILE* file = OpenUcd("DerivedNormalizationProps.txt");
    char line[MAX_LINE];
    memset(flags, 0, CODE_POINTS);
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* fields[3];
        int count = SplitFields(line, fields, 3);
        if (line[0] == '#' || count < 2 || strcmp(fields[1], name) != 0 ||
            (value != nullptr && (count < 3 || strcmp(fields[2], value) != 0)))
        {
            continue;
        }
        uint32_t first;
        uint32_t last;
        ParseRange(fields[0], &first, &last, "DerivedNormalizationProps.txt");
        memset(flags + first, 1, last - first + 1);
    }
    fclose(file);
}

static int MarkIndex(const Normalization* data, uint32_t mark)
{
    for (int i = 0; i < data->markCount; i++)
    {
        if (data->marks[i] == mark)
        {
            return i;
        }
    }
    Fail("U+%04X is no known mark", mark);
}

static int CompareCodePoints(const void* a, const void* b)
{
    uint32_t x = *(const uint32_t*)a;
    uint32_t y = *(const uint32_t*)b;
    return x < y ? -1 : x > y ? 1 : 0;
}

static const Normalization* s_sorting; // the data CompareOrder reads

static int CompareOrder(const void* a, const void* b)
{
    uint32_t x = s_sorting->pairs[*(const uint16_t*)a] & ~EXCLUDED;
    uint32_t y = s_sorting->pairs[*(const uint16_t*)b] & ~EXCLUDED;
    return x < y ? -1 : x > y ? 1 : 0;
}

static bool IsCanonical(const Mapping* mapping, int length)
{
    return !mapping->compatibility && mapping->length == length;
}

// The marks, sorted; the pairs and singles in code point order; the
// composing pairs by key.
static void CollectCanonical(Normalization* data)
{
    for (uint32_t c = 0; c < CODE_POINTS; c++)
    {
        const Mapping* mapping = MappingOf(data, c);
        bool known = false;
        for (int i = 0; i < data->markCount && IsCanonical(mapping, 2) && !known; i++)
        {
            known = data->marks[i] == mapping->codePoints[1];
        }
        if (IsCanonical(mapping, 2) && !known)
        {
            if (data->markCount == MAX_MARKS)
            {
                Fail("more than %d distinct second code points", MAX_MARKS);
            }
            data->marks[data->markCount++] = mapping->codePoints[1];
        }
    }
    qsort(data->marks, (size_t)data->markCount, sizeof(uint32_t), CompareCodePoints);
    for (uint32_t c = 0; c < CODE_POINTS; c++)
    {
        const Mapping* mapping = MappingOf(data, c);
        if (IsCanonical(mapping, 2))
        {
            if (data->pairCount == MAX_PAIRS || mapping->codePoints[0] >> 16 != c >> 16)
            {
                Fail("U+%04X: too many pairs, or a composite off its first code point's plane", c);
            }
            if (!data->excluded[c])
            {
                data->order[data->orderCount++] = (uint16_t)data->pairCount;
            }
            data->pairs[data->pairCount] = mapping->codePoints[0] << 7 |
                                           (uint32_t)MarkIndex(data, mapping->codePoints[1]) |
                                           (data->excluded[c] ? EXCLUDED : 0);
            data->pairSources[data->pairCount++] = c;
        }
        else if (IsCanonical(mapping, 1))
        {
            uint32_t plane = mapping->codePoints[0] >> 16;
            if (data->singleCount == MAX_SINGLES || (plane != 0 && plane != 2))
            {
                Fail("U+%04X: too many singletons, or one outside planes 0 and 2", c);
            }
            data->singles[data->singleCount] = mapping->codePoints[0];
            data->singleSources[data->singleCount++] = c;
        }
    }
    s_sorting = data;
    qsort(data->order, (size_t)data->orderCount, sizeof(uint16_t), CompareOrder);
}

// Appends a compatibility mapping to the pool: its length, then its code
// points in UTF-16.
static void AddToPool(Normalization* data, uint32_t source, const Mapping* mapping)
{
    if (data->poolCount + 1 + 2 * mapping->length > MAX_POOL)
    {
        Fail("the compatibility pool is full");
    }
    data->compatibilitySources[data->compatibilityCount++] = source;
    data->pool[data->poolCount++] = mapping->length;
    for (int i = 0; i < mapping->length; i++)
    {
        uint32_t c = mapping->codePoints[i];
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
}

// Whether c is the second of a pair that composes, or a Hangul vowel or
// trailing consonant, and so may combine with what precedes it.
static bool Combines(const Normalization* data, uint32_t c)
{
    if ((c >= 0x1161 && c <= 0x1175) || (c >= 0x11A8 && c <= 0x11C2))
    {
        return true;
    }
    for (int i = 0; i < data->pairCount; i++)
    {
        if ((data->pairs[i] & EXCLUDED) == 0 && data->marks[data->pairs[i] & 0x7F] == c)
        {
            return true;
        }
    }
    return false;
}

// Whether c may combine with what precedes it (NFC_QC=Maybe): it
// combines, or it is a pair that composes and whose first code point
// may combine, as the Tulu-Tigalari vowel signs are since Unicode 16.
static bool MayCombine(const Normalization* data, uint32_t c)
{
    if (Combines(data, c))
    {
        return true;
    }
    const Mapping* mapping = MappingOf(data, c);
    return !mapping->compatibility && mapping->length == 2 && !data->excluded[c] &&
           MayCombine(data, mapping->codePoints[0]);
}

// Whether the full decomposition of c uses a compatibility mapping, so
// NFKD differs from NFD.
static bool UsesCompatibility(const Normalization* data, uint32_t c)
{
    const Mapping* mapping = MappingOf(data, c);
    if (mapping->compatibility)
    {
        return true;
    }
    for (int i = 0; i < mapping->length; i++)
    {
        if (UsesCompatibility(data, mapping->codePoints[i]))
        {
            return true;
        }
    }
    return false;
}

// Checks the quick-check values the library derives against the UCD's.
static void CheckQuickCheck(const Normalization* data, uint8_t* flags)
{
    static const char* const names[4] = {"NFD_QC", "NFKD_QC", "NFC_QC", "NFKC_QC"};
    uint8_t* combines = Allocate(CODE_POINTS);
    for (uint32_t c = 0; c < CODE_POINTS; c++)
    {
        const Mapping* mapping = MappingOf(data, c);
        bool candidate =
            (mapping->length == 2 && !mapping->compatibility) || (c >= 0x1161 && c <= 0x11C2);
        for (int i = 0; i < data->markCount && !candidate; i++)
        {
            candidate = data->marks[i] == c;
        }
        combines[c] = (uint8_t)(candidate && MayCombine(data, c));
    }
    for (int form = 0; form < 4; form++)
    {
        for (int maybe = 0; maybe <= (form >= 2 ? 1 : 0); maybe++)
        {
            LoadDerived(names[form], maybe ? "M" : "N", flags);
            for (uint32_t c = 0; c < CODE_POINTS; c++)
            {
                const Mapping* mapping = MappingOf(data, c);
                bool canonical = mapping->length > 0 && !mapping->compatibility;
                bool compatibility = UsesCompatibility(data, c);
                bool excluded = canonical && (mapping->length == 1 || data->excluded[c]);
                bool derived;
                if (maybe)
                {
                    derived = combines[c] != 0;
                }
                else if (form == 0)
                {
                    derived = canonical || IsHangulSyllable(c);
                }
                else if (form == 1)
                {
                    derived = canonical || compatibility || IsHangulSyllable(c);
                }
                else
                {
                    derived = excluded || (form == 3 && compatibility);
                }
                if (derived != (flags[c] != 0))
                {
                    Fail("U+%04X: %s=%s is %d in the UCD, %d derived", c, names[form],
                         maybe ? "M" : "N", flags[c], derived);
                }
            }
        }
    }
    free(combines);
}

// The number of 16-bit units of the pool sequence at offset.
static uint32_t PoolUnits(const Normalization* data, int offset)
{
    uint32_t units = 0;
    for (int i = 0; i < data->pool[offset]; i++)
    {
        uint16_t unit = data->pool[offset + 1 + (int)units];
        units += unit >= 0xD800 && unit <= 0xDBFF ? 2 : 1;
    }
    return units;
}

static FILE* CreateDataFile(const char* fileName, const char* what)
{
    FILE* file = CreateOutput(fileName);
    fprintf(file, "// SPDX-License-Identifier: MIT\n// Copyright (c) 2026 Sirac Ozmen\n//\n");
    fprintf(file, "// %s: see src/tables.h. Generated by tools/munigen from\n", what);
    fprintf(file, "// UnicodeData.txt and DerivedNormalizationProps.txt in tools/ucd/\n");
    fprintf(file, "// (Unicode 18.0.0); do not edit.\n// clang-format off\n\n");
    fprintf(file, "#include \"../tables.h\"\n\n");
    return file;
}

static void WriteCanonical(const Normalization* data, const Blocks* blocks)
{
    FILE* file = CreateDataFile("decomposition_data.c", "Canonical decompositions");
    WriteStarts(file, "muniDecompositionStarts", blocks);
    WriteRanks(file, "muniDecompositionPair", blocks, data->pairSources, data->pairCount);
    WriteRanks(file, "muniDecompositionSingle", blocks, data->singleSources, data->singleCount);
    fprintf(file, "const uint32_t muniDecompositionMarks[%d] = {", data->markCount);
    for (int i = 0; i < data->markCount; i++)
    {
        uint32_t composes = Combines(data, data->marks[i]) ? 0x80000000u : 0;
        fprintf(file, "%s0x%08Xu,", i % 6 == 0 ? "\n    " : " ", data->marks[i] | composes);
    }
    fprintf(file, "\n};\n\nconst uint32_t muniDecompositionPairs[%d] = {", data->pairCount);
    for (int i = 0; i < data->pairCount; i++)
    {
        fprintf(file, "%s0x%08Xu,", i % 6 == 0 ? "\n    " : " ", data->pairs[i]);
    }
    fprintf(file, "\n};\n\nconst uint16_t muniCompositionOrder[%d] = {", data->orderCount);
    for (int i = 0; i < data->orderCount; i++)
    {
        fprintf(file, "%s%d,", i % 12 == 0 ? "\n    " : " ", data->order[i]);
    }
    fprintf(file, "\n};\n\nconst uint16_t muniDecompositionSingles[%d] = {", data->singleCount);
    for (int i = 0; i < data->singleCount; i++)
    {
        fprintf(file, "%s0x%04X,", i % 8 == 0 ? "\n    " : " ", data->singles[i] & 0xFFFF);
    }
    int planeBytes = (data->singleCount + 7) / 8;
    fprintf(file, "\n};\n\nconst uint8_t muniDecompositionPlaneTwo[%d] = {", planeBytes);
    for (int byte = 0; byte < planeBytes; byte++)
    {
        uint32_t bits = 0;
        for (int k = 0; k < 8 && byte * 8 + k < data->singleCount; k++)
        {
            bits |= (data->singles[byte * 8 + k] >> 16 == 2 ? 1u : 0u) << k;
        }
        fprintf(file, "%s%u,", byte % 16 == 0 ? "\n    " : " ", bits);
    }
    fprintf(file, "\n};\n\nconst uint16_t muniDecompositionMarkCount = %d;\n", data->markCount);
    fprintf(file, "const uint16_t muniCompositionCount = %d;\n", data->orderCount);
    fprintf(file, "const uint16_t muniDecompositionBlockCount = %d;\n", blocks->count);
    fclose(file);
    printf("%-24s %7d bytes  %d blocks, %d marks, %d pairs, %d singles\n", "DecompositionData",
           blocks->count * 22 + 4 + data->markCount * 4 + data->pairCount * 4 +
               data->orderCount * 2 + data->singleCount * 2 + planeBytes,
           blocks->count, data->markCount, data->pairCount, data->singleCount);
}

static void WriteCompatibility(const Normalization* data, const Blocks* blocks)
{
    FILE* file = CreateDataFile("compatibility_data.c", "Compatibility decompositions");
    WriteStarts(file, "muniCompatibilityStarts", blocks);
    WriteRanks(file, "muniCompatibility", blocks, data->compatibilitySources,
               data->compatibilityCount);
    // Where each block's first sequence starts in the pool.
    fprintf(file, "const uint16_t muniCompatibilityOffsets[%d] = {", blocks->count);
    int offset = 0;
    int entry = 0;
    for (int k = 0; k < blocks->count; k++)
    {
        while (data->compatibilitySources[entry] >> 6 < blocks->starts[k])
        {
            offset += 1 + (int)PoolUnits(data, offset);
            entry++;
        }
        fprintf(file, "%s%d,", k % 12 == 0 ? "\n    " : " ", offset);
    }
    fprintf(file, "\n};\n\nconst uint16_t muniCompatibilityPool[%d] = {", data->poolCount);
    for (int i = 0; i < data->poolCount; i++)
    {
        fprintf(file, "%s0x%04X,", i % 8 == 0 ? "\n    " : " ", data->pool[i]);
    }
    fprintf(file, "\n};\n\nconst uint16_t muniCompatibilityBlockCount = %d;\n", blocks->count);
    fclose(file);
    printf("%-24s %7d bytes  %d blocks, %d sequences\n", "CompatibilityData",
           blocks->count * 14 + 2 + data->poolCount * 2, blocks->count, data->compatibilityCount);
}

void WriteNormalization(void)
{
    static Normalization data;
    data.list = Allocate(sizeof(Mapping) * MAX_MAPPINGS);
    data.mappings = Allocate(sizeof(uint16_t) * CODE_POINTS);
    data.excluded = Allocate(CODE_POINTS);
    LoadMappings(&data);
    LoadDerived("Full_Composition_Exclusion", nullptr, data.excluded);
    CollectCanonical(&data);
    for (uint32_t c = 0; c < CODE_POINTS; c++)
    {
        const Mapping* mapping = MappingOf(&data, c);
        if (mapping->compatibility)
        {
            AddToPool(&data, c, mapping);
        }
    }
    uint8_t* flags = Allocate(CODE_POINTS);
    CheckQuickCheck(&data, flags);
    free(flags);
    static Blocks canonical;
    AddBlocks(&canonical, data.pairSources, data.pairCount);
    AddBlocks(&canonical, data.singleSources, data.singleCount);
    WriteBlockTable(&canonical, "decomposition_blocks", "DecompositionBlock",
                    "Canonical decomposition blocks", "UnicodeData.txt");
    WriteCanonical(&data, &canonical);
    static Blocks compatibility;
    AddBlocks(&compatibility, data.compatibilitySources, data.compatibilityCount);
    WriteBlockTable(&compatibility, "compatibility_blocks", "CompatibilityBlock",
                    "Compatibility decomposition blocks", "UnicodeData.txt");
    WriteCompatibility(&data, &compatibility);
    free(data.list);
    free(data.mappings);
    free(data.excluded);
}
