# Identifiers and security

Two headers protect names: `identifier.h` decides what an identifier may contain (UAX #31), and `security.h` detects names that can pass for each other or mix scripts suspiciously (UTS #39). Security is an optional component that needs normalization; see [Build options](getting-started.md#build-options).

## Identifiers

`muniIsIdentifierStart` and `muniIsIdentifierContinue` give the XID_Start and XID_Continue properties; `muniIsPatternSyntax` and `muniIsPatternWhiteSpace` the characters a language reserves for syntax and spacing. `muniCheckIdentifier` reports where a string stops being a default identifier: an XID_Start code point followed by XID_Continue code points. A scripting language or a configuration format uses these for its names. Compare identifiers by their NFKC_Casefold, see [Normalization and case](normalization-and-case.md#comparing-strings).

## Names people read

For user names in chat, account names and the like:

- `muniGetSkeleton` maps a string to its skeleton. Two strings are confusable when their skeletons are equal: "paypal" written with a Cyrillic "а" has the skeleton of the Latin one. Store a skeleton next to each name and compare bytes.
- `muniGetRestrictionLevel` grades how a string mixes scripts under the General Security Profile, from ASCII only to unrestricted. Moderately restrictive (Latin with one other recommended script, but not Cyrillic or Greek) is a common limit for user names.
- `muniGetResolvedScripts` lists the scripts every character of a string can belong to; none means the string mixes scripts.
- `muniCheckMixedNumbers` finds digits from more than one decimal system, some of which look alike.
- `muniIsIdentifierAllowed` tells whether a code point has Identifier_Status Allowed.

`samples/names.c` combines these into a user name policy: NFKC first, then refuse mixed scripts and mixed digits, key the account by NFKC_Casefold, and refuse a name whose skeleton is taken.

## Not obvious

- A skeleton follows the order in which bidi displays the string, so it takes a paragraph direction and a workspace of two bytes per byte of text, with its size. A smaller workspace returns `muni_errorCapacity` with `neededOut` 0 for any text, even text that skips bidi. `muni_bidiLeftToRight` gives the skeleton UTS #39 calls skeleton; text without right-to-left characters has the same skeleton in automatic and left-to-right paragraphs, and skips bidi entirely.
- A skeleton is not text to show, and it changes with the Unicode version or a fixed rule. Store `muniGetResultsVersion` beside stored skeletons and recompute them when it changes.
- The restriction level does not check syntax: an identifier should also pass `muniCheckIdentifier`.
- Whole-script and mixed-script confusable detection (UTS #39 sections 4.1 and 4.2) is not provided. Done exactly, it needs the sets of strings confusable with a given one, which the Unicode security data does not include.
- `muniGetResolvedScripts` counts Han as also Japanese, Korean and Han with Bopomofo or Latin, as UTS #39 asks; 64 entries always suffice.
