// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Identifiers: the table against the generator's hash over every code
// point, known values, and whole identifiers.

#include "tables.h"
#include "test_harness.h"

#include "maul-unicode/identifier.h"

#include <string.h>

static void TestTable(void)
{
    uint64_t hash = 0xcbf29ce484222325ull;
    for (uint32_t codePoint = 0; codePoint < 0x110000; codePoint++)
    {
        hash ^= muniLookupIdentifier(codePoint);
        hash *= 0x100000001b3ull;
    }
    CHECK(hash == muniIdentifierHash, "identifier bits match the generator");
    CHECK(muniIsIdentifierStart('a') && !muniIsIdentifierStart('1'), "a starts, 1 does not");
    CHECK(muniIsIdentifierContinue('1') && muniIsIdentifierContinue('_'), "1 and _ continue");
    CHECK(muniIsIdentifierStart(0x00E9) && muniIsIdentifierStart(0x4E00), "e acute and a Han");
    CHECK(!muniIsIdentifierStart(0x0301) && muniIsIdentifierContinue(0x0301),
          "a combining mark continues but does not start");
    CHECK(muniIsPatternSyntax('+') && !muniIsPatternSyntax('a'), "+ is syntax");
    CHECK(muniIsPatternWhiteSpace(' ') && muniIsPatternWhiteSpace(0x2029), "white space");
    CHECK(!muniIsPatternWhiteSpace(0x00A0), "no-break space is not pattern white space");
}

static muniResult Check(const char* text)
{
    return muniCheckIdentifier(text, strlen(text)).status;
}

static void TestIdentifiers(void)
{
    CHECK(Check("caf\xC3\xA9") == muni_success, "café");
    CHECK(Check("x1_y") == muni_success, "x1_y");
    CHECK(Check("\xE5\xA4\x89\xE6\x95\xB0") == muni_success, "a Japanese name");
    muniTextResult result = muniCheckIdentifier("1abc", 4);
    CHECK(result.status == muni_errorIdentifier && result.offset == 0, "a digit cannot start");
    result = muniCheckIdentifier("a-b", 3);
    CHECK(result.status == muni_errorIdentifier && result.offset == 1, "a hyphen breaks it");
    CHECK(Check("") == muni_errorIdentifier, "empty text is not an identifier");
    CHECK(muniCheckIdentifier("a\xFF", 2).status == muni_errorUtf8Lead, "ill-formed input");
    CHECK(muniCheckIdentifier(nullptr, 1).status == muni_errorInvalid, "NULL text");
}

int main(void)
{
    TestTable();
    TestIdentifiers();
    return s_failures == 0 ? 0 : 1;
}
