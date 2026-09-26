// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// munigen: writes the property tables in src/generated/ from the Unicode
// Character Database files in tools/ucd/ (docs/adr/muni-0003 and
// muni-0004). For each property it builds the full array of values,
// searches the three-level split that stores it in the fewest bytes,
// checks every code point through the finished table against the array,
// and writes the table with a hash of the array.
//
// usage: munigen <ucd directory> <output directory>
//
// A development tool: it runs on the maintainer's machine and in CI,
// never in a consumer's build, and it stops at the first problem.

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CODE_POINTS 0x110000u
#define MAX_LINE    4096

static const char* s_ucdDirectory;
static const char* s_outputDirectory;

[[noreturn]] static void Fail(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    fputs("munigen: ", stderr);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
    exit(1);
}

static void* Allocate(size_t size)
{
    void* memory = calloc(1, size);
    if (memory == nullptr)
    {
        Fail("out of memory");
    }
    return memory;
}

// Parsing the UCD.

static FILE* OpenUcd(const char* name)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", s_ucdDirectory, name);
    FILE* file = fopen(path, "r");
    if (file == nullptr)
    {
        Fail("cannot open %s", path);
    }
    return file;
}

// Removes the comment and the surrounding white space of a UCD field.
static char* Trim(char* text)
{
    char* hash = strchr(text, '#');
    if (hash != nullptr)
    {
        *hash = '\0';
    }
    while (*text == ' ' || *text == '\t')
    {
        text += 1;
    }
    size_t length = strlen(text);
    while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t' ||
                          text[length - 1] == '\n' || text[length - 1] == '\r'))
    {
        text[--length] = '\0';
    }
    return text;
}

// Splits a UCD line at ';' into at most fieldCapacity trimmed fields and
// returns how many there are. The line is modified.
static int SplitFields(char* line, char** fields, int fieldCapacity)
{
    int count = 0;
    char* cursor = line;
    while (count < fieldCapacity)
    {
        char* semicolon = strchr(cursor, ';');
        if (semicolon != nullptr)
        {
            *semicolon = '\0';
        }
        fields[count] = Trim(cursor);
        count += 1;
        if (semicolon == nullptr)
        {
            break;
        }
        cursor = semicolon + 1;
    }
    return count;
}

static uint32_t ParseCodePoint(const char* text, const char* context)
{
    char* end = nullptr;
    unsigned long value = strtoul(text, &end, 16);
    if (end == text || value >= CODE_POINTS)
    {
        Fail("bad code point '%s' in %s", text, context);
    }
    return (uint32_t)value;
}

// Returns the index of name in names, or fails.
static uint8_t ValueIndex(const char* const* names, int count, const char* name,
                          const char* context)
{
    for (int i = 0; i < count; i++)
    {
        if (strcmp(names[i], name) == 0)
        {
            return (uint8_t)i;
        }
    }
    Fail("unknown value '%s' in %s", name, context);
}

// Parses "XXXX" or "XXXX..YYYY" into an inclusive range.
static void ParseRange(const char* text, uint32_t* firstOut, uint32_t* lastOut, const char* context)
{
    const char* dots = strstr(text, "..");
    *firstOut = ParseCodePoint(text, context);
    *lastOut = dots != nullptr ? ParseCodePoint(dots + 2, context) : *firstOut;
    if (*lastOut < *firstOut)
    {
        Fail("reversed range '%s' in %s", text, context);
    }
}

// The short name of a property value, from PropertyValueAliases.txt: the
// UCD files spell a value by its short or its long name, and the library
// orders values by short name.
static void ShortValueName(const char* property, const char* name, char* shortOut, size_t capacity)
{
    FILE* file = OpenUcd("PropertyValueAliases.txt");
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        if (line[0] == '#')
        {
            continue;
        }
        char* fields[6];
        int count = SplitFields(line, fields, 6);
        if (count < 3 || strcmp(fields[0], property) != 0)
        {
            continue;
        }
        for (int i = 1; i < count; i++)
        {
            if (strcmp(fields[i], name) == 0)
            {
                snprintf(shortOut, capacity, "%s", fields[1]);
                fclose(file);
                return;
            }
        }
    }
    fclose(file);
    Fail("%s has no value named '%s'", property, name);
}

