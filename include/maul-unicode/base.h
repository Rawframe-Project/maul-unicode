// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The base of the Maul Unicode API: the library and Unicode versions,
// the export and attribute macros, and the result codes every fallible
// function returns.

#ifndef MAUL_UNICODE_BASE_H
#define MAUL_UNICODE_BASE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// The library version. CMake reads it from here.
#define MUNI_VERSION_MAJOR 0
#define MUNI_VERSION_MINOR 0
#define MUNI_VERSION_PATCH 1

// The one Unicode version every table in the library comes from.
#define MUNI_UNICODE_VERSION_MAJOR 18
#define MUNI_UNICODE_VERSION_MINOR 0
#define MUNI_UNICODE_VERSION_PATCH 0

// MUNI_API marks the public functions: dllexport or dllimport in a
// shared Windows build (maul_unicode_EXPORTS is defined while building
// the library), default visibility in a shared build elsewhere.
#if defined(MAUL_UNICODE_SHARED) && defined(_WIN32)
#if defined(maul_unicode_EXPORTS)
#define MUNI_API __declspec(dllexport) extern
#else
#define MUNI_API __declspec(dllimport) extern
#endif
#elif defined(MAUL_UNICODE_SHARED) && (defined(__GNUC__) || defined(__clang__))
#define MUNI_API __attribute__((visibility("default"))) extern
#else
#define MUNI_API extern
#endif

// MUNI_NODISCARD marks a function whose result must be read: every
// function that returns a status. The attribute is standard in C23 and
// C++17 and left out for older dialects.
#if defined(__cplusplus) && __cplusplus >= 201703L
#define MUNI_NODISCARD [[nodiscard]]
#elif !defined(__cplusplus) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define MUNI_NODISCARD [[nodiscard]]
#else
#define MUNI_NODISCARD
#endif

    // The status a fallible function returns. Zero is success, positive
    // values are outcomes that are not errors, negative values are errors.
    // The type has a fixed width so that result structs have one layout in
    // C and C++.
    typedef int32_t muniResult;

    enum
    {
        // The call did what was asked.
        muni_success = 0,
        // An argument is invalid: a null pointer where one is required, a
        // value out of range.
        muni_errorInvalid = -1,
        // A caller buffer or a named limit is too small for the result.
        muni_errorCapacity = -2,
    };

    // A library or Unicode version: major, minor and patch.
    typedef struct muniVersion
    {
        uint16_t major;
        uint16_t minor;
        uint16_t patch;
    } muniVersion;

    /// Returns the version of the library that was linked, which may differ
    /// from the MUNI_VERSION macros a program was compiled with.
    ///
    /// @return The library version.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_API muniVersion muniGetVersion(void);

    /// Returns the Unicode version every table in the library comes from.
    ///
    /// @return The Unicode version, 18.0.0 for this release.
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_API muniVersion muniGetUnicodeVersion(void);

    /// Returns the name of a result code, for diagnostics.
    ///
    /// @param result  Any value; an unknown one is named as such.
    /// @return A static, NUL-terminated string such as "muni_errorCapacity".
    /// @par Thread safety
    /// Safe from any thread.
    MUNI_API const char* muniResultName(muniResult result);

#ifdef __cplusplus
}
#endif

#endif // MAUL_UNICODE_BASE_H
