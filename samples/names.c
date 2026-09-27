// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A user name policy of the kind a game or chat service needs, built
// from the library's parts. A name is first put in NFKC, so full-width
// letters and other compatibility forms become their plain selves; then
//
// - it must be at most moderately restrictive in how it mixes scripts
//   (UTS #39), with digits of one decimal system;
// - two names are the same account when their NFKC_Casefold keys match,
//   the comparison UAX #31 gives identifiers, so "Alice", "ALICE" and a
//   full-width "ALICE" collide;
// - a name is refused when its skeleton matches a taken one's, so a
//   Cyrillic a cannot pass for a Latin one.
//
// Each candidate is checked against the names accepted before it.

#include "maul-unicode/case.h"
#include "maul-unicode/normalize.h"
#include "maul-unicode/security.h"

#include <stdio.h>
#include <string.h>

#define MAX_NAME  64
#define MAX_OUT   (MAX_NAME * 18 * 4)
#define MAX_TAKEN 16

typedef struct Name
{
    char key[MAX_OUT];
    size_t keyLength;
    char skeleton[MAX_OUT];
    size_t skeletonLength;
} Name;

// Whether two byte strings are equal.
static bool Equal(const char* a, size_t aLength, const char* b, size_t bLength)
{
    return aLength == bLength && memcmp(a, b, aLength) == 0;
}

// The reason a candidate is refused, or NULL when it is accepted and
// joins the taken names.
static const char* Check(const char* input, Name* taken, size_t* takenCount)
{
    char name[MAX_OUT];
    size_t length = 0;
    if (strlen(input) > MAX_NAME ||
        muniNormalize(input, strlen(input), muni_nfkc, muni_convertStrict, name, MAX_OUT, &length)
                .status != muni_success)
    {
        return "not UTF-8, or too long";
    }
    muniRestrictionLevel level = muni_restrictionUnrestricted;
    bool mixedDigits = false;
    if (muniGetRestrictionLevel(name, length, &level).status != muni_success ||
        level > muni_restrictionModeratelyRestrictive)
    {
        return "mixes scripts, or has characters no identifier should";
    }
    if (muniCheckMixedNumbers(name, length, &mixedDigits).status != muni_success || mixedDigits)
    {
        return "mixes digits of two systems";
    }
    Name candidate;
    static uint8_t workspace[2 * MAX_OUT];
    if (muniToNfkcCasefold(name, length, muni_convertStrict, candidate.key, MAX_OUT,
                           &candidate.keyLength)
                .status != muni_success ||
        muniGetSkeleton(name, length, muni_bidiLeftToRight, workspace, candidate.skeleton, MAX_OUT,
                        &candidate.skeletonLength)
                .status != muni_success)
    {
        return "cannot be normalized";
    }
    for (size_t i = 0; i < *takenCount; i++)
    {
        if (Equal(taken[i].key, taken[i].keyLength, candidate.key, candidate.keyLength))
        {
            return "the same name as one taken";
        }
        if (Equal(taken[i].skeleton, taken[i].skeletonLength, candidate.skeleton,
                  candidate.skeletonLength))
        {
            return "looks like one taken";
        }
    }
    if (*takenCount == MAX_TAKEN)
    {
        return "no room for more names";
    }
    taken[(*takenCount)++] = candidate;
    return nullptr;
}

int main(void)
{
    static const char* const candidates[] = {
        "paypal",
        "Alice",
        "p\xD0\xB0ypal",                                                // a Cyrillic a
        "\xEF\xBC\xA1\xEF\xBC\xAC\xEF\xBC\xA9\xEF\xBC\xA3\xEF\xBC\xA5", // full-width ALICE
        "ALICE",
        "rnallory", // "rn" looks like "m"
        "mallory",
        "\xD0\xB8\xD0\xB2\xD0\xB0\xD0\xBD", // Cyrillic ivan
        "\xCE\xA9mega",                     // Greek Omega and Latin
        "user\xE0\xA5\xA7\x32",             // Devanagari one and ASCII two
        "tokyo\xE6\x9D\xB1\xE4\xBA\xAC",    // Latin and Han
    };
    static Name taken[MAX_TAKEN];
    size_t takenCount = 0;
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++)
    {
        const char* reason = Check(candidates[i], taken, &takenCount);
        printf("%s \"%s\"%s%s\n", reason == nullptr ? "accepted" : "refused ", candidates[i],
               reason == nullptr ? "" : ": ", reason == nullptr ? "" : reason);
    }
    return 0;
}