// An enumerated property read from a UCD file whose lines are
// "range ; value" or, when key is not null, "range ; key ; value". The
// file's @missing lines give the defaults, applied first in the order
// they appear; names are the library's order of short value names.
static void LoadEnumerated(const char* fileName, const char* property, const char* key,
                           const char* const* names, int nameCount, uint8_t* values)
{
    FILE* file = OpenUcd(fileName);
    char line[MAX_LINE];
    char shortName[64];
    memset(values, 0, CODE_POINTS);
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* text = line;
        int missing = strncmp(line, "# @missing:", 11) == 0;
        if (missing)
        {
            text = line + 11;
        }
        else if (line[0] == '#' || line[0] == '\n')
        {
            continue;
        }
        char* fields[4];
        int count = SplitFields(text, fields, 4);
        int valueField = key != nullptr ? 2 : 1;
        if (count <= valueField || (key != nullptr && strcmp(fields[1], key) != 0))
        {
            continue;
        }
        uint32_t first;
        uint32_t last;
        ParseRange(fields[0], &first, &last, fileName);
        ShortValueName(property, fields[valueField], shortName, sizeof(shortName));
        uint8_t value = ValueIndex(names, nameCount, shortName, fileName);
        memset(values + first, value, last - first + 1);
    }
    fclose(file);
}

// A binary property: 1 for every code point a line of fileName lists
// with the given name, 0 elsewhere.
static void LoadBinary(const char* fileName, const char* name, uint8_t* values)
{
    FILE* file = OpenUcd(fileName);
    char line[MAX_LINE];
    memset(values, 0, CODE_POINTS);
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        if (line[0] == '#' || line[0] == '\n')
        {
            continue;
        }
        char* fields[3];
        if (SplitFields(line, fields, 3) < 2 || strcmp(fields[1], name) != 0)
        {
            continue;
        }
        uint32_t first;
        uint32_t last;
        ParseRange(fields[0], &first, &last, fileName);
        memset(values + first, 1, last - first + 1);
    }
    fclose(file);
}

// The value orders of the enumerated properties, as the public headers
// number them.
static const char* const s_graphemeBreakNames[] = {
    "XX", "CR", "LF", "CN", "EX", "ZWJ", "RI", "PP", "SM", "L", "V", "T", "LV", "LVT",
};
static const char* const s_wordBreakNames[] = {
    "XX", "CR", "LF", "NL", "Extend", "ZWJ", "RI", "FO", "KA",        "HL",
    "LE", "SQ", "DQ", "MB", "ML",     "MN",  "NU", "EX", "WSegSpace",
};
static const char* const s_sentenceBreakNames[] = {
    "XX", "CR", "LF", "EX", "SE", "FO", "SP", "LO", "UP", "LE", "NU", "AT", "SC", "ST", "CL",
};
static const char* const s_indicConjunctBreakNames[] = {
    "None",
    "Linker",
    "Consonant",
    "Extend",
};

#define COUNT_OF(array) ((int)(sizeof(array) / sizeof((array)[0])))

static const char* const s_lineBreakNames[] = {
    "XX", "BK", "CR", "LF", "CM", "NL", "SG", "WJ", "ZW", "GL", "SP", "ZWJ", "B2",
    "BA", "BB", "HY", "CB", "CL", "CP", "EX", "IN", "NS", "OP", "QU", "IS",  "NU",
    "PO", "PR", "SY", "AI", "AL", "CJ", "EB", "EM", "H2", "H3", "HL", "ID",  "JL",
    "JV", "JT", "RI", "SA", "AK", "AP", "AS", "VF", "VI", "HH",
};
static const char* const s_eastAsianWidthNames[] = {"N", "A", "H", "W", "F", "Na"};
static const char* const s_bidiClassNames[] = {
    "L",  "R",  "AL",  "EN",  "ES",  "ET",  "AN",  "CS",  "NSM", "BN",  "B",   "S",
    "WS", "ON", "LRE", "LRO", "RLE", "RLO", "PDF", "LRI", "RLI", "FSI", "PDI",
};

// The short names of every value of property, in the order
// PropertyValueAliases.txt lists them.
static int AllShortNames(const char* property, char names[][8], int capacity)
{
    FILE* file = OpenUcd("PropertyValueAliases.txt");
    char line[MAX_LINE];
    int count = 0;
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* fields[6];
        if (line[0] == '#' || SplitFields(line, fields, 6) < 3 || strcmp(fields[0], property) != 0)
        {
            continue;
        }
        if (count == capacity || strlen(fields[1]) >= 8)
        {
            Fail("%s: too many values or a long short name", property);
        }
        snprintf(names[count], 8, "%s", fields[1]);
        count += 1;
    }
    fclose(file);
    return count;
}

