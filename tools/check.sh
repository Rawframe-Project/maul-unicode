#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# The checks, in three tiers. Each tier includes the one before it.
#
#   tools/check.sh changed   the files changed against origin/main: the
#                            source rules, formatting, the documentation
#                            checks, the linux-clang-20-asan preset
#                            (assertions on), and the test labels the changes
#                            reach. Under two minutes; run it before every
#                            push.
#   tools/check.sh pr        what the pull request check runs on this
#                            machine: every check, the generated tables, the
#                            table budget, clang-tidy, the linux-harfbuzz and
#                            linux-clang-20-asan presets, and a short fuzzing
#                            pass.
#   tools/check.sh full      also every other Linux preset (ThreadSanitizer,
#                            shared with HarfBuzz and the package test,
#                            minimal, Clang Release, GCC Release) and
#                            longer fuzzing.
#
# The builds are the CI cells' CMake presets (CMakePresets.json), in
# out/<preset>; the other trees go to out/check-*. Needs gcc-14, clang-20,
# clang-tidy-20, ninja and the clang-format version CI pins, HarfBuzz and
# ICU: the organization's CI image has them all. The org documentation
# check runs when the organization's .github checkout is at ORG_DEFAULTS
# (default: ../../.github from the repository).
set -euo pipefail

tier=${1:-changed}
case "$tier" in
changed | pr | full) ;;
*)
    echo "usage: tools/check.sh [changed|pr|full]" >&2
    exit 2
    ;;
esac

cd "$(dirname "$0")/.."
root=$PWD
out=$root/out
mkdir -p "$out"
org=${ORG_DEFAULTS:-$root/../../.github}
jobs=${JOBS:-8}

step() { printf '\n== %s\n' "$*"; }

# A tree outside the presets (the generator, clang-tidy, fuzzing).
configure() { # name, then cmake arguments
    local name=check-$1
    shift
    cmake -S "$root" -B "$out/$name" -G Ninja -DMAUL_UNICODE_WERROR=ON "$@" >"$out/$name.log"
}

build() { cmake --build "$out/check-$1" -j "$jobs" >>"$out/check-$1.log"; }

# A CI cell: its configure, build and test presets. More ctest arguments
# (a label filter) follow the preset's name.
preset() {
    local name=$1
    shift
    step "preset $name"
    cmake --preset "$name" >"$out/$name.log"
    cmake --build --preset "$name" -j "$jobs" >>"$out/$name.log"
    ctest --preset "$name" -j "$jobs" "$@"
}

