#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Counts the instructions the library, ICU4C and ICU4X execute for the
# same work on the same texts, with valgrind's cachegrind, which a busy
# machine does not change. Each operation runs over each corpus once and
# six times; the difference over five is one pass, so loading the text
# and ICU's data, and making ICU's objects, drop out. Prints
# instructions per byte of UTF-8 text and the count each pass gave,
# which the three agree on except where the table says.
#
#   tools/oracle/bench/measure.sh BUILD ICU ICU_SOURCE [OPERATION...]
#
# BUILD is a Release build of the library (libmaul-unicode.a); ICU is
# ICU4C 79.1 installed (--prefix) and ICU_SOURCE its icu4c/source tree.
# Needs gcc-14, g++-14, cargo, valgrind and python3. Builds into
# out/oracle-bench. Run it through the workspace's heavy-job wrapper.
set -euo pipefail

if [ $# -lt 3 ]; then
    sed -n '13,18p' "$0" >&2
    exit 2
fi
root=$(cd "$(dirname "$0")/../../.." && pwd)
build=$(cd "$1" && pwd)
icu=$(cd "$2" && pwd)
icu_source=$(cd "$3" && pwd)
shift 3
here=$root/tools/oracle/bench
out=$root/out/oracle-bench
mkdir -p "$out"

# The corpora, each repeated whole up to 64 KiB.
python3 -I - "$here/corpus" "$out" <<'EOF'
import os, sys
source, out = sys.argv[1], sys.argv[2]
for name in ("latin", "world", "cjk"):
    with open(os.path.join(source, name + ".txt"), "rb") as f:
        data = f.read()
    text = data * (65536 // len(data))
    with open(os.path.join(out, name + ".txt"), "wb") as f:
        f.write(text)
EOF

gcc-14 -O2 -std=c23 -I"$root/include" "$here/ours.c" "$build/libmaul-unicode.a" -o "$out/ours"
scrptrun=$icu_source/extra/scrptrun
g++-14 -O2 -std=c++17 -I"$icu/include" -I"$scrptrun" -I"$icu_source/common" "$here/icu4c.cpp" \
    "$scrptrun/scrptrun.cpp" -L"$icu/lib" -licuuc -licudata -o "$out/icu4c"
CARGO_TARGET_DIR=$out/cargo cargo build --release --quiet --manifest-path "$here/icu4x/Cargo.toml"
cp "$out/cargo/release/icu4x_bench" "$out/icu4x"

operations=("$@")
if [ ${#operations[@]} -eq 0 ]; then
    operations=(grapheme word sentence line nfc nfd nfkc lower upper fold bidi scripts)
fi

# One line: corpus, operation, program, instructions per byte, count.
count() {
    local corpus=$1 operation=$2 program=$3 out=$4
    local text=$out/$corpus.txt
    local bytes once six result
    bytes=$(stat -c %s "$text")
    result=$("$out/$program" "$operation" "$text" 1 2>/dev/null) || { echo "$corpus $operation $program - -"; return; }
    refs() {
        valgrind --tool=cachegrind --cache-sim=no --cachegrind-out-file=/dev/null \
            "$out/$program" "$operation" "$text" "$1" 2>&1 >/dev/null | awk '/I *refs/ { gsub(",", "", $NF); print $NF }'
    }
    once=$(refs 1)
    six=$(refs 6)
    echo "$corpus $operation $program $(( (six - once) * 100 / 5 / bytes )) $result"
}
export -f count
export LD_LIBRARY_PATH=$icu/lib

for corpus in latin world cjk; do
    for operation in "${operations[@]}"; do
        for program in ours icu4c icu4x; do
            echo "$corpus $operation $program $out"
        done
        case $operation in
            grapheme | word | sentence | line) echo "$corpus $operation-find ours $out" ;;
        esac
    done
done | xargs -P 8 -L 1 bash -c 'count "$@"' _ | python3 -I "$here/table.py"
