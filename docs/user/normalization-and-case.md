# Normalization and case

The same text can be encoded more than one way, and text that differs only in case often has to compare equal. `normalize.h` and `case.h` cover both. Each is an optional component, on by default; see [Build options](getting-started.md#build-options).

## Normal forms

"é" is one code point, or "e" followed by a combining acute accent. `muniNormalize` puts text into one of the four normal forms of UAX #15: NFC and NFD for canonical equivalence, NFKC and NFKD also folding compatibility characters such as ligatures and full-width letters. Store and compare text in NFC.

`muniCheckNormalization` tells quickly whether text is already in a form: yes, no, or maybe (only normalizing tells). Most text already is, which saves the copy.

`muniDecomposePair` and `muniComposePair` work on single code points one level deep, the way shapers ask for them.

## Case

`muniConvertCase` lowercases, uppercases, titlecases or case folds text with the full mappings, which may change the length ("ß" uppercases to "SS"), and with the context rule of the final sigma. Titlecasing finds words by the word boundaries of UAX #29. A language selects extra rules:

- `muni_caseTurkic` for Turkish and Azerbaijani: dotted and dotless i.
- `muni_caseLithuanian`: the dot of an i is kept under other accents.
- `muni_caseGreek`: uppercasing drops accents and breathings but keeps a dialytika, and adds one where a dropped accent kept two vowels apart ("Μάιος" uppercases to "ΜΑΪΟΣ"); the eta that means "or" keeps its accent when it stands alone ("ή" to "Ή"): no cased letter precedes or follows it past case-ignorable characters, the context of the final sigma, and a character both cased and case-ignorable, such as U+0345 or U+037A, counts as a cased letter there, where ICU skips it as case-ignorable; an iota subscript becomes a capital iota. Greek capitals are written this way; the default rules keep every accent. Lowercasing, titlecasing and folding are the default ones.
- `muni_caseArmenian`: the ligature "և" uppercases to "ԵՎ" and titlecases to "Եվ", as Eastern Armenian spells it, where the default rules give "ԵՒ" and "Եւ".

`muniGetLowercase`, `muniGetUppercase`, `muniGetTitlecase` and `muniFoldCase` are the simple mappings of one code point; `muniIsCased` tells whether a code point has case.

## Comparing strings

To compare strings without regard to case, fold both and compare the results. To compare them as users see them, as UAX #31 compares identifiers, use `muniGetNfkcCasefold`: it folds case, turns compatibility forms into their plain selves, drops default ignorables such as a soft hyphen or a zero width space, and puts the result in NFC. It needs both components.

```c
size_t keyLength = 0;
muniTextResult result =
    muniGetNfkcCasefold(name, length, muni_convertStrict, key, sizeof(key), &keyLength);
// "Alice", "ALICE", a full-width "ALICE" and "Al" + soft hyphen + "ice"
// all get the key "alice".
```

## Not obvious

- Normalization reorders at most `MUNI_MAX_NORMALIZATION_SEGMENT` (32) code points at once: a starter and the combining marks after it, once fully decomposed. That leaves 31 marks after a starter, and fewer when the starter itself decomposes (U+01D8 is u and two marks). Longer runs are refused with `muni_errorLimit` and the offset of the mark that makes the run too long; real text carries a handful, and UAX #15's stream-safe text at most 30. Skeletons and NFKC_Casefold share the limit.
- Titlecasing is not idempotent when a combining mark titlecases to a letter (U+0345 to capital iota): the word boundaries move. The standard's definitions give this result.
- Lithuanian uppercasing drops one dot above after a soft-dotted letter, so uppercasing twice can drop a second dot.
- Greek uppercasing loses information on purpose: "άι" and "αϊ" both give "ΑΪ", so it is for display, not for keys; fold or use NFKC_Casefold to compare Greek text. It is not idempotent where a lone U+0345 or the micro sign U+00B5, which uppercase to capital iota and mu, is followed by Greek marks: the first pass keeps them all, and a second drops an accent, breathing, macron or vrachy among them, and a second dialytika, as in ICU. One dialytika after them stays on both passes.
- NFKC_Casefold is defined per code point, so it is not invariant under canonical equivalence where U+0345 is involved: U+1FB3 U+0301 and its NFD get different results, as in ICU.
- Normalized and case-folded keys change with the Unicode version, for new characters, and may change when a release fixes a rule; store `muniGetResultsVersion` beside them and recompute them when it changes.
