# muni-0014. Security data and skeletons

Status: Accepted

## Context

UTS #39 compares strings by skeleton: NFD, default ignorables removed,
each code point replaced by its prototype from confusables.txt (6,712
mappings, prototypes of up to 18 code points), NFD again, all in
display order (bidiSkeleton): the bidirectional algorithm reorders the
string, marks move after their base and mirrored characters become
their mirror. Restriction levels
need Identifier_Status, the Script_Extensions of every character with
the writing systems Hanb, Hntl, Jpan and Kore added, and the
Recommended scripts of UAX #31. Muni-0006 made the component optional.

## Decision

The component is `MAUL_UNICODE_SECURITY`, on by default, with the
header `security.h`: it covers the mechanisms of UTS #39 beyond
confusables. It needs normalization, and the build refuses the
combination without it.

The prototypes sit in a rank index like the normalization data
(muni-0011). Its 597 blocks are found by binary search over their
starts, with a direct hit where the first blocks are dense: a block
table would cost 17,942 bytes, more than the index. An entry is 16
bits: a prototype of one code point below U+8000 is stored as itself,
any other as its length and offset into a pool of UTF-16 units shared
by equal prototypes. Index, entries and pool take 30,408 bytes.

A table of two bits per code point (5,885 bytes) gives
Identifier_Status=Allowed and Default_Ignorable_Code_Point. The sets of
Identifier_Type would take 18,306 bytes; no mechanism of UTS #39 needs
more than the status, which the generator checks follows from the type,
so the types are left out. The zeros of the 77 decimal digit systems
(308 bytes) serve mixed-number detection. The whole component is
36,601 bytes of tables, which becomes its ceiling in muni-0008.

The skeleton skips bidi when no character can open an odd level (R,
AL, RLE, RLO, RLI) and the paragraph is not right to left: the display
order is then the logical order. Otherwise it resolves each paragraph
in the caller's workspace (muni-0010), two bytes per byte of text, and
walks the levels recursively, which orders the text without a map.

Whole-script and mixed-script confusable detection (sections 4.1 and
4.2) is not provided: done exactly, it needs the sets of strings
confusable with a given one, data the security files do not include.

## Consequences

A skeleton costs about 34 MiB/s on mixed text with right-to-left
paragraphs and 120 MiB/s on ASCII (GCC 14). The component adds 47.8 KB
to the static library. Tests hold every mapping that displays in
logical order, and every single-code-point mapping, to a shared
skeleton that a second pass keeps.