# The ctest labels a changed path reaches; "all" when it reaches every
# suite (shared code, the build, the tables).
labels_for() {
    case "$1" in
    *.md | LICENSE | .github/* | tools/check_*.py | tools/build_docs.py | tools/gen_api.py) ;;
    docs/user/*) echo guide ;;
    src/base.c | include/maul-unicode/base.h | test/test_base.c) echo base ;;
    src/properties.c | include/maul-unicode/properties.h | test/test_properties.c) echo properties ;;
    src/identifier.c | include/maul-unicode/identifier.h | test/test_identifier.c) echo identifier ;;
    src/grapheme.c | src/word.c | src/sentence.c | src/line.c | src/complex.* | src/segment* | \
        include/maul-unicode/segment.h | test/test_segment.c) echo "segment guide" ;;
    src/script_runs.c | include/maul-unicode/script.h | test/test_script.c) echo "script guide" ;;
    src/bidi* | include/maul-unicode/bidi.h | test/test_bidi.c) echo "bidi security guide" ;;
    src/normalize.c | src/normalizer.h | src/decompose.* | include/maul-unicode/normalize.h | \
        test/test_normalize.c) echo "normalize case security harfbuzz guide" ;;
    src/case* | src/nfkc_casefold.c | include/maul-unicode/case.h | test/test_case.c) echo "case guide" ;;
    src/security.c | src/skeleton.c | include/maul-unicode/security.h | test/test_security.c) echo security ;;
    src/harfbuzz.c | include/maul-unicode/harfbuzz.h | test/test_harfbuzz.c | test/test_guide_harfbuzz.c)
        echo "harfbuzz guide_harfbuzz" ;;
    test/test_guide.c) echo guide ;;
    test/fuzz_* | test/seeds/* | bench/* | samples/*) ;;
    *) echo all ;;
    esac
}

source_checks() {
    step "source, module, length, documentation and snippet rules"
    python3 tools/check_source.py
    python3 tools/check_modules.py
    python3 tools/check_lengths.py
    python3 tools/check_docs.py
    python3 tools/check_guide.py
    if [ -f "$org/scripts/docs_check.py" ]; then
        python3 "$org/scripts/docs_check.py" --exclude tools/ucd
    else
        echo "skipped the organization's documentation check: no checkout at $org"
    fi
}

all_c_files() { git ls-files 'include/*.h' 'src/*.[ch]' 'test/*.[ch]' 'tools/*.[ch]' 'bench/*.[ch]' 'samples/*.[ch]'; }

if [ "$tier" = changed ]; then
    base=$(git merge-base HEAD origin/main 2>/dev/null || echo HEAD)
    mapfile -t changed < <({
        git diff --name-only "$base"
        git ls-files --others --exclude-standard
    } | sort -u)
    if [ ${#changed[@]} -eq 0 ]; then
        echo "nothing changed against origin/main"
        exit 0
    fi
    source_checks
    mapfile -t c_files < <(printf '%s\n' "${changed[@]}" | grep -E '\.[ch]$' | grep -v '^src/generated/' || true)
    present=()
    for f in "${c_files[@]+"${c_files[@]}"}"; do [ -f "$f" ] && present+=("$f"); done
    if [ ${#present[@]} -gt 0 ]; then
        step "clang-format on ${#present[@]} changed files"
        clang-format --dry-run -Werror "${present[@]}"
    fi
    labels=""
    for f in "${changed[@]}"; do labels="$labels $(labels_for "$f")"; done
    if [ -z "${labels// /}" ]; then
        echo "no test reached by the changes"
        exit 0
    fi
    if [[ " $labels " == *" all "* ]]; then
        preset linux-clang-20-asan
    else
        regex=$(printf '%s\n' $labels | sort -u | paste -sd '|')
        echo "tests labeled $regex"
        preset linux-clang-20-asan -L "^($regex)\$"
    fi
    exit 0
fi

source_checks

step "clang-format on every file"
clang-format --dry-run -Werror $(all_c_files)

step "generated tables match the generator, and stay within their ceilings"
configure generator -DCMAKE_C_COMPILER=gcc-14 -DMAUL_UNICODE_BUILD_TESTS=OFF
cmake --build "$out/check-generator" --target munigen >>"$out/check-generator.log"
rm -rf "$out/regenerated"
mkdir -p "$out/regenerated"
"$out/check-generator/munigen" tools/ucd "$out/regenerated" >"$out/table-sizes.txt"
diff -r src/generated "$out/regenerated"
python3 tools/table_budget.py "$out/table-sizes.txt"

preset linux-harfbuzz

step "clang-tidy"
configure tidy -DCMAKE_C_COMPILER=clang-20 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DMAUL_UNICODE_BUILD_TESTS=OFF \
    -DMAUL_UNICODE_HARFBUZZ=ON
for f in src/*.c; do
    clang-tidy-20 -p "$out/check-tidy" --quiet --warnings-as-errors='*' "$f" 2>/dev/null
done

preset linux-clang-20-asan

fuzz_seconds=10
[ "$tier" = full ] && fuzz_seconds=60
step "fuzzing, $fuzz_seconds seconds per target"
fuzz_icu=OFF
[ "$tier" = full ] && fuzz_icu=ON
configure fuzz -DCMAKE_C_COMPILER=clang-20 -DMAUL_UNICODE_FUZZ=ON -DMAUL_UNICODE_FUZZ_ICU=$fuzz_icu \
    -DMAUL_UNICODE_BUILD_TESTS=OFF
build fuzz
for target in "$out"/check-fuzz/fuzz_*; do
    name=$(basename "$target")
    corpus=$out/corpus/$name
    mkdir -p "$corpus"
    [ -d "test/seeds/$name" ] && cp test/seeds/"$name"/* "$corpus"/
    "$target" -max_total_time=$fuzz_seconds -max_len=256 "$corpus" >"$out/$name.log" 2>&1 ||
        {
            tail -30 "$out/$name.log"
            exit 1
        }
    echo "$name: no finding"
done

[ "$tier" = full ] || exit 0

for name in linux-clang-20-tsan linux-shared linux-minimal linux-clang-20 linux-gcc-14; do
    preset "$name"
done

step "all checks passed"
