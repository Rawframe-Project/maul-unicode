// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Building a table: the layout search and the writing of the lookup.

#include "munigen.h"

// Building a table.
//
// A table stores an array of small values over the code points. Each
// split cuts the array into blocks of 2^k items, stores every distinct
// block once in a pool, and replaces the array by the block numbers,
// which are split again. The search tries up to MAX_SPLITS splits with
// k from 1 to 9 at each, stores the values in 1, 2, 4 or 8 bits and each
// index in 8 or 16 bits, and keeps the smallest result. Code points at
// or above highStart all share highValue and are not stored.

#define MAX_SPLITS 4

typedef struct Array
{
    uint32_t* items;
    uint32_t count;
} Array;

typedef struct Plan
{
    int splits;                   // number of index levels above the data
    int blockBits[MAX_SPLITS];    // k of each split, from the data upward
    Array levels[MAX_SPLITS + 1]; // levels[0] is the data, levels[splits] the top index
    int itemBits[MAX_SPLITS + 1]; // stored bits per item of each level
    size_t bytes;
} Plan;

static void FreeArray(Array* array)
{
    free(array->items);
    array->items = nullptr;
    array->count = 0;
}

static void FreePlan(Plan* plan)
{
    for (int level = 0; level <= plan->splits; level++)
    {
        FreeArray(&plan->levels[level]);
    }
    memset(plan, 0, sizeof(*plan));
}

static uint64_t HashBytes(const uint8_t* bytes, size_t length)
{
    uint64_t hash = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < length; i++)
    {
        hash ^= bytes[i];
        hash *= 0x100000001b3ull;
    }
    return hash;
}

static uint32_t MaxItem(const Array* array)
{
    uint32_t max = 0;
    for (uint32_t i = 0; i < array->count; i++)
    {
        max = array->items[i] > max ? array->items[i] : max;
    }
    return max;
}

// Bits per stored value in the data level: the smallest of 1, 2, 4, 8
// and 16 that holds every value.
static int ValueBits(uint32_t max)
{
    return max < 2 ? 1 : max < 4 ? 2 : max < 16 ? 4 : max < 256 ? 8 : 16;
}

// Bits per stored block number in an index level, or 0 when it does not
// fit in 16 bits.
static int IndexBits(uint32_t max)
{
    return max < 256 ? 8 : max < 65536 ? 16 : 0;
}

static size_t LevelBytes(uint32_t count, int bits)
{
    return ((size_t)count * (size_t)bits + 7) / 8;
}

// Cuts array into blocks of 2^blockBits items (the last one padded with
// zeros), stores each distinct block once in poolOut and the block
// numbers in indexOut. Identical blocks are found through a hash table.
static void SplitArray(const Array* array, int blockBits, Array* poolOut, Array* indexOut)
{
    uint32_t blockSize = 1u << blockBits;
    uint32_t blockCount = (array->count + blockSize - 1) / blockSize;
    uint32_t* padded = Allocate(sizeof(uint32_t) * (size_t)blockCount * blockSize);
    memcpy(padded, array->items, sizeof(uint32_t) * array->count);
    uint32_t slotCount = 1;
    while (slotCount < blockCount * 2)
    {
        slotCount *= 2;
    }
    uint32_t* slots = Allocate(sizeof(uint32_t) * slotCount);
    memset(slots, 0xff, sizeof(uint32_t) * slotCount);
    poolOut->items = Allocate(sizeof(uint32_t) * (size_t)blockCount * blockSize);
    poolOut->count = 0;
    indexOut->items = Allocate(sizeof(uint32_t) * blockCount);
    indexOut->count = blockCount;
    for (uint32_t block = 0; block < blockCount; block++)
    {
        const uint32_t* items = padded + (size_t)block * blockSize;
        uint32_t slot = (uint32_t)HashBytes((const uint8_t*)items, sizeof(uint32_t) * blockSize) &
                        (slotCount - 1);
        uint32_t number = UINT32_MAX;
        while (slots[slot] != UINT32_MAX)
        {
            const uint32_t* candidate = poolOut->items + (size_t)slots[slot] * blockSize;
            if (memcmp(candidate, items, sizeof(uint32_t) * blockSize) == 0)
            {
                number = slots[slot];
                break;
            }
            slot = (slot + 1) & (slotCount - 1);
        }
        if (number == UINT32_MAX)
        {
            number = poolOut->count / blockSize;
            memcpy(poolOut->items + poolOut->count, items, sizeof(uint32_t) * blockSize);
            poolOut->count += blockSize;
            slots[slot] = number;
        }
        indexOut->items[block] = number;
    }
    free(padded);
    free(slots);
}

