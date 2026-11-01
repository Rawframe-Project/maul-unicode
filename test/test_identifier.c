// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Identifiers: the table against the generator's hash over every code
// point, every code point against the UCD files, known values, and
// whole identifiers.

#include "tables.h"
#include "test_harness.h"

#include "maul-unicode/identifier.h"

#ifdef MUNI_UCD_DATA
#include "ucd.h"
#endif

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
    for (int i = 0; i < 2; i++)
    {
        uint32_t past = i == 0 ? 0x110000 : UINT32_MAX;
        CHECK(!muniIsIdentifierStart(past) && !muniIsIdentifierContinue(past) &&
                  !muniIsPatternSyntax(past) && !muniIsPatternWhiteSpace(past),
              "past U+10FFFF has no identifier property");
    }
}

#ifdef MUNI_UCD_DATA
// Every code point against the UCD files, read here without the
// generator.
static void TestTableAgainstTheUcd(void)
{
    static const struct
    {
        const char* file;
        const char* property;
        bool (*has)(uint32_t);
    } properties[] = {
        {"DerivedCoreProperties.txt", "XID_Start", muniIsIdentifierStart},
        {"DerivedCoreProperties.txt", "XID_Continue", muniIsIdentifierContinue},
        {"PropList.txt", "Pattern_Syntax", muniIsPatternSyntax},
        {"PropList.txt", "Pattern_White_Space", muniIsPatternWhiteSpace},
    };
    static uint8_t table[UCD_CODE_POINTS];
    for (size_t i = 0; i < sizeof(properties) / sizeof(properties[0]); i++)
    {
        memset(table, 0, sizeof(table));
        bool read = UcdFillBinary(properties[i].file, properties[i].property, table);
        int failures = 0;
        for (uint32_t c = 0; c < UCD_CODE_POINTS; c++)
        {
            if ((properties[i].has(c) ? 1 : 0) != table[c] && failures++ < 5)
            {
                printf("%s of U+%04X differs\n", properties[i].property, (unsigned)c);
            }
        }
        CHECK(read && failures == 0, properties[i].property);
    }
}
#endif

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
#ifdef MUNI_UCD_DATA
    TestTableAgainstTheUcd();
#endif
    TestIdentifiers();
    return s_failures == 0 ? 0 : 1;
}
