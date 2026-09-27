# muni-0015. Emoji properties, White_Space and decimal digits

Status: Accepted

## Context

The requirements list emoji properties (UTS #51), whitespace classes
and numeric values where needed among the character properties.
Extended_Pictographic already had a table of its own (568 bytes) for
segmentation. The other emoji properties are Emoji,
Emoji_Presentation, Emoji_Modifier, Emoji_Modifier_Base and
Emoji_Component. White_Space holds 25 code points. The security
component (muni-0014) kept the zeros of the decimal digit systems for
mixed-number detection.

## Decision

The six emoji properties share one table of bits, Extended_Pictographic
in the lowest (1,653 bytes); a table for the five new ones beside the
old one would take 2,033. White_Space gets a table of its own (95
bytes). Decimal digit values (Numeric_Type Decimal, which is General
Category Nd) come from the sorted zeros of the 77 digit systems, which
move from the security data into the core (310 bytes): a digit's value
is its distance from the greatest zero at or below it, which the
generator checks for every digit. The security tables shrink to
36,289 bytes, under their ceiling.

Other numeric values (superscripts, fractions, Roman numerals) are left
out: nothing in the requirements parses them, and Numeric_Value would
need a table of its own.

## Consequences

Segmentation reads one bit of the emoji table where it read the old
table, at no measurable cost. The core grows by 1,490 bytes for eight
new functions.
