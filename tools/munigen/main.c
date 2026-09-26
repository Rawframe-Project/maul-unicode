// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// munigen: writes the property tables in src/generated/ from the Unicode
// Character Database files in tools/ucd/ (docs/adr/muni-0003 and
// muni-0004). For each property it builds the full array of values,
// searches the multi-level split that stores it in the fewest bytes,
// checks every code point through the finished table against the array,
// and writes the table with a hash of the array.
//
// usage: munigen <ucd directory> <output directory>
//
// A development tool: it runs on the maintainer's machine and in CI,
// never in a consumer's build, and it stops at the first problem.

#include "munigen.h"

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: munigen <ucd directory> <output directory>\n");
        return 2;
    }
    SetDirectories(argv[1], argv[2]);
    WriteProperties();
    WriteNormalization();
    return 0;
}
