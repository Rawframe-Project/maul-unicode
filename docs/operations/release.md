# Release

Releases are made by release-please from the conventional commit titles on `main`. Versions follow Semantic Versioning; before 1.0.0 a breaking change bumps the minor version, a feature the minor and a fix the patch. Tags are `vX.Y.Z`.

## Steps

1. release-please keeps a draft pull request titled `chore: release X.Y.Z` open. It moves the version between the markers in `include/maul-unicode/base.h`, where CMake reads it, updates `.release-please-manifest.json`, and adds an entry to `CHANGELOG.md` from the `feat`, `fix` and `perf` titles.
2. Rewrite that entry in user language before merging: what changed for someone using the library, grouped as Added, Changed, Fixed, Removed and Security. Leave out refactors, tests, CI and documentation. Say what a caller must change for a breaking change, and name any change of results (a new Unicode version, a fixed rule) since stored data may depend on them.
3. Check that the `required` CI job passed on the release pull request, including the table ceilings and the web size report.
4. Mark the pull request ready and merge it. release-please tags the merge commit and publishes the GitHub release with the changelog entry as its notes. The Pages workflow publishes the documentation from `main`.
5. The `assets` job of `.github/workflows/release.yml` then attaches the source archive `maul-unicode-X.Y.Z.tar.gz` (the tagged tree, generated tables and Unicode data included), its `SHA256SUMS`, an SPDX SBOM named after the repository and tag, and a build provenance attestation. Check that they are on the release; a user verifies a download with `sha256sum -c SHA256SUMS` and `gh attestation verify maul-unicode-X.Y.Z.tar.gz -R Rawframe-Project/maul-unicode`. If release-please itself failed in the push run, run the release workflow again by hand (`workflow_dispatch`); if only `assets` failed, re-run that job from its run, since a second release-please run finds the release made and starts no `assets`.

## Notes

- A new Unicode version is a minor release, never a patch: results change. See [development.md](development.md#a-new-unicode-version).
- The shared family files must match their canonical copies in the organization's `.github` repository before a release.
- A library that pins this one moves its pin in a pull request of its own.
