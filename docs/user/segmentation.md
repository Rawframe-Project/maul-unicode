# Characters, words, sentences and lines

`segment.h` finds the boundaries of grapheme clusters, words and sentences (UAX #29) and the line break opportunities (UAX #14). All four use one iterator type, `muniSegmentIterator`, a plain struct the caller keeps on the stack that needs no cleanup.

## Grapheme clusters, words and sentences

A user perceives a grapheme cluster as one character: an emoji family joined by zero width joiners, a flag of two regional indicators, a letter with its accents, a Hindi conjunct. Caret movement, selection and backspace must never split one. Word boundaries serve double-click selection and word-by-word movement; sentence boundaries serve sentence selection and text to speech.

An iterator reports each boundary after the start of the text, the last one being the end of the text; empty text has none. Here each cluster goes to the program's `UseCluster`:

```c
muniSegmentIterator iterator;
if (muniInitGraphemeIterator(&iterator, text, length, false) == muni_success)
{
    size_t start = 0;
    size_t end = 0;
    while (muniNextSegmentBreak(&iterator, &end) == muni_success)
    {
        UseCluster(text + start, end - start);
        start = end;
    }
}
```

`muniInitWordIterator` and `muniInitSentenceIterator` work the same way. `muniFindGraphemeBreaks`, `muniFindWordBreaks` and `muniFindSentenceBreaks` write every boundary of a whole text into an array instead, which is faster when the text is at hand.

## Line breaking

A line iterator reports each position where a line may end, and whether it must: after a line feed, a paragraph separator and the like, and at the end of the text.

```c
muniSegmentIterator lines;
if (muniInitLineIterator(&lines, text, length, false) == muni_success)
{
    size_t end = 0;
    bool mandatory = false;
    while (muniNextLineBreak(&lines, &end, &mandatory) == muni_success)
    {
        // a line may end at end; it must when mandatory is true
    }
}
```

A layout engine measures the text between opportunities and ends each line at the last opportunity that fits, or at the first mandatory one. Spaces before an opportunity belong to the line before it and are not drawn at its end. `muniFindLineBreaks` fills arrays of offsets and mandatory flags.

Chinese and Japanese text often allows a break before the small kana, the prolonged sound mark and similar characters (Line_Break=CJ), which the default rules keep with the character before them. CSS calls the default strict line breaking, and its normal and loose line breaking allow those breaks. A line iterator takes them as ideographs with an option, set before its first break:

```c
if (muniSetLineOptions(&lines, muni_lineCjAsIdeographic) != muni_success)
{
    // lines is not a line iterator, or has already started
}
```

## Thai, Lao, Khmer and Burmese

These scripts write words without spaces (Line_Break class SA), so breaking inside them needs a dictionary, which the library does not carry. By default, as rule LB1 of UAX #14 allows, a run of SA text has no line break inside it, and word segmentation gives one word per cluster.

A program with a word segmenter hands it to a line or word iterator. The segmenter receives a maximal run of SA text and an offset inside it, and answers with the offset of the first word break after it, or the run's length when there is none:

```c
static size_t BreakThai(void* context, const char* run, size_t length, size_t from)
{
    const ThaiDictionary* dictionary = context;
    return FindWordBreak(dictionary, run, length, from);
}
```

It is set before the iterator is asked for its first break:

```c
if (muniSetComplexBreaker(&lines, BreakThai, dictionary) != muni_success)
{
    // lines is not a line or word iterator, or has already started
}
```

The iterator then never breaks before a combining mark, whatever the segmenter answers, and a word iterator reports whole words. The segmenter must give the same answer for the same run and offset each time.

## Language conventions

The classes Unicode assigns are the default. Some languages' conventions, as CLDR records them, give a few code points another class: Greek ends a sentence at the semicolon and at U+037E GREEK QUESTION MARK, and POSIX English keeps the full stop out of words while numbers keep it. A word or sentence iterator takes a table of such overrides before its first boundary:

```c
static const muniBreakClassOverride greek[] = {
    {0x003B, muni_sbSTerm},
    {0x037E, muni_sbSTerm},
};
muniSegmentIterator sentences;
if (muniInitSentenceIterator(&sentences, text, length, false) == muni_success &&
    muniSetBreakClassOverrides(&sentences, greek, 2) == muni_success)
{
    // a sentence now ends after "; " as after "? "
}
```

The entries are in strictly increasing order of code point, and their values are Word_Break classes (`muni_wb...`) for a word iterator and Sentence_Break classes (`muni_sb...`) for a sentence iterator. The table is read in place, so it must stay unchanged while the iterator is used. Conventions that only remove boundaries, such as CLDR's abbreviations after which a sentence goes on, need no override: the caller skips those boundaries.

## Text in pieces

An iterator initialized with `moreFollows` set stops with `muni_needMoreText` when the next boundary depends on text it has not seen. `muniFeedSegmentIterator` then hands it the next piece, which may start in the middle of a UTF-8 sequence. The iterator keeps no pointer to earlier pieces, so a stream can be segmented through a small buffer, and offsets stay relative to the start of the whole text. Feeding before the iterator asked, or after the last piece, returns `muni_errorState`, since the unread rest of the piece would be lost; so does a setting such as `muniSetLineOptions` once the iterator was asked for a break.

## Not obvious

- Word and sentence boundaries are the standard's, which do not always fall on grapheme cluster boundaries: a Prepend character that is also numeric, such as U+0600, takes a word boundary after it before punctuation. A caller that needs both snaps one to the other.
- Between two word boundaries lies a word, a run of spaces or a punctuation mark. The caller tells them apart, for example by the General_Category of the first code point.
- Some rules look ahead. A word boundary may wait for the next code point ("can't", "3.14"), and a sentence boundary after an abbreviation's period may wait until the next letter shows whether the sentence goes on. With text in pieces, that is when `muni_needMoreText` comes back before a boundary that looks decided.
- A run of SA text lies within one piece; a piece boundary inside a run hands the run to the segmenter in parts.
