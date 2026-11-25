# Linux release packaging

The [packaging workflow](../.github/workflows/release.yml) runs on relevant
branch pushes, `vX.Y.Z` tag pushes, and manual runs with an `X.Y.Z` version.
Branch pushes use `0.0.0` for test packages. The job uploads artifacts only
after the package checks pass; it does not create tags or GitHub Releases.

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

Production publication is separate. Use the artifacts from a passing run
and apply the [approval gates](LOCAL-AGENT-OPERATIONS.md#verification-and-promotion)
to the exact commit, target, and artifact digests. A successful package
job alone is not release approval.
