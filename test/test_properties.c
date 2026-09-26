// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Property lookups: every code point through the public API against the
// hash the generator computed from the UCD, and known values by hand.

#include "tables.h"
#include "test_harness.h"

#include "maul-unicode/properties.h"

static uint64_t HashAll(uint8_t (*lookup)(uint32_t))
{
    uint64_t hash = 0xcbf29ce484222325ull;
    for (uint32_t c = 0; c < 0x110000u; c++)
    {
        hash ^= lookup(c);
        hash *= 0x100000001b3ull;
    }
    return hash;
}

static uint8_t GeneralCategory(uint32_t codePoint)
{
    return muniGetGeneralCategory(codePoint);
}

static void TestGeneralCategoryMatchesTheUcdEverywhere(void)
{
    CHECK(HashAll(GeneralCategory) == muniGeneralCategoryHash, "hash over every code point");
}

static void TestGeneralCategoryKnownValues(void)
{
    CHECK(muniGetGeneralCategory('A') == muni_gcLu, "A is Lu");
    CHECK(muniGetGeneralCategory('a') == muni_gcLl, "a is Ll");
    CHECK(muniGetGeneralCategory('0') == muni_gcNd, "0 is Nd");
    CHECK(muniGetGeneralCategory(' ') == muni_gcZs, "space is Zs");
    CHECK(muniGetGeneralCategory(0x0378) == muni_gcCn, "U+0378 is unassigned");
    CHECK(muniGetGeneralCategory(0xD800) == muni_gcCs, "U+D800 is a surrogate");
    CHECK(muniGetGeneralCategory(0xE000) == muni_gcCo, "U+E000 is private use");
    CHECK(muniGetGeneralCategory(0x4E00) == muni_gcLo, "U+4E00, a CJK range, is Lo");
    CHECK(muniGetGeneralCategory(0x1F600) == muni_gcSo, "U+1F600 is So");
    CHECK(muniGetGeneralCategory(0x10FFFD) == muni_gcCo, "U+10FFFD is private use");
    CHECK(muniGetGeneralCategory(0x10FFFF) == muni_gcCn, "U+10FFFF is a noncharacter");
    CHECK(muniGetGeneralCategory(0x110000) == muni_gcCn, "past U+10FFFF is unassigned");
    CHECK(muniGetGeneralCategory(UINT32_MAX) == muni_gcCn, "UINT32_MAX is unassigned");
}

int main(void)
{
    TestGeneralCategoryMatchesTheUcdEverywhere();
    TestGeneralCategoryKnownValues();
    return s_failures == 0 ? 0 : 1;
}