static Array CopyArray(const Array* array)
{
    Array copy = {Allocate(sizeof(uint32_t) * (array->count ? array->count : 1)), array->count};
    memcpy(copy.items, array->items, sizeof(uint32_t) * array->count);
    return copy;
}

// The smallest plan for array, which sits at the given level (0 for the
// data), with at most splitsLeft more splits. Returns a plan whose bytes
// is SIZE_MAX when nothing fits.
static Plan SearchPlan(const Array* array, int level, int splitsLeft)
{
    Plan best = {0};
    best.bytes = SIZE_MAX;
    int bits = level == 0 ? ValueBits(MaxItem(array)) : IndexBits(MaxItem(array));
    if (bits != 0 && (level > 0 || array->count <= 4096))
    {
        best.splits = 0;
        best.levels[0] = CopyArray(array);
        best.itemBits[0] = bits;
        best.bytes = LevelBytes(array->count, bits);
    }
    if (splitsLeft == 0)
    {
        return best;
    }
    for (int blockBits = 1; blockBits <= 9; blockBits++)
    {
        if ((1u << blockBits) >= array->count || (level == 0 && (bits << blockBits) < 8))
        {
            continue;
        }
        Array pool;
        Array index;
        SplitArray(array, blockBits, &pool, &index);
        int poolBits = level == 0 ? ValueBits(MaxItem(&pool)) : IndexBits(MaxItem(&pool));
        Plan upper = SearchPlan(&index, level + 1, splitsLeft - 1);
        FreeArray(&index);
        size_t bytes = upper.bytes == SIZE_MAX || poolBits == 0
                           ? SIZE_MAX
                           : upper.bytes + LevelBytes(pool.count, poolBits);
        if (bytes < best.bytes)
        {
            FreePlan(&best);
            best.splits = upper.splits + 1;
            best.levels[0] = pool;
            best.itemBits[0] = poolBits;
            best.blockBits[0] = blockBits;
            for (int i = 0; i <= upper.splits; i++)
            {
                best.levels[i + 1] = upper.levels[i];
                best.itemBits[i + 1] = upper.itemBits[i];
            }
            for (int i = 0; i < upper.splits; i++)
            {
                best.blockBits[i + 1] = upper.blockBits[i];
            }
            best.bytes = bytes;
        }
        else
        {
            FreeArray(&pool);
            FreePlan(&upper);
        }
    }
    return best;
}

typedef struct Table
{
    Plan plan;
    uint32_t highStart;
    uint16_t highValue;
} Table;

// The same walk as the lookup the generator writes.
static uint32_t Lookup(const Table* table, uint32_t codePoint)
{
    if (codePoint >= table->highStart)
    {
        return table->highValue;
    }
    const Plan* plan = &table->plan;
    int shift = 0;
    for (int level = 0; level < plan->splits; level++)
    {
        shift += plan->blockBits[level];
    }
    uint32_t item = plan->levels[plan->splits].items[codePoint >> shift];
    for (int level = plan->splits - 1; level >= 0; level--)
    {
        shift -= plan->blockBits[level];
        uint32_t mask = (1u << plan->blockBits[level]) - 1u;
        item = plan->levels[level]
                   .items[(item << plan->blockBits[level]) | ((codePoint >> shift) & mask)];
    }
    return item;
}

static Table BuildTable(const uint16_t* values, const char* name)
{
    Table table = {0};
    table.highValue = values[CODE_POINTS - 1];
    table.highStart = CODE_POINTS;
    while (table.highStart > 0 && values[table.highStart - 1] == table.highValue)
    {
        table.highStart -= 1;
    }
    Array array = {Allocate(sizeof(uint32_t) * (table.highStart ? table.highStart : 1)),
                   table.highStart};
    for (uint32_t c = 0; c < table.highStart; c++)
    {
        array.items[c] = values[c];
    }
    table.plan = SearchPlan(&array, 0, MAX_SPLITS);
    FreeArray(&array);
    if (table.plan.bytes == SIZE_MAX)
    {
        Fail("%s: no layout fits in 16-bit indexes", name);
    }
    for (uint32_t c = 0; c < CODE_POINTS; c++)
    {
        if (Lookup(&table, c) != values[c])
        {
            Fail("%s: the table answers %u for U+%04X, the UCD %u", name, Lookup(&table, c), c,
                 values[c]);
        }
    }
    return table;
}

