# Script runs and shaping

A shaper such as HarfBuzz shapes one script at a time. `script.h` splits text into runs of one script, and the optional `maul-unicode-harfbuzz` library gives HarfBuzz its Unicode data from Maul Unicode.

## Script runs

A script iterator resolves Common and Inherited characters (spaces, punctuation, digits, combining marks) into the run around them, narrows a run by Script_Extensions where a character is shared by a few scripts, and keeps a closing bracket in the script of its opening bracket:

```c
muniScriptIterator scripts;
if (muniInitScriptIterator(&scripts, text, length, false) == muni_success)
{
    muniScriptRun run;
    while (muniNextScriptRun(&scripts, &run) == muni_success)
    {
        // the text up to run.end has script run.script
    }
}
```

Like the segment iterators, it takes text in pieces through `muniFeedScriptIterator` after `muni_needMoreText`. `muniFindScriptRuns` writes the runs of a whole text into an array.

A text layout pipeline itemizes by paragraph ([bidi](bidi.md)), then by script and bidi run, then by font, and shapes each item.

## HarfBuzz

Built with `-DMAUL_UNICODE_HARFBUZZ=ON`, the static library `maul-unicode-harfbuzz` provides `muniCreateHarfBuzzFunctions`, which answers HarfBuzz's questions about general category, combining class, mirroring, script, composition and decomposition from Maul Unicode. Shaping, segmentation and bidi then agree on one Unicode version.

```c
hb_unicode_funcs_t* functions = muniCreateHarfBuzzFunctions();
hb_buffer_set_unicode_funcs(buffer, functions);
```

The functions are immutable and may be shared between threads; release them with `hb_unicode_funcs_destroy`. Installed, the library is a package of its own: `find_package(maul-unicode-harfbuzz)` with the target `maul-unicode::maul-unicode-harfbuzz`, or the pkg-config module `maul-unicode-harfbuzz`, both of which bring in HarfBuzz.

## Not obvious

- Leading Common characters take the script of the first run. A text of Common and Inherited characters only is one run of `MUNI_SCRIPT_COMMON`.
- The bracket stack holds 32 opening brackets; when it is full the oldest is forgotten, and its closing bracket then joins the run around it like any other neutral character.
- Script tags have the same values as HarfBuzz's `hb_script_t`, so a run's script passes to `hb_buffer_set_script` unchanged.
