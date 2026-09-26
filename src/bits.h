// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Bit counting on 64-bit words, through the compiler builtins GCC and
// Clang (clang-cl included) provide; F5 rules out <stdbit.h>.

#ifndef MAUL_UNICODE_SRC_BITS_H
#define MAUL_UNICODE_SRC_BITS_H

#include <stdint.h>

static inline unsigned muniCountOnes(uint64_t bits)
{
    return (unsigned)__builtin_popcountll(bits);
}

// The index of the lowest set bit; bits must not be 0.
static inline unsigned muniLowestBit(uint64_t bits)
{
    return (unsigned)__builtin_ctzll(bits);
}

#endif // MAUL_UNICODE_SRC_BITS_H
