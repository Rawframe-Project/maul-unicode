// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4X's side of the instruction count comparison: icu4x_bench
// OPERATION FILE PASSES does the work of ours.c with ICU4X 2.4 and its
// compiled data, on UTF-8. The word and line segmenters are made with
// new_for_non_complex_scripts, without the dictionary and LSTM models
// the library has no counterpart of. Bidi is the unicode-bidi crate over
// ICU4X's Bidi_Class, the pairing ICU4X documents. Script runs have no
// ICU4X counterpart. A segmenter also reports the boundary at 0, which
// the counts leave out, as the other two programs do not report it.

use icu::casemap::CaseMapper;
use icu::locale::LanguageIdentifier;
use icu::normalizer::{ComposingNormalizerBorrowed, DecomposingNormalizerBorrowed};
use icu::segmenter::options::{LineBreakOptions, SentenceBreakInvariantOptions, WordBreakInvariantOptions};
use icu::segmenter::{GraphemeClusterSegmenter, LineSegmenter, SentenceSegmenter, WordSegmenter};
use std::hint::black_box;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let op = args[1].as_str();
    let text = std::fs::read_to_string(&args[2]).unwrap();
    let reps: usize = args[3].parse().unwrap();
    let mut sum = 0usize;
    match op {
        "grapheme" => {
            let s = GraphemeClusterSegmenter::new();
            for _ in 0..reps {
                sum += s.segment_str(black_box(&text)).count() - 1;
            }
        }
        "word" => {
            let s = WordSegmenter::new_for_non_complex_scripts(WordBreakInvariantOptions::default());
            for _ in 0..reps {
                sum += s.segment_str(black_box(&text)).count() - 1;
            }
        }
        "sentence" => {
            let s = SentenceSegmenter::new(SentenceBreakInvariantOptions::default());
            for _ in 0..reps {
                sum += s.segment_str(black_box(&text)).count() - 1;
            }
        }
        "line" => {
            let s = LineSegmenter::new_for_non_complex_scripts(LineBreakOptions::default());
            for _ in 0..reps {
                sum += s.segment_str(black_box(&text)).count() - 1;
            }
        }
        "nfc" | "nfkc" => {
            let n = if op == "nfc" {
                ComposingNormalizerBorrowed::new_nfc()
            } else {
                ComposingNormalizerBorrowed::new_nfkc()
            };
            let mut out = String::with_capacity(text.len() * 4);
            for _ in 0..reps {
                out.clear();
                n.normalize_to(black_box(&text), &mut out).unwrap();
                sum += out.len();
            }
        }
        "nfd" => {
            let n = DecomposingNormalizerBorrowed::new_nfd();
            let mut out = String::with_capacity(text.len() * 4);
            for _ in 0..reps {
                out.clear();
                n.normalize_to(black_box(&text), &mut out).unwrap();
                sum += out.len();
            }
        }
        "lower" | "upper" | "fold" => {
            let c = CaseMapper::new();
            let root = LanguageIdentifier::UNKNOWN;
            for _ in 0..reps {
                let r = match op {
                    "lower" => c.lowercase_to_string(black_box(&text), &root),
                    "upper" => c.uppercase_to_string(black_box(&text), &root),
                    _ => c.fold_string(black_box(&text)),
                };
                sum += r.len();
            }
        }
        "bidi" => {
            let adapter = icu_properties::CodePointMapData::<icu_properties::props::BidiClass>::new();
            for _ in 0..reps {
                let info = unicode_bidi::BidiInfo::new_with_data_source(&adapter, black_box(&text), None);
                for p in &info.paragraphs {
                    let (_, runs) = info.visual_runs(p, p.range.clone());
                    sum += runs.len();
                }
            }
        }
        _ => std::process::exit(2),
    }
    println!("{}", sum);
}
