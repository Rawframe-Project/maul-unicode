// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Property lookups over the generated tables.

#include "maul-unicode/properties.h"

#include "tables.h"

muniGeneralCategory muniGetGeneralCategory(uint32_t codePoint)
{
    return muniLookupGeneralCategory(codePoint);
}
