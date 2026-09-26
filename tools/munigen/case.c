// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The case data: simple mappings from UnicodeData.txt, simple foldings
// (statuses C and S) and full foldings (F) from CaseFolding.txt, the
// unconditional full mappings of SpecialCasing.txt, and Cased and
// Case_Ignorable from DerivedCoreProperties.txt.
//
// Each code point gets a record: the indexes of its upper, lower, title
// and fold distances among the distinct distances, and flags. The table
// stores the record's index. Code points with a full mapping that
// differs from the simple one ("special") list their four full mappings
// in a sorted table. The conditional mappings (Final_Sigma and the
// Turkic and Lithuanian rules) are left to the library's code.

#include "munigen.h"

#define MAX_RECORDS    255
#define MAX_DELTAS     255
#define MAX_SPECIALS   256
#define MAX_SEQUENCE   3
#define MAX_POOL       2048
#define FLAG_CASED     1
#define FLAG_IGNORABLE 2
#define FLAG_SPECIAL   4

enum
{
    Upper = 0,
    Lower = 1,
    Title = 2,
    Fold = 3,
};

typedef struct Special
{
    uint32_t codePoint;
    uint8_t lengths[4];
    uint32_t sequences[4][MAX_SEQUENCE];
} Special;

typedef struct CaseData
{
    uint32_t* simple[4]; // per code point, the simple mapping
    uint8_t* flags;
    int32_t deltas[MAX_DELTAS];
    int deltaCount;
    uint8_t records[MAX_RECORDS][5]; // upper, lower, title, fold, flags
    int recordCount;
    Special specials[MAX_SPECIALS];
    int specialCount;
} CaseData;

static void LoadSimple(CaseData* data)
{
    FILE* file = OpenUcd("UnicodeData.txt");
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* fields[15];
        if (SplitFields(line, fields, 15) < 15)
        {
            continue;
        }
        uint32_t codePoint = ParseCodePoint(fields[0], "UnicodeData.txt");
        if (fields[12][0] != '\0')
        {
            data->simple[Upper][codePoint] = ParseCodePoint(fields[12], "UnicodeData.txt");
        }
        if (fields[13][0] != '\0')
        {
            data->simple[Lower][codePoint] = ParseCodePoint(fields[13], "UnicodeData.txt");
        }
        // An empty titlecase field means the uppercase mapping.
        data->simple[Title][codePoint] = fields[14][0] != '\0'
                                             ? ParseCodePoint(fields[14], "UnicodeData.txt")
                                             : data->simple[Upper][codePoint];
    }
    fclose(file);
}

static Special* FindSpecial(CaseData* data, uint32_t codePoint)
{
    for (int i = 0; i < data->specialCount; i++)
    {
        if (data->specials[i].codePoint == codePoint)
        {
            return &data->specials[i];
        }
    }
    if (data->specialCount == MAX_SPECIALS)
    {
        Fail("more than %d special case mappings", MAX_SPECIALS);
    }
    Special* special = &data->specials[data->specialCount++];
    memset(special, 0, sizeof(*special));
    special->codePoint = codePoint;
    return special;
}

static void ParseSequence(char* text, Special* special, int kind, const char* context)
{
    special->lengths[kind] = 0;
    for (char* item = strtok(text, " "); item != nullptr; item = strtok(nullptr, " "))
    {
        if (special->lengths[kind] == MAX_SEQUENCE)
        {
            Fail("%s: a mapping longer than %d", context, MAX_SEQUENCE);
        }
        special->sequences[kind][special->lengths[kind]++] = ParseCodePoint(item, context);
    }
    special->lengths[kind] |= 0x80; // given by the file
}

static void LoadFolding(CaseData* data)
{
    FILE* file = OpenUcd("CaseFolding.txt");
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* fields[4];
        if (line[0] == '#' || SplitFields(line, fields, 4) < 3)
        {
            continue;
        }
        uint32_t codePoint = ParseCodePoint(fields[0], "CaseFolding.txt");
        if (strcmp(fields[1], "C") == 0 || strcmp(fields[1], "S") == 0)
        {
            data->simple[Fold][codePoint] = ParseCodePoint(fields[2], "CaseFolding.txt");
        }
        else if (strcmp(fields[1], "F") == 0)
        {
            ParseSequence(fields[2], FindSpecial(data, codePoint), Fold, "CaseFolding.txt");
        }
    }
    fclose(file);
}

