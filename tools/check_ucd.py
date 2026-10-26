#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks that tools/ucd/ holds the Unicode files exactly as published:
# every file's SHA-256 matches the list in tools/ucd/README.md, the list
# names every file there, and every file it names is there. The tables
# CI regenerates come from these files, so an edited one would pass that
# check with tables made from it.
#
# usage: python3 tools/check_ucd.py

import hashlib
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UCD = os.path.join(ROOT, "tools", "ucd")
README = os.path.join(UCD, "README.md")
ENTRY = re.compile(r"^([0-9a-f]{64})  (\S+)$")


def main():
    listed = {}
    with open(README, encoding="utf-8") as f:
        for line in f:
            found = ENTRY.match(line.rstrip("\n"))
            if found:
                listed[found.group(2)] = found.group(1)
    present = sorted(n for n in os.listdir(UCD) if n != "README.md")
    wrong = []
    for name in present:
        if name not in listed:
            wrong.append(f"tools/ucd/{name}: not in the list of tools/ucd/README.md")
            continue
        with open(os.path.join(UCD, name), "rb") as f:
            digest = hashlib.sha256(f.read()).hexdigest()
        if digest != listed[name]:
            wrong.append(f"tools/ucd/{name}: SHA-256 {digest}, the list says {listed[name]}")
    for name in sorted(set(listed) - set(present)):
        wrong.append(f"tools/ucd/{name}: listed in tools/ucd/README.md but missing")
    for line in wrong:
        print(line)
    if wrong or not present:
        return 1
    print(f"tools/ucd: the {len(present)} files match their SHA-256")
    return 0


if __name__ == "__main__":
    sys.exit(main())
