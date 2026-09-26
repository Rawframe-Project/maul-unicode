// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Versions and result names.

#include "maul-unicode/base.h"

muniVersion muniGetVersion(void)
{
    return (muniVersion){MUNI_VERSION_MAJOR, MUNI_VERSION_MINOR, MUNI_VERSION_PATCH};
}

muniVersion muniGetUnicodeVersion(void)
{
    return (muniVersion){MUNI_UNICODE_VERSION_MAJOR, MUNI_UNICODE_VERSION_MINOR,
                         MUNI_UNICODE_VERSION_PATCH};
}

const char* muniResultName(muniResult result)
{
    switch (result)
    {
    case muni_success:
        return "muni_success";
    case muni_errorInvalid:
        return "muni_errorInvalid";
    case muni_errorCapacity:
        return "muni_errorCapacity";
    default:
        return "unknown result";
    }
}
