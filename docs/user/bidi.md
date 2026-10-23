# Bidirectional text

Hebrew, Arabic and other right-to-left scripts mix with left-to-right text and numbers. The Unicode Bidirectional Algorithm (UAX #9) decides the display order, and `bidi.h` implements it in caller memory.

## Resolving a paragraph

`muniResolveBidi` resolves one paragraph and gives every byte an embedding level: even levels run left to right, odd ones right to left. It needs one level byte and one workspace byte per byte of text, both from the caller:

```c
size_t paragraphLength = 0;
uint8_t paragraphLevel = 0;
muniResult status = muniResolveBidi(text, length, muni_bidiAuto, levels, workspace,
                                    &paragraphLength, &paragraphLevel);
```

A paragraph ends after its first paragraph separator (LF, CR LF, U+2029 and the like) or at the end of the text; call again on the rest for the next paragraph. `muni_bidiAuto` takes the direction from the first strong character; a program that knows the direction, such as a right-to-left user interface, passes `muni_bidiLeftToRight` or `muni_bidiRightToLeft`.

## Ordering lines

After line breaking (see [Characters, words, sentences and lines](segmentation.md)), `muniReorderBidiLine` orders each line: it writes the line's runs of equal level from left to right, after putting trailing whitespace back at the paragraph level (rule L1). A renderer lays out the runs in that order. The glyphs of an odd run go right to left, and characters with a `muniGetMirroringGlyph`, such as parentheses, are drawn mirrored there.

`muniReorderBidiLevels` computes a visual order for any units the caller has levels for (code points, clusters or glyphs), and `muniInvertBidiMap` turns it into a logical-to-visual map for hit testing. `samples/bidi.c` shows the whole path.

## Not obvious

- Levels are per byte, so they index directly by the byte offsets shaping and segmentation report. Characters that rule X9 removes, such as U+202A to U+202E, take the level of the character before them.
- The algorithm's own limits bound the work: 125 embedding levels and 63 open brackets. When more than 63 brackets are open at once, the run of text they are in gets no bracket pairs at all, as rule BD16 says.
- `muniReorderBidiLine` takes the line's text as well as its levels, because rule L1 depends on which characters are whitespace, tabs and separators.
- When `muniReorderBidiLine` reports `muni_errorCapacity` it writes no run; call it with a NULL array and capacity 0 first to learn the count.