// Writing a table.

static void WriteLevel(FILE* file, int level, const Array* array, int bits)
{
    size_t bytes = LevelBytes(array->count, bits);
    if (bits == 16)
    {
        fprintf(file, "static const uint16_t s_level%d[%u] = {", level, array->count);
        for (uint32_t i = 0; i < array->count; i++)
        {
            fprintf(file, "%s%u,", i % 16 == 0 ? "\n    " : " ", array->items[i]);
        }
    }
    else
    {
        fprintf(file, "static const uint8_t s_level%d[%zu] = {", level, bytes);
        int perByte = 8 / bits;
        for (size_t byte = 0; byte < bytes; byte++)
        {
            uint32_t packed = 0;
            for (int k = 0; k < perByte; k++)
            {
                size_t item = byte * (size_t)perByte + (size_t)k;
                if (item < array->count)
                {
                    packed |= array->items[item] << (k * bits);
                }
            }
            fprintf(file, "%s%u,", byte % 24 == 0 ? "\n    " : " ", packed);
        }
    }
    fprintf(file, "\n};\n\n");
}

// Writes the expression that reads item `index` of a level.
static void WriteRead(FILE* file, int level, int bits, const char* index)
{
    if (bits >= 8)
    {
        fprintf(file, "s_level%d[%s]", level, index);
        return;
    }
    int perByteBits = bits == 1 ? 3 : bits == 2 ? 2 : 1;
    fprintf(file, "((uint32_t)s_level%d[(%s) >> %d] >> (((%s) & %du) * %d) & %uu)", level, index,
            perByteBits, index, (1 << perByteBits) - 1, bits, (1u << bits) - 1u);
}

// Writes src/generated/<fileName>.c defining muniLookup<symbol> and
// muni<symbol>Hash.
static void WriteTableOf(const uint16_t* values, bool wide, uint64_t hash, const char* fileName,
                         const char* symbol, const char* description, const char* inputs)
{
    Table table = BuildTable(values, symbol);
    const char* type = wide ? "uint16_t" : "uint8_t";
    const Plan* plan = &table.plan;
    char name[256];
    snprintf(name, sizeof(name), "%s.c", fileName);
    FILE* file = CreateOutput(name);
    fprintf(file, "// SPDX-License-Identifier: MIT\n");
    fprintf(file, "// Copyright (c) 2026 Sirac Ozmen\n");
    fprintf(file, "//\n");
    char text[1024];
    snprintf(text, sizeof(text),
             "%s. Generated by tools/munigen from %s in tools/ucd/ (Unicode 18.0.0); do not "
             "edit. %zu bytes of tables.",
             description, inputs, plan->bytes);
    WriteComment(file, text);
    fprintf(file, "// clang-format off\n\n");
    fprintf(file, "#include \"../tables.h\"\n\n");
    for (int level = plan->splits; level >= 0; level--)
    {
        WriteLevel(file, level, &plan->levels[level], plan->itemBits[level]);
    }
    int shift = 0;
    for (int level = 0; level < plan->splits; level++)
    {
        shift += plan->blockBits[level];
    }
    fprintf(file, "%s muniLookup%s(uint32_t codePoint)\n{\n", type, symbol);
    fprintf(file, "    if (codePoint >= 0x%Xu)\n    {\n        return %u;\n    }\n",
            table.highStart, table.highValue);
    char index[128];
    snprintf(index, sizeof(index), "codePoint >> %d", shift);
    fprintf(file, "    uint32_t item = ");
    WriteRead(file, plan->splits, plan->itemBits[plan->splits], index);
    fprintf(file, ";\n");
    for (int level = plan->splits - 1; level >= 0; level--)
    {
        shift -= plan->blockBits[level];
        snprintf(index, sizeof(index), "(item << %d) | ((codePoint >> %d) & %uu)",
                 plan->blockBits[level], shift, (1u << plan->blockBits[level]) - 1u);
        fprintf(file, "    item = ");
        WriteRead(file, level, plan->itemBits[level], index);
        fprintf(file, ";\n");
    }
    fprintf(file, "    return (%s)item;\n}\n\n", type);
    fprintf(file, "const uint64_t muni%sHash = 0x%016llxull;\n", symbol, (unsigned long long)hash);
    fclose(file);
    printf("%-24s %7zu bytes  %d levels, blocks", symbol, plan->bytes, plan->splits + 1);
    for (int level = 0; level < plan->splits; level++)
    {
        printf(" %d", plan->blockBits[level]);
    }
    printf(", high start U+%04X\n", table.highStart);
    FreePlan(&table.plan);
}

