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
    while (muniSegmentIterator_NextBreak(&iterator, &end) == muni_success)
    {
        UseCluster(text + start, end - start);
        start = end;
    }
}
```

`muniInitWordIterator` and `muniInitSentenceIterator` work the same way. After each boundary of a word iterator, `muniSegmentIterator_GetWordKind` tells what the segment before it holds: no word (spaces, punctuation, symbols), a number, letters, kana or ideographs, the highest among its code points. ICU's word rule statuses agree on a segment of letters or digits alone; on a mixed segment ICU reports the rule it matched last, so it calls "ab12" a number. Word movement and double-click selection skip the segments of kind `muni_wordNone`; letters with digits count as letters, and a superscript digit such as "²" is no number. `muniFindGraphemeBreaks`, `muniFindWordBreaks` and `muniFindSentenceBreaks` write every boundary of a whole text into an array instead, which is faster when the text is at hand.

To cut text to fit a buffer, `muniCutUtf8` gives the length of the longest prefix within a byte budget that ends at a code point or, with `muni_unitGraphemeCluster`, a grapheme cluster boundary. Cutting at a code point keeps the text well formed; cutting at a cluster also keeps a letter with its accents, an emoji sequence or a flag whole, which is what a text field or a preedit string should show.

## Line breaking

A line iterator reports each position where a line may end, and whether it must: after a line feed, a paragraph separator and the like, and at the end of the text.

```c
muniSegmentIterator lines;
if (muniInitLineIterator(&lines, text, length, false) == muni_success)
{
    size_t end = 0;
    bool mandatory = false;
    while (muniSegmentIterator_NextLineBreak(&lines, &end, &mandatory) == muni_success)
    {
        // a line may end at end; it must when mandatory is true
    }
}
```

A layout engine measures the text between opportunities and ends each line at the last opportunity that fits, or at the first mandatory one. Spaces before an opportunity belong to the line before it and are not drawn at its end. `muniFindLineBreaks` fills arrays of offsets and mandatory flags.

A line iterator breaks at one of the strictness levels of CSS's `line-break` property, set before its first break. Normal, the default, follows UAX #14. Strict breaks less in Chinese and Japanese, and loose breaks more: before the small kana and the prolonged sound mark (Line_Break=CJ), before iteration marks, between characters such as two ellipses and before U+2010 and U+2013 after an ideograph, and in Chinese and Japanese also before centered punctuation, before a postfix and after a prefix that is wide, fullwidth or of ambiguous width, such as "％" or "€". Anywhere breaks between any two grapheme clusters, for text such as long identifiers or URLs that would otherwise overflow. Some of these rules apply only to Chinese and Japanese, so the iterator needs the text's writing system, which the caller knows from its language:

```c
if (muniSegmentIterator_SetLineBreakStrictness(&lines, muni_lineBreakLoose) != muni_success ||
    muniSegmentIterator_SetWritingSystem(&lines, muni_writingChineseJapanese) != muni_success)
{
    // lines is not a line iterator, or has already started
}
```

The added breaks yield to what UAX #14 glues: a word joiner or no-break space, an opening bracket or quotation mark and what follows it, and the hard breaks, which stay mandatory at every level.

The levels follow CSS Text 3 as its Candidate Recommendation Draft of 14 August 2026 words them, so they differ from ICU's line breaking with the same `@lb=` keyword. ICU applies its Chinese and Japanese rules to Korean too; CSS Text 3 limits them to Chinese and Japanese, so Korean text, which takes `muni_writingOther`, breaks differently from ICU's Korean locale.

- Normal keeps Line_Break=CJ a nonstarter, so no line starts with a small kana or the prolonged sound mark. ICU's normal breaks before them, as earlier CSS drafts did; CSS now allows that break only in loose.
- In Chinese, Japanese and Korean, ICU takes the curly quotation marks U+2018, U+2019, U+201C and U+201D as opening and closing punctuation at every level, so it breaks before "“" after an ideograph and after "”" before one. Here they keep the rules UAX #14 gives quotation marks (LB19 and LB19a).
- Loose breaks before U+2010 and U+2013 after an ideograph in any language, as CSS allows; ICU does so only in Chinese, Japanese and Korean.
- Loose in Chinese and Japanese breaks before a wide or ambiguous postfix and after such a prefix, since CSS lists these breaks without further conditions: also in "a％", and in "￥…" before the ellipsis. Besides the glue above, a prefix keeps what rule LB13 keeps before it: closing punctuation, slashes, and exclamation and question marks but the fullwidth ones, which are centered punctuation, so "￥）" and "￥、" stay together. ICU, in Chinese, Japanese and Korean, makes no break after a prefix before an ellipsis, hyphen or comma, nor before a postfix after a letter; it also breaks before a prefix after a letter ("a￥"), which CSS does not list.

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
if (muniSegmentIterator_SetComplexBreaker(&lines, BreakThai, dictionary) != muni_success)
{
    // lines is not a line or word iterator, or has already started
}
```

