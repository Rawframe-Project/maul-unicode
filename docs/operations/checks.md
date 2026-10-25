# Checks

What verifies a change, locally and in CI, and the time each may take.

## Locally

`tools/check.sh` runs the checks in three tiers; each includes the one before it. Its builds are the CI cells' presets from `CMakePresets.json`.

| Tier | Runs | Budget |
|---|---|---|
| `changed` | The source, module, length, API documentation and snippet rules, the Unicode files against their SHA-256, the organization's documentation check, `clang-format` on the changed C files, and the `linux-clang-20-asan` preset (assertions on) with the CTest labels the changed files reach. | under 2 minutes |
| `pr` | Every rule on every file, the generated tables against the generator and their ceilings, clang-tidy, the `linux-harfbuzz` and `linux-clang-20-asan` presets, and 10 seconds of fuzzing per target. | under 10 minutes |
| `full` | Also every other Linux preset (ThreadSanitizer, the shared library with the HarfBuzz functions, the package test and the exports test, every optional component left out, Clang and GCC Release) and 60 seconds per fuzz target with the ICU comparison. | under 20 minutes |

Run `changed` before every push. On the shared development machine, run `pr` and `full` through the workspace's heavy-job wrapper. Builds go to `out/<preset>` and the other trees to `out/check-*`, each with its log beside it.

## In CI

`.github/workflows/ci.yml` runs on every pull request, on every push to `main` and every night. The Linux jobs run in the organization's CI image, pinned by digest, and install nothing. The ruleset requires only its `required` job, which passes when every other job passed or was skipped, and the title check.

| Job | What it guards |
|---|---|
| `changes` | Whether a pull request changes anything beyond documentation; when it does not, the build jobs below are skipped. |
| `docs` | The organization's documentation check: allowed files, the em dash, record numbers, page sizes, links. `tools/ucd/` is excluded. |
| `zizmor` | The workflows: injection, persisted credentials, permissions, unpinned actions. |
| `checks` | `clang-format`, the family's source rules, the module graph, lengths, API documentation, documentation snippets, that every file in `tools/ucd/` matches its SHA-256 in `tools/ucd/README.md` (`tools/check_ucd.py`), that the documentation site builds, that `munigen` regenerates `src/generated/` byte for byte, the table ceilings, and clang-tidy. It runs on documentation changes too. |
| `build` | One call of the organization's `cpp.yml` per preset: GCC 14 and Clang 20 in Release, Clang with AddressSanitizer and UBSan (each test repeated up to three times on a pull request, so a flaky one fails before it merges), Clang with ThreadSanitizer, the shared library (with the HarfBuzz functions, so they link only against what it exports), the static library with the HarfBuzz functions, every optional component left out, macOS and `clang-cl` on Windows. All build with warnings as errors; the shared and HarfBuzz cells run the package test, and the shared cell checks that the library exports the public functions and nothing else. ccache is restored on every run and saved only from `main`. |
| `msvc-headers` | Every public header compiled by MSVC's `cl` as C17 and C++17 with `/W4 /WX`. |
| `fuzz` | Every fuzz target from its seeds in `test/seeds/` and its cached corpus, four at a time: 20 seconds each on a pull request, 90 seconds on `main` and 600 at night. Dispatching `ci` by hand runs the night's fuzzing, 600 seconds per target unless its `fuzz-seconds` input says otherwise. On `main` and at night each corpus is then minimized and saved. A finding uploads the logs and the crashing input. |
| `web` | The library and its tests under Emscripten and Node, and the size of the static library in total and per optional component. The image has no Emscripten, so this job uses the runner and the emsdk action. |

`pr-title.yml` checks that the pull request title is a conventional commit with a scope from `AGENTS.md`, and `pr-gate.yml` closes a pull request from outside the organization unless it closes an issue labeled `accepted`. `codeql.yml` runs CodeQL on every pull request and on `main`, building with GCC 14 (an advanced setup, since the default one builds with the runner's GCC 13). `scorecard.yml` grades the repository's security practices weekly for the README badge, and Dependabot proposes updates of the pinned actions once a week in one pull request.

## Tests

Each suite in `test/` is one executable, registered with CTest under its name and labeled with it. The data suites read the Unicode files in `tools/ucd/`. The white-box suites (properties, identifier, case, security) include `src/tables.h` for the generator's hashes and are not built in shared builds. No test reports itself skipped; a test preset that finds no test fails, and so does a CI cell where a test is skipped or not run. `test_guide.c` and `test_guide_harfbuzz.c` run the C snippets of `docs/user/` as written; `tools/check_guide.py` fails when a snippet and its copy differ.

## Fuzzing

Every fuzz target (`test/fuzz_<area>.c`) checks properties that must hold for any input rather than known answers: no crash, no broken assertion, piecewise iteration giving the whole-text answer, round trips, idempotence where the standard promises it, agreement between parts (validation and conversion, normalization and its quick check, NFKC_Casefold and the normal forms), and output through a short buffer matching output through a large one. `fuzz_icu` compares with ICU, a test-only oracle, where both Unicode versions agree; its header lists what it leaves out and why.

Each target's corpus carries over from run to run in one Actions cache entry, `fuzz-corpus-<run id>`, holding a directory per target. Every run restores the latest entry and adds the committed seeds, which are the floor. Runs on `main` and at night then minimize each target's corpus with libFuzzer's `-merge=1` into a fresh directory, swap it in only when the merge finished, and save a new entry. Pull requests restore but never save. A target whose minimized corpus passes 25 MB fails the step, and the corpora are not saved, so one target cannot crowd the others out of the cache. The organization's other libraries run one fuzz job per target, each with its own cache. This repository has nine targets in one job, four at a time, because nine more containers per run would cost more runner time than the targets' corpora need, and the corpora are still kept and minimized separately.

A crash a fuzzer finds becomes a test in the suite of its module (see [development.md](development.md#reproducing-a-fuzz-finding)).

## Budgets

Over a budget, tests are pruned or merged before anything else is added. The pull request check takes about two minutes and the run on `main` about five, most of it the fuzz job. The night run fuzzes each target for 600 seconds, about 30 minutes in all: it is the standard's extended nightly run, which may take up to two hours and is not a required check; a build cell takes under a minute and a half, with or without ccache.