// Writes src/generated/<fileName>.c for a property of 8-bit values: the
// lookup returns uint8_t, and the hash covers one byte per code point.
void WriteTable(const uint8_t* values, const char* fileName, const char* symbol,
                const char* description, const char* inputs)
{
    uint16_t* wide = Allocate(sizeof(uint16_t) * CODE_POINTS);
    for (uint32_t c = 0; c < CODE_POINTS; c++)
    {
        wide[c] = values[c];
    }
    WriteTableOf(wide, false, HashBytes(values, CODE_POINTS), fileName, symbol, description,
                 inputs);
    free(wide);
}

// Writes src/generated/<fileName>.c for a property of 16-bit values: the
// lookup returns uint16_t, and the hash covers two bytes per code point,
// low byte first.
void WriteWideTable(const uint16_t* values, const char* fileName, const char* symbol,
                    const char* description, const char* inputs)
{
    uint8_t* bytes = Allocate(2 * (size_t)CODE_POINTS);
    for (uint32_t c = 0; c < CODE_POINTS; c++)
    {
        bytes[2 * c] = (uint8_t)values[c];
        bytes[2 * c + 1] = (uint8_t)(values[c] >> 8);
    }
    WriteTableOf(values, true, HashBytes(bytes, 2 * (size_t)CODE_POINTS), fileName, symbol,
                 description, inputs);
    free(bytes);
}

// Rank indexes.

static int CompareStarts(const void* a, const void* b)
{
    return (int)*(const uint16_t*)a - (int)*(const uint16_t*)b;
}

void AddBlocks(Blocks* blocks, const uint32_t* sources, int count)
{
    for (int i = 0; i < count; i++)
    {
        uint16_t start = (uint16_t)(sources[i] >> 6);
        int k = 0;
        while (k < blocks->count && blocks->starts[k] != start)
        {
            k++;
        }
        if (k == blocks->count)
        {
            if (blocks->count == MAX_BLOCKS)
            {
                Fail("more than %d blocks", MAX_BLOCKS);
            }
            blocks->starts[blocks->count++] = start;
        }
    }
    qsort(blocks->starts, (size_t)blocks->count, sizeof(uint16_t), CompareStarts);
}

void WriteBlockTable(const Blocks* blocks, const char* fileName, const char* symbol,
                     const char* description, const char* inputs)
{
    if (blocks->count > UINT8_MAX)
    {
        Fail("%s: more than %d blocks for an 8-bit table", symbol, UINT8_MAX);
    }
    uint8_t* values = Allocate(CODE_POINTS);
    for (int k = 0; k < blocks->count; k++)
    {
        memset(values + ((uint32_t)blocks->starts[k] << 6), k + 1, 64);
    }
    WriteTable(values, fileName, symbol, description, inputs);
    free(values);
}

void WriteStarts(FILE* file, const char* name, const Blocks* blocks)
{
    fprintf(file, "const uint16_t %s[%d] = {", name, blocks->count);
    for (int k = 0; k < blocks->count; k++)
    {
        fprintf(file, "%s0x%04X,", k % 8 == 0 ? "\n    " : " ", blocks->starts[k]);
    }
    fprintf(file, "\n};\n\n");
}

void WriteRanks(FILE* file, const char* name, const Blocks* blocks, const uint32_t* sources,
                int count)
{
    fprintf(file, "const uint64_t %sBits[%d] = {", name, blocks->count);
    for (int k = 0; k < blocks->count; k++)
    {
        uint64_t bits = 0;
        for (int i = 0; i < count; i++)
        {
            bits |= sources[i] >> 6 == blocks->starts[k] ? (uint64_t)1 << (sources[i] & 63) : 0;
        }
        fprintf(file, "%s0x%016llXull,", k % 3 == 0 ? "\n    " : " ", (unsigned long long)bits);
    }
    fprintf(file, "\n};\n\nconst uint16_t %sRanks[%d] = {", name, blocks->count + 1);
    int rank = 0;
    for (int k = 0; k <= blocks->count; k++)
    {
        while (k < blocks->count && rank < count && sources[rank] >> 6 < blocks->starts[k])
        {
            rank++;
        }
        fprintf(file, "%s%d,", k % 12 == 0 ? "\n    " : " ", k == blocks->count ? count : rank);
    }
    fprintf(file, "\n};\n\n");
}
