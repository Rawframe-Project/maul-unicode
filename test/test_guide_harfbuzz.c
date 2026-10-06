// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The guide's HarfBuzz snippet (docs/guide.md), as written there
// (tools/check_guide.py checks it, family record 0019): HarfBuzz's buffer
// takes Maul Unicode's functions. Built with MAUL_UNICODE_HARFBUZZ.

#include "test_harness.h"

#include "maul-unicode/harfbuzz.h"

#include <hb.h>

int main(void)
{
    hb_buffer_t* buffer = hb_buffer_create();
    hb_unicode_funcs_t* functions = muniCreateHarfBuzzFunctions();
    hb_buffer_set_unicode_funcs(buffer, functions);
    CHECK(functions != nullptr && hb_buffer_get_unicode_funcs(buffer) == functions,
          "the buffer's Unicode functions are Maul Unicode's");
    hb_buffer_destroy(buffer);
    hb_unicode_funcs_destroy(functions);
    return s_failures == 0 ? 0 : 1;
}
