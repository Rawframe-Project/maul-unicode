#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Test hygiene, as the repository standard's Enforcement asks: a test is
# deterministic, hermetic and fast, and never skips unexplained. In the
# test sources (test/, and test/ beside each part tools/source-dirs.txt
# lists) it rejects:
#
# - sleep: sleep, usleep, nanosleep, Sleep and thrd_sleep, which make a
#   test slow and its result depend on timing;
# - wall-clock: time(), gettimeofday, timespec_get and clock_gettime with
#   CLOCK_REALTIME, whose answers differ from run to run;
# - retry: an identifier or label naming a retry or an attempt, the shape
#   of a loop that runs an assertion until it passes;
# - skip: exit(77), return 77, and macros named like SKIP (GTEST_SKIP), and
#   in the CMake files the SKIP_RETURN_CODE, SKIP_REGULAR_EXPRESSION and
#   DISABLED test properties.
#
# A use that must stay carries "// hygiene: <reason>" on its line or the
# line before (in CMake, "# hygiene: <reason>"). Comments and string
# literals are not searched.
#
# With --ctest it also reads each configured build's tests from
# "ctest --show-only=json-v1" and rejects a test without a label, and a
# build that registers none. Each finding names the file and line, or the
# build and test, with its rule; any finding fails the check.
#
#   python3 tools/check_test_hygiene.py [--ctest BUILD_DIR ...]
#   python3 tools/check_test_hygiene.py --self-test
#
# --self-test runs the rules over tools/test_hygiene/, whose files mark
# each line that must be found with "expect: <rule>", and fails unless
# exactly those are found.

import argparse
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIXTURES = os.path.join(ROOT, "tools", "test_hygiene")
C_SUFFIXES = (".c", ".h", ".cc", ".cpp", ".hpp", ".m", ".mm")
SKIP_DIRS = {".git", "out", "build", "_site", "node_modules"}

C_RULES = [
    ("sleep", re.compile(r"(?<![\w.>])(sleep|usleep|nanosleep|Sleep|thrd_sleep)\s*\("),
     "a sleep makes the test slow and its result depend on timing"),
    ("wall-clock",
     re.compile(r"(?<![\w.>])(time|gettimeofday|timespec_get)\s*\(|"
                r"\bclock_gettime\s*\(\s*CLOCK_REALTIME\b"),
     "the wall clock differs from run to run"),
    ("retry", re.compile(r"\b\w*(retry|retries|attempt)\w*\b", re.IGNORECASE),
     "a test that passes on a later try hides a flaky one"),
    ("skip", re.compile(r"\bexit\s*\(\s*77\s*\)|\breturn\s+77\b|\b\w*SKIP\w*\s*\("),
     "a skipped test checks nothing"),
]
CMAKE_RULES = [
    ("skip", re.compile(r"\b(SKIP_RETURN_CODE|SKIP_REGULAR_EXPRESSION|DISABLED)\b"),
     "a skipped or disabled test checks nothing"),
]
C_MARKER = re.compile(r"//.*\bhygiene:\s*\S")
CMAKE_MARKER = re.compile(r"#.*\bhygiene:\s*\S")
EXPECT = re.compile(r"\bexpect:\s*([\w-]+)")


def code_lines(text):
    """The C text line by line with comments and string and character
    literals blanked, so that only code is searched."""
    out = []
    line = []
    i = 0
    state = None  # None, "line", "block", '"' or "'"
    while i < len(text):
        c = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if c == "\n":
            out.append("".join(line))
            line = []
            if state in ("line", '"', "'"):
                state = None
        elif state is None:
            if c == "/" and nxt == "/":
                state = "line"
                i += 1
            elif c == "/" and nxt == "*":
                state = "block"
                i += 1
            elif c in "\"'":
                state = c
                line.append(" ")
            else:
                line.append(c)
        elif state == "block":
            if c == "*" and nxt == "/":
                state = None
                i += 1
        elif state in "\"'":
            if c == "\\":
                i += 1
            elif c == state:
                state = None
        i += 1
    out.append("".join(line))
    return out


def cmake_code(line):
    """A CMake line without its comment (strings are not blanked; test
    properties are never in them)."""
    return line.split("#", 1)[0]