The iterator then never breaks before a combining mark, whatever the segmenter answers, and a word iterator reports whole words. The segmenter must give the same answer for the same run and offset each time. It may not use its own iterator: while it runs, the setters, `muniSegmentIterator_Feed` and the `muniNext` calls on that iterator return `muni_errorBusy` and change nothing, and it must not initialize the iterator again.

## Language conventions

The classes Unicode assigns are the default. Some languages' conventions, as CLDR records them, give a few code points another class: Greek ends a sentence at the semicolon and at U+037E GREEK QUESTION MARK, and POSIX English keeps the full stop out of words while numbers keep it. A word or sentence iterator takes a table of such overrides before its first boundary:

```c
static const muniBreakClassOverride greek[] = {
    {0x003B, muni_sbSTerm},
    {0x037E, muni_sbSTerm},
};
muniSegmentIterator sentences;
if (muniInitSentenceIterator(&sentences, text, length, false) == muni_success &&
    muniSegmentIterator_SetBreakClassOverrides(&sentences, greek, 2) == muni_success)
{
    // a sentence now ends after "; " as after "? "
}
```

The entries are in strictly increasing order of code point, and their values are Word_Break classes (`muni_wb...`) for a word iterator and Sentence_Break classes (`muni_sb...`) for a sentence iterator. The table is read in place, so it must stay unchanged while the iterator is used. Conventions that only remove boundaries, such as CLDR's abbreviations after which a sentence goes on, need no override: the caller skips those boundaries.

## Text in pieces

An iterator initialized with `moreFollows` set stops with `muni_needMoreText` when the next boundary depends on text it has not seen. `muniSegmentIterator_Feed` then hands it the next piece, which may start in the middle of a UTF-8 sequence. The iterator keeps no pointer to earlier pieces, so a stream can be segmented through a small buffer, and offsets stay relative to the start of the whole text. Feeding before the iterator asked, or after the last piece, returns `muni_errorState`, since the unread rest of the piece would be lost; so does a setting such as `muniSegmentIterator_SetLineBreakStrictness` once the iterator was asked for a break.

## Not obvious

- Word and sentence boundaries are the standard's, which do not always fall on grapheme cluster boundaries: a Prepend character that is also numeric, such as U+0600, takes a word boundary after it before punctuation. A caller that needs both snaps one to the other.
- Between two word boundaries lies a word, a run of spaces or a punctuation mark. The caller tells them apart, for example by the General_Category of the first code point.
- Some rules look ahead. A word boundary may wait for the next code point ("can't", "3.14"), and a sentence boundary after an abbreviation's period may wait until the next letter shows whether the sentence goes on. With text in pieces, that is when `muni_needMoreText` comes back before a boundary that looks decided.
- A run of SA text lies within one piece; a piece boundary inside a run hands the run to the segmenter in parts.
