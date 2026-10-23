# Checks

What verifies a change, locally and in CI, and the time each may take.

## Locally

`tools/check.sh` runs the checks in three tiers; each includes the one before it.

| Tier | Runs | Budget |
|---|---|---|
| `changed` | The source, module, length, API documentation and snippet rules, the organization's documentation check, `clang-format` on the changed C files, a Debug build with assertions on, and the CTest labels the changed files reach. | under 2 minutes |
| `pr` | Every rule on every file, the generated tables against the generator and their ceilings, a Release build with GCC and the HarfBuzz functions, clang-tidy, the suite under AddressSanitizer and UBSan, and 10 seconds of fuzzing per target. | under 10 minutes |
| `full` | Also ThreadSanitizer, a build with every optional component left out, the shared library with HarfBuzz installed and used by `samples/minimal`, and 60 seconds per fuzz target with the ICU comparison. | under 20 minutes |

Run `changed` before every push. On the shared development machine, run `pr` and `full` through the workspace's heavy-job wrapper. Build trees go to `build-check/`, one per configuration, with each build's log beside it.

## In CI

`.github/workflows/ci.yml` runs on every pull request, on every push to `main` and every night. The ruleset requires only its `required` job, which passes when every other job passed.

| Job | What it guards |
|---|---|
| `docs` | The organization's documentation check: allowed files, the em dash, record numbers, page sizes, links. `tools/ucd/` is excluded. |
| `checks` | `clang-format`, the family's source rules, the module graph, lengths, API documentation, documentation snippets, that the documentation site builds, that `munigen` regenerates `src/generated/` byte for byte, the table ceilings, and clang-tidy. |
| `build-test` | Eight cells, all with warnings as errors: GCC 14 and Clang 20 in Release, Clang with AddressSanitizer and UBSan, Clang with ThreadSanitizer, a shared library, the HarfBuzz functions, every optional component left out, and macOS. The shared and HarfBuzz cells install the library and build `samples/minimal` against it. A skipped test fails its cell. |
| `fuzz` | Every fuzz target from its seeds in `test/seeds/`: 20 seconds each on a pull request, 90 seconds on `main` and at night. |
| `windows-clang-cl` | The library and its tests with `clang-cl`, and every public header compiled by MSVC's `cl` as C17 and C++17 with `/W4 /WX`. |
| `web-emscripten` | The library and its tests under Emscripten and Node, and the size of the static library in total and per optional component. |

`pr-title.yml` checks that the pull request title is a conventional commit with a scope from `AGENTS.md`. `codeql.yml` runs CodeQL on every pull request and on `main`, building with GCC 14 (an advanced setup, since the default one builds with the runner's GCC 13). `scorecard.yml` grades the repository's security practices weekly for the README badge, and Dependabot proposes updates of the pinned actions once a week in one pull request.

## Tests

Each suite in `test/` is one executable, registered with CTest under its name and labeled with it. The data suites read the Unicode files in `tools/ucd/`. The white-box suites (properties, identifier, case, security) include `src/tables.h` for the generator's hashes and are skipped in shared builds. `test_guide.c` and `test_guide_harfbuzz.c` run the C snippets of `docs/user/` as written; `tools/check_guide.py` fails when a snippet and its copy differ.

## Fuzzing

Every fuzz target (`test/fuzz_<area>.c`) checks properties that must hold for any input rather than known answers: no crash, no broken assertion, piecewise iteration giving the whole-text answer, round trips, idempotence where the standard promises it, agreement between parts (validation and conversion, normalization and its quick check, NFKC_Casefold and the normal forms), and output through a short buffer matching output through a large one. `fuzz_icu` compares with ICU, a test-only oracle, where both Unicode versions agree; its header lists what it leaves out and why.

A crash a fuzzer finds becomes a test in the suite of its module (see [development.md](development.md#reproducing-a-fuzz-finding)).

## Budgets

Over a budget, tests are pruned or merged before anything else is added. The pull request check takes about six minutes, most of it the fuzz job; the run on `main` and at night about seventeen.