def scan_file(path, rel, findings):
    text = open(path, encoding="utf-8", errors="replace").read()
    raw = text.split("\n")
    if path.endswith(C_SUFFIXES):
        rules, marker, codes = C_RULES, C_MARKER, code_lines(text)
    else:
        rules, marker, codes = CMAKE_RULES, CMAKE_MARKER, [cmake_code(x) for x in raw]
    for index, code in enumerate(codes):
        for rule, pattern, reason in rules:
            if not pattern.search(code):
                continue
            if marker.search(raw[index]) or (index > 0 and marker.search(raw[index - 1])):
                continue
            findings.append((rel, index + 1, rule,
                             f"{rel}:{index + 1}: {rule}: {reason}; mark it"
                             " '// hygiene: <reason>' if it must stay"))


def walk(top, accept):
    for folder, dirs, files in os.walk(top):
        dirs[:] = sorted(d for d in dirs if d not in SKIP_DIRS and not d.startswith("."))
        for name in sorted(files):
            if accept(name):
                yield os.path.join(folder, name)


def source_dirs():
    path = os.path.join(ROOT, "tools", "source-dirs.txt")
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        return [line.split("#", 1)[0].strip() for line in f if line.split("#", 1)[0].strip()]


def external_dirs():
    path = os.path.join(ROOT, "tools", "external-dirs.txt")
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        return [line.split("#", 1)[0].strip() for line in f if line.split("#", 1)[0].strip()]


def is_cmake(name):
    return name == "CMakeLists.txt" or name.endswith(".cmake")


def scan_tree(findings):
    for top in ["test"] + [os.path.join(d, "test") for d in source_dirs()]:
        folder = os.path.join(ROOT, top)
        if os.path.isdir(folder):
            for path in walk(folder, lambda n: n.endswith(C_SUFFIXES)):
                scan_file(path, os.path.relpath(path, ROOT), findings)
    skipped = [os.path.join(ROOT, d) for d in external_dirs()] + [FIXTURES]
    for path in walk(ROOT, is_cmake):
        if not any(path.startswith(d + os.sep) for d in skipped):
            scan_file(path, os.path.relpath(path, ROOT), findings)


def check_labels(name, data, findings):
    tests = data.get("tests", [])
    if not tests:
        findings.append((name, 0, "unlabeled-test", f"{name}: no test is registered"))
    for test in tests:
        labels = [p.get("value") for p in test.get("properties", []) if p.get("name") == "LABELS"]
        if not any(labels):
            findings.append((name, 0, "unlabeled-test",
                             f"{name}: test {test.get('name')} has no label; give it its suite's"))


def ctest_json(build):
    out = subprocess.run(["ctest", "--show-only=json-v1", "--test-dir", build], check=True,
                         capture_output=True, text=True).stdout
    return json.loads(out)


def self_test():
    findings = []
    expected = set()
    for path in walk(FIXTURES, lambda n: n.endswith(C_SUFFIXES) or is_cmake(n)):
        rel = os.path.relpath(path, ROOT)
        scan_file(path, rel, findings)
        for number, line in enumerate(open(path, encoding="utf-8").read().split("\n"), 1):
            for rule in EXPECT.findall(line):
                expected.add((rel, number, rule))
    labels = os.path.join(FIXTURES, "ctest.json")
    with open(labels, encoding="utf-8") as f:
        check_labels("tools/test_hygiene/ctest.json", json.load(f), findings)
    check_labels("an empty build", {"tests": []}, findings)
    expected |= {("tools/test_hygiene/ctest.json", 0, "unlabeled-test"),
                 ("an empty build", 0, "unlabeled-test")}
    found = {(rel, line, rule) for rel, line, rule, _ in findings}
    wrong = False
    for item in sorted(expected - found):
        print(f"missed: {item[0]}:{item[1]}: {item[2]}")
        wrong = True
    for item in sorted(found - expected):
        print(f"unexpected: {item[0]}:{item[1]}: {item[2]}")
        wrong = True
    if wrong or not expected:
        return 1
    print(f"test hygiene self-test: the {len(expected)} expected findings, nothing else")
    return 0


def main():
    parser = argparse.ArgumentParser(description="Checks test hygiene and test labels.")
    parser.add_argument("--ctest", action="append", default=[], metavar="BUILD_DIR",
                        help="a configured build whose tests must all carry a label")
    parser.add_argument("--self-test", action="store_true",
                        help="run the rules over tools/test_hygiene/")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    findings = []
    scan_tree(findings)
    for build in args.ctest:
        check_labels(build, ctest_json(build), findings)
    for finding in findings:
        print(finding[3])
    if findings:
        return 1
    builds = f", every test labeled in {len(args.ctest)} build(s)" if args.ctest else ""
    print(f"test hygiene: no sleep, wall clock, retry or skip{builds}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
