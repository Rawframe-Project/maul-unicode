// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The property tables tools/munigen.c writes into src/generated/. Each
// property has a lookup with its layout compiled in, and the hash of
// its value for every code point as the generator computed it from the
// UCD: FNV-1a 64 over one byte per code point from U+0000 to U+10FFFF.

#ifndef MAUL_UNICODE_SRC_TABLES_H
#define MAUL_UNICODE_SRC_TABLES_H

#include <stdint.h>

uint8_t muniLookupGeneralCategory(uint32_t codePoint);
extern const uint64_t muniGeneralCategoryHash;
uint8_t muniLookupGraphemeClusterBreak(uint32_t codePoint);
extern const uint64_t muniGraphemeClusterBreakHash;
uint8_t muniLookupWordBreak(uint32_t codePoint);
extern const uint64_t muniWordBreakHash;
uint8_t muniLookupSentenceBreak(uint32_t codePoint);
extern const uint64_t muniSentenceBreakHash;
uint8_t muniLookupIndicConjunctBreak(uint32_t codePoint);
extern const uint64_t muniIndicConjunctBreakHash;
uint8_t muniLookupExtendedPictographic(uint32_t codePoint);
extern const uint64_t muniExtendedPictographicHash;

uint8_t muniLookupLineBreak(uint32_t codePoint);
extern const uint64_t muniLineBreakHash;
uint8_t muniLookupEastAsianWidth(uint32_t codePoint);
extern const uint64_t muniEastAsianWidthHash;
uint8_t muniLookupBidiClass(uint32_t codePoint);
extern const uint64_t muniBidiClassHash;
uint8_t muniLookupCombiningClass(uint32_t codePoint);
extern const uint64_t muniCombiningClassHash;
// The Script table stores an index into muniScriptTags.
uint8_t muniLookupScript(uint32_t codePoint);
extern const uint64_t muniScriptHash;
extern const uint32_t muniScriptTags[];
// The ScriptExtensions table stores 0 when a code point's extensions are
// its Script alone, or 1 plus the index of a set: set i holds the Script
// indexes muniScriptSetMembers[muniScriptSetStarts[i]] up to
// muniScriptSetStarts[i + 1].
uint8_t muniLookupScriptExtensions(uint32_t codePoint);
extern const uint64_t muniScriptExtensionsHash;
extern const uint16_t muniScriptSetStarts[];
extern const uint8_t muniScriptSetMembers[];

// The BidiMirror table stores 0, or 1 plus an index into
// muniBidiMirrorDeltas, the distance to the Bidi_Mirroring_Glyph.
uint8_t muniLookupBidiMirror(uint32_t codePoint);
extern const uint64_t muniBidiMirrorHash;
extern const int32_t muniBidiMirrorDeltas[];
// Bidi_Paired_Bracket_Type: 0 none, 1 open, 2 close. The paired bracket
// is the Bidi_Mirroring_Glyph.
uint8_t muniLookupBidiBracket(uint32_t codePoint);
extern const uint64_t muniBidiBracketHash;

#endif // MAUL_UNICODE_SRC_TABLES_H
