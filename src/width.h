// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The columns a grapheme cluster takes in a fixed-cell font, for
// muniMeasureUtf8's column units.

#ifndef MAUL_UNICODE_SRC_WIDTH_H
#define MAUL_UNICODE_SRC_WIDTH_H

#include <stddef.h>
#include <stdint.h>

// The columns of the grapheme cluster cluster[0..length), length at least
// 1: 0, 1 or 2, decided by its first two code points as
// docs/internals/segmentation.md describes. ambiguousWide gives East Asian
// Ambiguous characters two columns instead of one.
size_t muniClusterWidth(const uint8_t* cluster, size_t length, bool ambiguousWide);

#endif // MAUL_UNICODE_SRC_WIDTH_H