// Canonical_Combining_Class, field 3 of UnicodeData.txt, stored as is.
static void LoadCombiningClass(uint8_t* values)
{
    FILE* file = OpenUcd("UnicodeData.txt");
    char line[MAX_LINE];
    memset(values, 0, CODE_POINTS);
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* fields[15];
        if (SplitFields(line, fields, 15) < 4 || fields[0][0] == '\0')
        {
            continue;
        }
        long value = strtol(fields[3], nullptr, 10);
        if (value < 0 || value > 254)
        {
            Fail("bad combining class '%s' in UnicodeData.txt", fields[3]);
        }
        values[ParseCodePoint(fields[0], "UnicodeData.txt")] = (uint8_t)value;
    }
    fclose(file);
}

// Writes the ISO 15924 tag of every script index into src/generated/,
// big-endian ASCII in a uint32_t ('Latn' is 0x4C61746E).
static void WriteScriptTags(char names[][8], int count)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/script_tags.c", s_outputDirectory);
    FILE* file = fopen(path, "w");
    if (file == nullptr)
    {
        Fail("cannot write %s", path);
    }
    fprintf(file, "// SPDX-License-Identifier: MIT\n// Copyright (c) 2026 Sirac Ozmen\n//\n");
    fprintf(file, "// The ISO 15924 tag of each Script value the Script table stores.\n");
    fprintf(file, "// Generated by tools/munigen.c from PropertyValueAliases.txt in tools/ucd/\n");
    fprintf(file, "// (Unicode 18.0.0); do not edit.\n// clang-format off\n\n");
    fprintf(file, "#include \"../tables.h\"\n\n");
    fprintf(file, "const uint32_t muniScriptTags[%d] = {", count);
    for (int i = 0; i < count; i++)
    {
        if (strlen(names[i]) != 4)
        {
            Fail("script '%s' has no four-letter tag", names[i]);
        }
        uint32_t tag = (uint32_t)(unsigned char)names[i][0] << 24 |
                       (uint32_t)(unsigned char)names[i][1] << 16 |
                       (uint32_t)(unsigned char)names[i][2] << 8 |
                       (uint32_t)(unsigned char)names[i][3];
        fprintf(file, "\n    0x%08Xu, // %s", tag, names[i]);
    }
    fprintf(file, "\n};\n");
    fclose(file);
}

// Bidi_Mirroring_Glyph from BidiMirroring.txt. The table stores, for
// each code point, 0 when it has no mirror, or 1 plus the index of the
// distance to its mirror in deltas, a list of the distinct distances in
// order of first use.
#define MAX_DELTAS 255

static int LoadMirroring(uint8_t* values, int32_t* deltas)
{
    FILE* file = OpenUcd("BidiMirroring.txt");
    char line[MAX_LINE];
    int count = 0;
    memset(values, 0, CODE_POINTS);
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* fields[3];
        if (line[0] == '#' || line[0] == '\n' || SplitFields(line, fields, 3) < 2)
        {
            continue;
        }
        uint32_t codePoint = ParseCodePoint(fields[0], "BidiMirroring.txt");
        int32_t delta =
            (int32_t)ParseCodePoint(fields[1], "BidiMirroring.txt") - (int32_t)codePoint;
        int index = 0;
        while (index < count && deltas[index] != delta)
        {
            index++;
        }
        if (index == count)
        {
            if (count == MAX_DELTAS)
            {
                Fail("BidiMirroring.txt: more than %d distinct distances", MAX_DELTAS);
            }
            deltas[count++] = delta;
        }
        values[codePoint] = (uint8_t)(index + 1);
    }
    fclose(file);
    return count;
}

static void WriteMirrorDeltas(const int32_t* deltas, int count)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/bidi_mirror_deltas.c", s_outputDirectory);
    FILE* file = fopen(path, "w");
    if (file == nullptr)
    {
        Fail("cannot write %s", path);
    }
    fprintf(file, "// SPDX-License-Identifier: MIT\n// Copyright (c) 2026 Sirac Ozmen\n//\n");
    fprintf(file, "// The distances from code points to their Bidi_Mirroring_Glyph; the\n");
    fprintf(file, "// BidiMirror table stores 1 plus an index into them. Generated by\n");
    fprintf(file, "// tools/munigen.c from BidiMirroring.txt in tools/ucd/ (Unicode 18.0.0);\n");
    fprintf(file, "// do not edit.\n// clang-format off\n\n");
    fprintf(file, "#include \"../tables.h\"\n\n");
    fprintf(file, "const int32_t muniBidiMirrorDeltas[%d] = {", count);
    for (int i = 0; i < count; i++)
    {
        fprintf(file, "%s%d,", i % 10 == 0 ? "\n    " : " ", deltas[i]);
    }
    fprintf(file, "\n};\n");
    fclose(file);
}

