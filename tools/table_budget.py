#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Checks the generated tables' sizes against their ceilings
# (muni-0008): reads the generator's report, the lines
# "Name  12345 bytes ...", and tools/table-budget.txt, and fails when a
# component's tables together pass its ceiling or when a table in the
# report belongs to no component.
#
# usage: munigen tools/ucd OUT | python3 tools/table_budget.py
#        python3 tools/table_budget.py REPORT

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUDGET = os.path.join(ROOT, "tools", "table-budget.txt")
TABLE = re.compile(r"^(\w+)\s+(\d+) bytes\b")


def budget():
    """(component, ceiling, tables) for each line of the listing."""
    result = []
    for number, line in enumerate(open(BUDGET, encoding="utf-8"), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        match = re.match(r"^([^:]+):\s*(\d+)\s+(.+)$", line)
        if not match:
            sys.exit("table-budget.txt:%d: expected 'component: ceiling tables...'" % number)
        result.append((match.group(1), int(match.group(2)), match.group(3).split()))
    return result


def main():
    report = open(sys.argv[1], encoding="utf-8") if len(sys.argv) > 1 else sys.stdin
    sizes = {}
    for line in report:
        match = TABLE.match(line)
        if match:
            sizes[match.group(1)] = int(match.group(2))
    if not sizes:
        sys.exit("table budget: the report names no table")
    failures = 0
    covered = set()
    for component, ceiling, tables in budget():
        missing = [t for t in tables if t not in sizes]
        if missing:
            print("%s: no table named %s in the report" % (component, ", ".join(missing)))
            failures += 1
            continue
        covered.update(tables)
        total = sum(sizes[t] for t in tables)
        verdict = "over" if total > ceiling else "within"
        print("%-30s %6d of %6d bytes, %s" % (component, total, ceiling, verdict))
        failures += total > ceiling
    for table in sorted(set(sizes) - covered):
        print("%s: %d bytes in no component of tools/table-budget.txt" % (table, sizes[table]))
        failures += 1
    if failures:
        sys.exit("table budget: %d failure(s)" % failures)
    print("table budget: %d tables in %d components, all within their ceilings" %
          (len(sizes), len(budget())))


if __name__ == "__main__":
    main()
