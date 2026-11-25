# Linux release packaging

The [packaging workflow](../.github/workflows/release.yml) runs on relevant
topic pushes, `vX.Y.Z` tag pushes, and manual runs with an `X.Y.Z` version.
Topic pushes use `0.0.0` for test packages. The job uploads artifacts only
after the package checks pass; it does not publish releases itself.

The workflow uses pinned GCC 16.2.0 and source-built Boost 1.92.0. It links
the development CLI with the GCC runtime and Boost linked statically. The
OS supplies glibc and the loader; the package targets Linux x86‑64 with
glibc 2.41 or later. This is not a fully static executable for `FROM scratch`,
an CAP-managed toolchain, official stage‑2 self-hosting, or the proposed
`cap install --standalone` command for users' projects.

The `.deb` and `.rpm` install `/usr/bin/cap`; the archive installs `bin/cap`
under any extraction prefix. All formats include the CAP, Boost, and GCC
runtime license texts. A compiler and archiver are still needed to build
projects with CAP. `SHA256SUMS` covers all three packages.

CI installs and removes the packages without network access in fresh
Debian 13 slim and Fedora 44 minimal containers. The tar test extracts and
relocates the archive as an unprivileged user. Each test checks that the
installed executable is byte-for-byte the linked executable and that
`cap build --help` runs without a toolchain installed. Use the regular
[build and test workflow](../.github/workflows/ci.yml) for end-to-end builds.

## Develop prereleases

After a human merges a milestone PR into `develop`, the
[develop workflow](../.github/workflows/develop-prerelease.yml) identifies
the merged PR by its result commit. It tests that commit, builds and
install-tests the same three package formats, then publishes a GitHub
prerelease with `SHA256SUMS` and `CHANGELOG.md`. Direct pushes and merges
from other branches do not publish. The GitHub release notes and changelog
include the milestone PR summary, its manual verification instructions,
and a link to the changes since the previous develop tip.
Use `## Changelog` and `## Manual verification` headings in the milestone
PR description so reviewers and testers see the same instructions.

Previews use package version `0.0.<run number>` and a tag bound to the full
merge commit. They are not marked latest. Stable release publication is
separate: apply the [approval gates](LOCAL-AGENT-OPERATIONS.md#verification-and-promotion)
to the exact commit, target, and artifact digests.