// Bidi_Paired_Bracket_Type from BidiBrackets.txt: 0 none, 1 open, 2
// close. The file's Bidi_Paired_Bracket must be the Bidi_Mirroring_Glyph,
// which the library then uses for both.
static void LoadBrackets(uint8_t* values, const uint8_t* mirrors, const int32_t* deltas)
{
    FILE* file = OpenUcd("BidiBrackets.txt");
    char line[MAX_LINE];
    memset(values, 0, CODE_POINTS);
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* fields[4];
        if (line[0] == '#' || line[0] == '\n' || SplitFields(line, fields, 4) < 3)
        {
            continue;
        }
        uint32_t codePoint = ParseCodePoint(fields[0], "BidiBrackets.txt");
        uint32_t pair = ParseCodePoint(fields[1], "BidiBrackets.txt");
        if (mirrors[codePoint] == 0 ||
            (uint32_t)((int32_t)codePoint + deltas[mirrors[codePoint] - 1]) != pair)
        {
            Fail("BidiBrackets.txt: U+%04X pairs with U+%04X, not its mirror", codePoint, pair);
        }
        values[codePoint] = strcmp(fields[2], "o") == 0 ? 1 : strcmp(fields[2], "c") == 0 ? 2 : 0;
    }
    fclose(file);
}

// General_Category from UnicodeData.txt. Ranges appear as a "<..., First>"
// line followed by its "<..., Last>" line; code points the file does not
// list are unassigned (Cn, value 0).
static const char* const s_generalCategoryNames[] = {
    "Cn", "Lu", "Ll", "Lt", "Lm", "Lo", "Mn", "Mc", "Me", "Nd", "Nl", "No", "Pc", "Pd", "Ps",
    "Pe", "Pi", "Pf", "Po", "Sm", "Sc", "Sk", "So", "Zs", "Zl", "Zp", "Cc", "Cf", "Cs", "Co",
};

static void LoadGeneralCategory(uint8_t* values)
{
    FILE* file = OpenUcd("UnicodeData.txt");
    char line[MAX_LINE];
    uint32_t rangeFirst = 0;
    int inRange = 0;
    int nameCount = COUNT_OF(s_generalCategoryNames);
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* fields[15];
        if (SplitFields(line, fields, 15) < 3 || fields[0][0] == '\0')
        {
            continue;
        }
        uint32_t codePoint = ParseCodePoint(fields[0], "UnicodeData.txt");
        uint8_t value = ValueIndex(s_generalCategoryNames, nameCount, fields[2], "UnicodeData.txt");
        size_t nameLength = strlen(fields[1]);
        if (nameLength > 8 && strcmp(fields[1] + nameLength - 8, ", First>") == 0)
        {
            rangeFirst = codePoint;
            inRange = 1;
            continue;
        }
        uint32_t first = inRange ? rangeFirst : codePoint;
        for (uint32_t c = first; c <= codePoint; c++)
        {
            values[c] = value;
        }
        inRange = 0;
    }
    fclose(file);
}

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

// Bits per stored value in the data level: the smallest of 1, 2, 4 and 8
// that holds every value.
static int ValueBits(uint32_t max)
{
    return max < 2 ? 1 : max < 4 ? 2 : max < 16 ? 4 : 8;
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
    uint8_t highValue;
} Table;

// The same walk as the lookup the generator writes.
static uint8_t Lookup(const Table* table, uint32_t codePoint)
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
    return (uint8_t)item;
}

