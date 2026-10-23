# Security

The security component implements the mechanisms of UTS #39 that names need: confusable skeletons, resolved script sets, restriction levels and mixed numbers. It is optional and needs normalization, since a skeleton is built from NFD.

## Skeletons

UTS #39 compares strings by skeleton: NFD, default ignorables removed, each code point replaced by its prototype from `confusables.txt`, NFD again, all in display order (bidiSkeleton), where the bidirectional algorithm reorders the string, marks stay after their base and mirrored characters become their mirror. [`src/skeleton.c`](../../src/skeleton.c) does this in one pass in caller memory:

- The code points come in display order, as rules L1 to L4 of UAX #9 arrange them. The skeleton resolves each paragraph into the caller's workspace (two bytes per byte, as [bidi.md](bidi.md) explains) and walks the levels recursively, which orders the text without a map.
- When no character can open an odd level (R, AL, RLE, RLO, RLI) and the paragraph is not right to left, the display order is the logical order and bidi is skipped. Most names take this path.
- Two stages of canonical ordering follow. The first gives NFD of the text, whose code points (but the default ignorables) are replaced by their prototypes; the second gives NFD of those, which is the skeleton. Each stage holds a starter and its marks, as the normalizer does, with the same limit of 32.

## Prototype data

The 6,700 or so prototypes sit in a rank index like the decomposition data ([tables.md](tables.md#one-table-per-property)). Its blocks are found by binary search over their starts, with a direct hit where the first blocks are dense, because a block table would cost more than the index itself. An entry is 16 bits: a prototype of one code point below U+8000 is stored as itself, any other as its length and offset into a pool of UTF-16 units that equal prototypes share. Sources a skeleton never looks up are left out: those with a canonical decomposition, which NFD replaces first, and the default ignorables, which the skeleton drops before mapping.

## Script sets and levels

[`src/security.c`](../../src/security.c) works on script sets as 256-bit masks over Script indexes. The four writing systems that combine scripts (Hanb, Hntl, Jpan, Kore) take the top indexes, and the resolved set of a character is augmented with them as UTS #39 section 5.1 says. A Common or Inherited character without extensions fits every script and leaves a set alone.

The restriction level needs only Identifier_Status=Allowed per code point, a table of its own, and the Recommended scripts of UAX #31. Identifier_Type would cost several times as much and no mechanism needs more than the status, which the generator checks follows from the type; the types are left out.

Mixed-number detection uses the sorted zeros of the decimal digit systems that `muniGetDecimalDigitValue` uses too.

## What is left out

Whole-script and mixed-script confusable detection (UTS #39 sections 4.1 and 4.2) is not provided: done exactly, it needs the sets of strings confusable with a given one, data the security files do not include. An approximation would give answers a caller cannot rely on.

## Traps

- A confusables mapping of several code points is compared with its source only when it displays in logical order: bidi reorders a spelled-out prototype of right-to-left letters, and not the single code point it maps from. The security test leaves those out for that reason, and holds every other mapping to a shared skeleton that a second pass keeps.
- A skeleton depends on the Unicode version; callers are told to recompute stored ones when it changes. A table change here is a user-visible change and belongs in the changelog.
