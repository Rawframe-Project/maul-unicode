// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Reading the Unicode Character Database files and the helpers every
// part of the generator shares.

#include "munigen.h"

static const char* s_ucdDirectory;
static const char* s_outputDirectory;

void SetDirectories(const char* ucdDirectory, const char* outputDirectory)
{
    s_ucdDirectory = ucdDirectory;
    s_outputDirectory = outputDirectory;
}

[[noreturn]] void Fail(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    fputs("munigen: ", stderr);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
    exit(1);
}

void* Allocate(size_t size)
{
    void* memory = calloc(1, size);
    if (memory == nullptr)
    {
        Fail("out of memory");
    }
    return memory;
}

FILE* CreateOutput(const char* fileName)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", s_outputDirectory, fileName);
    FILE* file = fopen(path, "w");
    if (file == nullptr)
    {
        Fail("cannot write %s", path);
    }
    return file;
}

// Parsing the UCD.

FILE* OpenUcd(const char* name)
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
int SplitFields(char* line, char** fields, int fieldCapacity)
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

uint32_t ParseCodePoint(const char* text, const char* context)
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
uint8_t ValueIndex(const char* const* names, int count, const char* name, const char* context)
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
void ParseRange(const char* text, uint32_t* firstOut, uint32_t* lastOut, const char* context)
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
void LoadEnumerated(const char* fileName, const char* property, const char* key,
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
void LoadBinary(const char* fileName, const char* name, uint8_t* values)
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

// The short names of every value of property, in the order
// PropertyValueAliases.txt lists them.
int AllShortNames(const char* property, char names[][8], int capacity)
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