// The unconditional lines of SpecialCasing.txt: code; lower; title; upper.
static void LoadSpecialCasing(CaseData* data)
{
    FILE* file = OpenUcd("SpecialCasing.txt");
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* fields[6];
        int count = SplitFields(line, fields, 6);
        if (line[0] == '#' || count < 4 || fields[0][0] == '\0' ||
            (count > 4 && fields[4][0] != '\0'))
        {
            continue;
        }
        Special* special = FindSpecial(data, ParseCodePoint(fields[0], "SpecialCasing.txt"));
        ParseSequence(fields[1], special, Lower, "SpecialCasing.txt");
        ParseSequence(fields[2], special, Title, "SpecialCasing.txt");
        ParseSequence(fields[3], special, Upper, "SpecialCasing.txt");
    }
    fclose(file);
}

static int DeltaIndex(CaseData* data, int32_t delta)
{
    for (int i = 0; i < data->deltaCount; i++)
    {
        if (data->deltas[i] == delta)
        {
            return i;
        }
    }
    if (data->deltaCount == MAX_DELTAS)
    {
        Fail("more than %d distinct case distances", MAX_DELTAS);
    }
    data->deltas[data->deltaCount] = delta;
    return data->deltaCount++;
}

static int RecordIndex(CaseData* data, const uint8_t* record)
{
    for (int i = 0; i < data->recordCount; i++)
    {
        if (memcmp(data->records[i], record, 5) == 0)
        {
            return i;
        }
    }
    if (data->recordCount == MAX_RECORDS)
    {
        Fail("more than %d distinct case records", MAX_RECORDS);
    }
    memcpy(data->records[data->recordCount], record, 5);
    return data->recordCount++;
}

static int CompareSpecials(const void* a, const void* b)
{
    uint32_t x = ((const Special*)a)->codePoint;
    uint32_t y = ((const Special*)b)->codePoint;
    return x < y ? -1 : x > y ? 1 : 0;
}

// Fills the full mappings a special code point's files left out with its
// simple ones.
static void CompleteSpecials(CaseData* data)
{
    qsort(data->specials, (size_t)data->specialCount, sizeof(Special), CompareSpecials);
    for (int i = 0; i < data->specialCount; i++)
    {
        Special* special = &data->specials[i];
        for (int kind = 0; kind < 4; kind++)
        {
            if ((special->lengths[kind] & 0x80) != 0)
            {
                special->lengths[kind] &= 0x7F;
                continue;
            }
            special->lengths[kind] = 1;
            special->sequences[kind][0] = data->simple[kind][special->codePoint];
        }
    }
}

// Appends a code point to the pool in UTF-16.
static void AddUnits(uint16_t* pool, int* count, uint32_t codePoint)
{
    if (*count + 2 > MAX_POOL)
    {
        Fail("the special case pool is full");
    }
    if (codePoint >= 0x10000)
    {
        pool[(*count)++] = (uint16_t)(0xD800 + ((codePoint - 0x10000) >> 10));
        pool[(*count)++] = (uint16_t)(0xDC00 + ((codePoint - 0x10000) & 0x3FF));
    }
    else
    {
        pool[(*count)++] = (uint16_t)codePoint;
    }
}

