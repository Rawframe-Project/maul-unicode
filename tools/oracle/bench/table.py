#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Reads measure.sh's lines (corpus, operation, program, instructions per
# byte times 100, count per pass) from standard input and prints them as
# a Markdown table: the library through its iterators and through
# muniFind*Breaks, ICU4C, ICU4X, and the library's better figure over the
# faster reference's. A row whose counts differ says so.
import collections
import sys

TEXTS = {"latin": "Latin", "world": "ru el ar he hi", "cjk": "CJK"}
OPERATIONS = ["grapheme", "word", "sentence", "line", "nfc", "nfd", "nfkc", "lower", "upper",
              "fold", "bidi", "scripts"]
figures = collections.defaultdict(dict)
counts = collections.defaultdict(dict)
order = []
for line in sys.stdin:
    corpus, operation, program, per_byte, count = line.split()
    if operation.endswith("-find"):
        operation, program = operation[: -len("-find")], "find"
    key = (corpus, operation)
    if key not in order:
        order.append(key)
    if per_byte != "-":
        figures[key][program] = int(per_byte) / 100
        counts[key][program] = count

print("| Text | Operation | maul-unicode | `muniFind*Breaks` | ICU4C | ICU4X | maul-unicode / faster | Counts |")
print("|---|---|---:|---:|---:|---:|---:|---|")
for corpus in TEXTS:
    for key in sorted((k for k in order if k[0] == corpus), key=lambda k: OPERATIONS.index(k[1])):
        row = figures[key]
        cell = lambda program: f"{row[program]:.1f}" if program in row else "-"
        ours = [row[p] for p in ("ours", "find") if p in row]
        references = [row[p] for p in ("icu4c", "icu4x") if p in row]
        ratio = f"{min(ours) / min(references):.2f}" if ours and references else "-"
        agreed = len(set(counts[key].values())) <= 1
        note = "" if agreed else " ".join(f"{p} {c}" for p, c in counts[key].items() if p != "find")
        print(f"| {TEXTS[corpus]} | {key[1]} | {cell('ours')} | {cell('find')} | {cell('icu4c')} | "
              f"{cell('icu4x')} | {ratio} | {note} |")
