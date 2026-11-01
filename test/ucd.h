// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Reading the files of tools/ucd/ in the data suites, without the
// generator, so that a suite can check what the library gives for every
// code point against the file it comes from. A line gives a code point
// or a range and the fields after it; an @missing line gives a value for
// the code points no data line lists.

#ifndef MUNI_TEST_UCD_H
#define MUNI_TEST_UCD_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UCD_CODE_POINTS 0x110000u
#define UCD_MAX_FIELDS  16

typedef struct UcdLine
{
    uint32_t first;
    uint32_t last;
    bool missing; // an @missing line
    int count;    // the fields after the code points
    char* fields[UCD_MAX_FIELDS];
} UcdLine;

// A property value as the files write it, short and long, and the value
// of the library's enumeration.
typedef struct UcdName
{
    const char* shortName;
    const char* longName;
    uint8_t value;
} UcdName;

static inline FILE* UcdOpen(const char* name)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", MUNI_UCD_DATA, name);
    FILE* file = fopen(path, "r");
    if (file == nullptr)
    {
        printf("%s does not open\n", path);
    }
    return file;
}

static inline char* UcdTrim(char* text)
{
    while (*text == ' ' || *text == '\t')
    {
        text++;
    }
    size_t length = strlen(text);
    while (length > 0 && strchr(" \t\r\n", text[length - 1]) != nullptr)
    {
        text[--length] = '\0';
    }
    return text;
}

// Reads the next data or @missing line of a file into *line, whose
// fields point into buffer; false at the end of the file.
static inline bool UcdNext(FILE* file, char* buffer, size_t size, UcdLine* line)
{
    while (fgets(buffer, (int)size, file) != nullptr)
    {
        char* text = buffer;
        if (strncmp(text, "\xEF\xBB\xBF", 3) == 0)
        {
            text += 3;
        }
        line->missing = strncmp(text, "# @missing:", 11) == 0;
        if (line->missing)
        {
            text += 11;
        }
        char* comment = strchr(text, '#');
        if (comment != nullptr)
        {
            *comment = '\0';
        }
        text = UcdTrim(text);
        if (*text == '\0')
        {
            continue;
        }
        char* end = nullptr;
        line->first = (uint32_t)strtoul(text, &end, 16);
        line->last = line->first;
        if (end[0] == '.' && end[1] == '.')
        {
            line->last = (uint32_t)strtoul(end + 2, &end, 16);
        }
        line->count = 0;
        char* field = strchr(end, ';');
        while (field != nullptr && line->count < UCD_MAX_FIELDS)
        {
            char* next = strchr(field + 1, ';');
            if (next != nullptr)
            {
                *next = '\0';
            }
            line->fields[line->count++] = UcdTrim(field + 1);
            field = next;
        }
        return true;
    }
    return false;
}

static inline bool UcdEndsWith(const char* text, const char* end)
{
    size_t length = strlen(text);
    size_t endLength = strlen(end);
    return length >= endLength && strcmp(text + length - endLength, end) == 0;
}

// The value of a name, short or long, or -1 when names has neither.
static inline int UcdValue(const UcdName* names, size_t count, const char* name)
{
    for (size_t i = 0; i < count; i++)
    {
        if (strcmp(names[i].shortName, name) == 0 || strcmp(names[i].longName, name) == 0)
        {
            return names[i].value;
        }
    }
    return -1;
}

// Sets table[c] to the value a file gives each code point: the @missing
// lines first, in file order, then the data lines. The value is field
// `field` of a line; with a property, only lines whose field 0 names it
// count. A value names does not list is printed and makes it false.
static inline bool UcdFill(const char* fileName, const char* property, int field,
                           const UcdName* names, size_t count, uint8_t* table)
{
    FILE* file = UcdOpen(fileName);
    if (file == nullptr)
    {
        return false;
    }
    static char buffer[4096];
    bool known = true;
    for (int pass = 0; pass < 2; pass++)
    {
        rewind(file);
        UcdLine line;
        while (UcdNext(file, buffer, sizeof(buffer), &line))
        {
            if (line.missing != (pass == 0) || line.count <= field ||
                (property != nullptr && strcmp(line.fields[0], property) != 0))
            {
                continue;
            }
            int value = UcdValue(names, count, line.fields[field]);
            if (value < 0)
            {
                printf("%s: no value named %s\n", fileName, line.fields[field]);
                known = false;
                continue;
            }
            for (uint32_t c = line.first; c <= line.last && c < UCD_CODE_POINTS; c++)
            {
                table[c] = (uint8_t)value;
            }
        }
    }
    fclose(file);
    return known;
}

// Sets table[c] to 1 for the code points a file gives a binary property,
// and leaves the others as they are.
static inline bool UcdFillBinary(const char* fileName, const char* property, uint8_t* table)
{
    FILE* file = UcdOpen(fileName);
    if (file == nullptr)
    {
        return false;
    }
    static char buffer[4096];
    UcdLine line;
    while (UcdNext(file, buffer, sizeof(buffer), &line))
    {
        if (!line.missing && line.count == 1 && strcmp(line.fields[0], property) == 0)
        {
            for (uint32_t c = line.first; c <= line.last && c < UCD_CODE_POINTS; c++)
            {
                table[c] = 1;
            }
        }
    }
    fclose(file);
    return true;
}

// Counts the code points where get differs from table, and prints the
// first few.
static inline int UcdCompare(const char* what, uint8_t (*get)(uint32_t), const uint8_t* table)
{
    int failures = 0;
    for (uint32_t c = 0; c < UCD_CODE_POINTS; c++)
    {
        uint8_t value = get(c);
        if (value != table[c] && failures++ < 5)
        {
            printf("%s of U+%04X: %u, the UCD gives %u\n", what, (unsigned)c, value, table[c]);
        }
    }
    return failures;
}

#endif // MUNI_TEST_UCD_H
