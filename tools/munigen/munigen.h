// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// What the parts of munigen share.

#ifndef MUNIGEN_H
#define MUNIGEN_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CODE_POINTS     0x110000u
#define MAX_LINE        4096
#define COUNT_OF(array) ((int)(sizeof(array) / sizeof((array)[0])))

// ucd.c: files, failure and parsing.

// Sets where the UCD files are read and the generated sources written.
void SetDirectories(const char* ucdDirectory, const char* outputDirectory);

// Prints the message and exits with status 1.
[[noreturn]] void Fail(const char* format, ...);

// Zeroed memory, or a failure.
void* Allocate(size_t size);

FILE* OpenUcd(const char* name);

// Opens a file in the output directory for writing, or fails.
FILE* CreateOutput(const char* fileName);

// Writes text as // comment lines of at most 79 columns, wrapped at
// spaces.
void WriteComment(FILE* file, const char* text);

// Splits a UCD line at ';' into at most fieldCapacity trimmed fields and
// returns how many there are. The line is modified.
int SplitFields(char* line, char** fields, int fieldCapacity);

uint32_t ParseCodePoint(const char* text, const char* context);

// Returns the index of name in names, or fails.
uint8_t ValueIndex(const char* const* names, int count, const char* name, const char* context);

// Parses "XXXX" or "XXXX..YYYY" into an inclusive range.
void ParseRange(const char* text, uint32_t* firstOut, uint32_t* lastOut, const char* context);

// An enumerated property read from a UCD file whose lines are
// "range ; value", or "range ; key ; value" when key is not null.
void LoadEnumerated(const char* fileName, const char* property, const char* key,
                    const char* const* names, int nameCount, uint8_t* values);

// A binary property: 1 for every code point a line of fileName lists
// with the given name, 0 elsewhere.
void LoadBinary(const char* fileName, const char* name, uint8_t* values);

// The short names of all values of a property, in the order
// PropertyValueAliases.txt lists them; returns how many there are.
int AllShortNames(const char* property, char names[][8], int capacity);

// table.c: tables.

// Writes src/generated/<fileName>.c for a property of 8-bit values: the
// lookup returns uint8_t, and the hash covers one byte per code point.
void WriteTable(const uint8_t* values, const char* fileName, const char* symbol,
                const char* description, const char* inputs);

// The same for 16-bit values: the lookup returns uint16_t, and the hash
// covers two bytes per code point, low byte first.
void WriteWideTable(const uint16_t* values, const char* fileName, const char* symbol,
                    const char* description, const char* inputs);

// Rank indexes (table.c): code points found through the 64-code-point
// blocks that hold any. A block table, or a binary search over the
// blocks' starts, gives a block its number; per block, a 64-bit map
// marks the code points listed, and the count of those before the block
// is its rank.

#define MAX_BLOCKS 1024

typedef struct Blocks
{
    uint16_t starts[MAX_BLOCKS]; // each block's first code point >> 6, sorted
    int count;
} Blocks;

// Adds the blocks of the listed code points.
void AddBlocks(Blocks* blocks, const uint32_t* sources, int count);

// Writes the block table: 1 plus the block's number in its code points,
// 0 elsewhere.
void WriteBlockTable(const Blocks* blocks, const char* fileName, const char* symbol,
                     const char* description, const char* inputs);

// Writes the blocks' starts as the array name.
void WriteStarts(FILE* file, const char* name, const Blocks* blocks);

// Writes the arrays <name>Bits and <name>Ranks over the sources, which
// are in code point order.
void WriteRanks(FILE* file, const char* name, const Blocks* blocks, const uint32_t* sources,
                int count);

// properties.c: every property table.
void WriteProperties(void);

// normalization.c: the decomposition and composition data.
void WriteNormalization(void);

// case.c: the case mappings and foldings.
void WriteCase(void);

// security.c: the data of UTS #39.
void WriteSecurity(void);

#endif // MUNIGEN_H