static void WriteCaseData(const CaseData* data)
{
    FILE* file = CreateOutput("case_data.c");
    fprintf(file, "// SPDX-License-Identifier: MIT\n// Copyright (c) 2026 Sirac Ozmen\n//\n");
    fprintf(file, "// Case records, distances and full mappings: see src/tables.h. Generated\n");
    fprintf(file, "// by tools/munigen from UnicodeData.txt, CaseFolding.txt,\n");
    fprintf(file, "// SpecialCasing.txt and DerivedCoreProperties.txt in tools/ucd/\n");
    fprintf(file, "// (Unicode 18.0.0); do not edit.\n// clang-format off\n\n");
    fprintf(file, "#include \"../tables.h\"\n\n");
    fprintf(file, "const int32_t muniCaseDeltas[%d] = {", data->deltaCount);
    for (int i = 0; i < data->deltaCount; i++)
    {
        fprintf(file, "%s%d,", i % 10 == 0 ? "\n    " : " ", data->deltas[i]);
    }
    fprintf(file, "\n};\n\nconst uint8_t muniCaseRecords[%d][5] = {", data->recordCount);
    for (int i = 0; i < data->recordCount; i++)
    {
        const uint8_t* record = data->records[i];
        fprintf(file, "%s{%u, %u, %u, %u, %u},", i % 4 == 0 ? "\n    " : " ", record[0], record[1],
                record[2], record[3], record[4]);
    }
    static uint16_t pool[MAX_POOL];
    int poolCount = 0;
    fprintf(file, "\n};\n\nconst uint32_t muniCaseSpecials[%d] = {", data->specialCount);
    for (int i = 0; i < data->specialCount; i++)
    {
        const Special* special = &data->specials[i];
        uint32_t lengths = 0;
        for (int kind = 0; kind < 4; kind++)
        {
            lengths |= (uint32_t)special->lengths[kind] << (2 * kind);
        }
        fprintf(file, "%s0x%08Xu,", i % 6 == 0 ? "\n    " : " ", special->codePoint << 8 | lengths);
    }
    fprintf(file, "\n};\n\nconst uint16_t muniCaseSpecialOffsets[%d] = {", data->specialCount);
    for (int i = 0; i < data->specialCount; i++)
    {
        fprintf(file, "%s%d,", i % 12 == 0 ? "\n    " : " ", poolCount);
        const Special* special = &data->specials[i];
        for (int kind = 0; kind < 4; kind++)
        {
            for (int k = 0; k < special->lengths[kind]; k++)
            {
                AddUnits(pool, &poolCount, special->sequences[kind][k]);
            }
        }
    }
    fprintf(file, "\n};\n\nconst uint16_t muniCaseSpecialPool[%d] = {", poolCount);
    for (int i = 0; i < poolCount; i++)
    {
        fprintf(file, "%s0x%04X,", i % 8 == 0 ? "\n    " : " ", pool[i]);
    }
    fprintf(file, "\n};\n\nconst uint16_t muniCaseSpecialCount = %d;\n", data->specialCount);
    fclose(file);
    printf("%-24s %7d bytes  %d records, %d distances, %d specials\n", "CaseData",
           data->deltaCount * 4 + data->recordCount * 5 + data->specialCount * 6 + poolCount * 2 +
               2,
           data->recordCount, data->deltaCount, data->specialCount);
}

void WriteCase(void)
{
    static CaseData data;
    for (int kind = 0; kind < 4; kind++)
    {
        data.simple[kind] = Allocate(sizeof(uint32_t) * CODE_POINTS);
        for (uint32_t c = 0; c < CODE_POINTS; c++)
        {
            data.simple[kind][c] = c;
        }
    }
    data.flags = Allocate(CODE_POINTS);
    LoadSimple(&data);
    LoadFolding(&data);
    LoadSpecialCasing(&data);
    CompleteSpecials(&data);
    uint8_t* values = Allocate(CODE_POINTS);
    uint8_t* flags = Allocate(CODE_POINTS);
    LoadBinary("DerivedCoreProperties.txt", "Cased", flags);
    for (uint32_t c = 0; c < CODE_POINTS; c++)
    {
        data.flags[c] = flags[c] ? FLAG_CASED : 0;
    }
    LoadBinary("DerivedCoreProperties.txt", "Case_Ignorable", flags);
    for (uint32_t c = 0; c < CODE_POINTS; c++)
    {
        data.flags[c] |= flags[c] ? FLAG_IGNORABLE : 0;
    }
    for (int i = 0; i < data.specialCount; i++)
    {
        data.flags[data.specials[i].codePoint] |= FLAG_SPECIAL;
    }
    uint8_t identity[5] = {0};
    identity[0] = (uint8_t)DeltaIndex(&data, 0);
    RecordIndex(&data, identity); // record 0 maps everything to itself
    for (uint32_t c = 0; c < CODE_POINTS; c++)
    {
        uint8_t record[5];
        for (int kind = 0; kind < 4; kind++)
        {
            record[kind] = (uint8_t)DeltaIndex(&data, (int32_t)data.simple[kind][c] - (int32_t)c);
        }
        record[4] = data.flags[c];
        values[c] = (uint8_t)RecordIndex(&data, record);
    }
    WriteTable(values, "case", "Case", "Case records", "UnicodeData.txt and CaseFolding.txt");
    WriteCaseData(&data);
    for (int kind = 0; kind < 4; kind++)
    {
        free(data.simple[kind]);
    }
    free(data.flags);
    free(values);
    free(flags);
}