static Table BuildTable(const uint8_t* values, const char* name)
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
static void WriteTable(const uint8_t* values, const char* fileName, const char* symbol,
                       const char* description, const char* inputs)
{
    Table table = BuildTable(values, symbol);
    const Plan* plan = &table.plan;
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.c", s_outputDirectory, fileName);
    FILE* file = fopen(path, "w");
    if (file == nullptr)
    {
        Fail("cannot write %s", path);
    }
    fprintf(file, "// SPDX-License-Identifier: MIT\n");
    fprintf(file, "// Copyright (c) 2026 Sirac Ozmen\n");
    fprintf(file, "//\n");
    fprintf(file, "// %s. Generated by tools/munigen.c from %s in tools/ucd/\n", description,
            inputs);
    fprintf(file, "// (Unicode 18.0.0); do not edit. %zu bytes of tables.\n", plan->bytes);
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
    fprintf(file, "uint8_t muniLookup%s(uint32_t codePoint)\n{\n", symbol);
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
    fprintf(file, "    return (uint8_t)item;\n}\n\n");
    fprintf(file, "const uint64_t muni%sHash = 0x%016llxull;\n", symbol,
            (unsigned long long)HashBytes(values, CODE_POINTS));
    fclose(file);
    printf("%-24s %7zu bytes  %d levels, blocks", symbol, plan->bytes, plan->splits + 1);
    for (int level = 0; level < plan->splits; level++)
    {
        printf(" %d", plan->blockBits[level]);
    }
    printf(", high start U+%04X\n", table.highStart);
    FreePlan(&table.plan);
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: munigen <ucd directory> <output directory>\n");
        return 2;
    }
    s_ucdDirectory = argv[1];
    s_outputDirectory = argv[2];
    uint8_t* values = Allocate(CODE_POINTS);

    LoadGeneralCategory(values);
    WriteTable(values, "general_category", "GeneralCategory", "General_Category",
               "UnicodeData.txt");
    LoadEnumerated("GraphemeBreakProperty.txt", "GCB", nullptr, s_graphemeBreakNames,
                   COUNT_OF(s_graphemeBreakNames), values);
    WriteTable(values, "grapheme_cluster_break", "GraphemeClusterBreak", "Grapheme_Cluster_Break",
               "GraphemeBreakProperty.txt");
    LoadEnumerated("WordBreakProperty.txt", "WB", nullptr, s_wordBreakNames,
                   COUNT_OF(s_wordBreakNames), values);
    WriteTable(values, "word_break", "WordBreak", "Word_Break", "WordBreakProperty.txt");
    LoadEnumerated("SentenceBreakProperty.txt", "SB", nullptr, s_sentenceBreakNames,
                   COUNT_OF(s_sentenceBreakNames), values);
    WriteTable(values, "sentence_break", "SentenceBreak", "Sentence_Break",
               "SentenceBreakProperty.txt");
    LoadEnumerated("DerivedCoreProperties.txt", "InCB", "InCB", s_indicConjunctBreakNames,
                   COUNT_OF(s_indicConjunctBreakNames), values);
    WriteTable(values, "indic_conjunct_break", "IndicConjunctBreak", "Indic_Conjunct_Break",
               "DerivedCoreProperties.txt");
    LoadBinary("emoji-data.txt", "Extended_Pictographic", values);
    WriteTable(values, "extended_pictographic", "ExtendedPictographic", "Extended_Pictographic",
               "emoji-data.txt");
    LoadEnumerated("LineBreak.txt", "lb", nullptr, s_lineBreakNames, COUNT_OF(s_lineBreakNames),
                   values);
    WriteTable(values, "line_break", "LineBreak", "Line_Break", "LineBreak.txt");
    LoadEnumerated("EastAsianWidth.txt", "ea", nullptr, s_eastAsianWidthNames,
                   COUNT_OF(s_eastAsianWidthNames), values);
    WriteTable(values, "east_asian_width", "EastAsianWidth", "East_Asian_Width",
               "EastAsianWidth.txt");
    LoadEnumerated("DerivedBidiClass.txt", "bc", nullptr, s_bidiClassNames,
                   COUNT_OF(s_bidiClassNames), values);
    WriteTable(values, "bidi_class", "BidiClass", "Bidi_Class", "DerivedBidiClass.txt");
    LoadCombiningClass(values);
    WriteTable(values, "canonical_combining_class", "CombiningClass", "Canonical_Combining_Class",
               "UnicodeData.txt");
    static char scriptNames[256][8];
    int scriptCount = AllShortNames("sc", scriptNames, 256);
    const char* scriptList[256];
    for (int i = 0; i < scriptCount; i++)
    {
        scriptList[i] = scriptNames[i];
    }
    LoadEnumerated("Scripts.txt", "sc", nullptr, scriptList, scriptCount, values);
    WriteTable(values, "script", "Script", "Script", "Scripts.txt");
    WriteScriptTags(scriptNames, scriptCount);
    static int32_t deltas[MAX_DELTAS];
    int deltaCount = LoadMirroring(values, deltas);
    WriteTable(values, "bidi_mirror", "BidiMirror", "Bidi_Mirroring_Glyph distances",
               "BidiMirroring.txt");
    WriteMirrorDeltas(deltas, deltaCount);
    uint8_t* mirrors = Allocate(CODE_POINTS);
    memcpy(mirrors, values, CODE_POINTS);
    LoadBrackets(values, mirrors, deltas);
    free(mirrors);
    WriteTable(values, "bidi_bracket", "BidiBracket", "Bidi_Paired_Bracket_Type",
               "BidiBrackets.txt");

    free(values);
    return 0;
}
