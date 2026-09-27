// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Uses the installed library as a consumer would, in C17: the version,
// one property and, when installed, the HarfBuzz functions guessing the
// direction of Arabic text.

#include "maul-unicode/properties.h"

#ifdef PACKAGE_HARFBUZZ
#include "maul-unicode/harfbuzz.h"

#include <hb.h>
#endif

#include <stdio.h>

int main(void)
{
    int failures = 0;
    if (muniGetGeneralCategory('A') != muni_gcLu)
    {
        printf("FAIL: the general category of A\n");
        failures += 1;
    }
#ifdef PACKAGE_HARFBUZZ
    hb_unicode_funcs_t* functions = muniCreateHarfBuzzFunctions();
    hb_buffer_t* buffer = hb_buffer_create();
    hb_buffer_set_unicode_funcs(buffer, functions);
    hb_buffer_add_utf8(buffer, "\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x8A", -1, 0, -1);
    hb_buffer_guess_segment_properties(buffer);
    if (hb_buffer_get_direction(buffer) != HB_DIRECTION_RTL)
    {
        printf("FAIL: HarfBuzz guesses Arabic right to left\n");
        failures += 1;
    }
    hb_buffer_destroy(buffer);
    hb_unicode_funcs_destroy(functions);
#endif
    printf("package test: %d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
