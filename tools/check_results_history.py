#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks that the history of results versions in test/test_results.c only
# grows: a diff against the base may add a `{version, 0x...ull},` line but
# never remove or edit one, so the hash a version was released with stays
# what the test compares against. A change of results takes a new
# version instead.
#
# usage: python3 tools/check_results_history.py BASE
#        python3 tools/check_results_history.py --self-test

import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HISTORY = "test/test_results.c"
FIXTURES = os.path.join(ROOT, "tools", "results_history")
ENTRY = re.compile(r"^-\s*\{\s*\d+\s*,\s*0x[0-9A-Fa-f]+ull\s*\},")


def removed_entries(diff):
    """The history lines a unified diff removes."""
    return [line for line in diff.splitlines() if ENTRY.match(line)]


def self_test():
    """Each fixture named *-ok.diff must pass and each *-bad.diff fail."""
    wrong = []
    names = sorted(n for n in os.listdir(FIXTURES) if n.endswith(".diff"))
    for name in names:
        with open(os.path.join(FIXTURES, name), encoding="utf-8") as f:
            removed = removed_entries(f.read())
        if name.endswith("-ok.diff") and removed:
            wrong.append(f"{name}: refused, but it only adds a version")
        elif name.endswith("-bad.diff") and not removed:
            wrong.append(f"{name}: passed, but it changes a released version")
    if not any(n.endswith("-ok.diff") for n in names) or not any(n.endswith("-bad.diff") for n in names):
        wrong.append("tools/results_history/ needs at least one -ok and one -bad fixture")
    return wrong


def main():
    if sys.argv[1:] == ["--self-test"]:
        wrong = self_test()
    elif len(sys.argv) == 2:
        diff = subprocess.run(["git", "diff", sys.argv[1], "--", HISTORY], cwd=ROOT, check=True,
                              capture_output=True, text=True).stdout
        wrong = [f"{HISTORY}: removes or edits a released version: {line[1:].strip()}"
                 for line in removed_entries(diff)]
    else:
        print("usage: check_results_history.py BASE | --self-test", file=sys.stderr)
        return 2
    for line in wrong:
        print(line, file=sys.stderr)
    if wrong:
        print("results history: add a line with the next version instead", file=sys.stderr)
        return 1
    print("results history: only grows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
