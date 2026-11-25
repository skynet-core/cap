# Bootstrap specification v1.3 (normative)

## Allowed bootstrap prerequisites
System C++26-capable compiler, Git, GNU Make, POSIX shell and standard host utilities. A network connection is required unless verified Boost sources are preseeded. GNU Make is permitted **only** to bootstrap cap and orchestrate quality commands. Boost's own `bootstrap.sh` and `b2` are explicitly permitted **only** to build Boost itself. CMake, Meson, Ninja and third-party build backends remain prohibited for cap projects.

## Pipeline
1. `make prerequisites`: verify tools and test compiler acceptance of `-std=c++26`. This remains flag acceptance only; it establishes neither module nor reflection support.
   `make bootstrap-probes` is the opt-in named-module check: it compiles a named C++20 module and separate importer in C++26 mode, then runs the importer. Each invocation uses and retains a fresh `mktemp` directory under `BOOTSTRAP_ROOT/bootstrap-probes`, preventing stale BMI/object reuse. It fails explicitly when the configured compiler is missing or any compile/run fails. It does not build Boost or change existing test flags.
2. `make boost-fetch`: require the explicit verified version and full commit; clone or verify the preseeded Boost superproject and recursive submodules, then record their identities.
3. `make boost-bootstrap`: run upstream `bootstrap.sh` to build `b2`.
4. `make boost-build`: compile and install required Boost libraries using `b2`, with controlled prefix, variant and parallelism.
5. `make bootstrap`: compile the stage-0 driver (`bootstrap/main.cpp`) with the system compiler and installed Boost. The driver then compiles and verifies the stage-1 executable (`bootstrap/stage1.cpp`); neither stage invokes other general build systems.
6. `make self-host` fails explicitly without building Boost: the stage-2 build and [stage comparison](ADR/0003-bootstrap.md#stage-comparison-design) are not implemented.
7. `make test/coverage/bench/profile`: invoke implemented verification commands after stage-1 is functional.

## Preconditions and verification
Do not claim success if only Boost builds. The stage-0 driver and stage-1 artifact are implemented as of M0; `make bootstrap` fails on source identity, local install integrity, compile or self-check errors. For releases, pin Boost and its submodules, check provenance, verify compiler and library ABI, test offline/preseeded builds, and run integration, coverage, sanitizer and performance gates. See `BOOST-POLICY.md`.

## Acceptance criteria
A supported clean host can build Boost via Git + GNU Make + upstream Boost bootstrap/b2, compile cap with C++26, self-host, and run the full quality suite. Trace processes to prove that cap builds do not invoke third-party build systems. Document any unsupported host platforms.

## v1.4 superseding reproducibility policy
The v1.3 `latest` development default above is superseded: Boost fetch/build requires explicit `BOOST_VERSION=<exact-tag-version>` and `BOOST_COMMIT=<verified-full-40-hex-commit>`. Both values must be independently verified against trusted upstream provenance. Existing checkout reuse must verify the revision; mismatches fail and require explicit `make boost-clean`. A fully preseeded checkout can be used offline after revision/submodule checks. `make prerequisites` remains a lightweight compiler flag probe; full feature and ABI probes are separate required tests. See `BOOST-POLICY.md`.

## M0 compiler feature evidence
On `/home/linuxbrew/.linuxbrew/bin/g++-16`, `make bootstrap-probes` verifies
actual named-module support by compiling a `.cppm` module and a separate
importer and running the importer. The fresh output directory is retained as
evidence. This target does not probe reflection.

A separate local smoke with pinned GCC 16 compiled and ran the reflection
expression using `<meta>`, `^^int`, and `std::meta::is_type`. The repository
fixture is deferred outside the candidate because the required Clang-based
clice check reports unsupported `std::meta` and `^^` syntax; this failure is
not suppressed and no clean reflection static-check result is claimed.

Before running checkout code, `boost-fetch` must verify the requested HEAD and tag, recursive submodule identities, and clean tracked files and index in the superproject and every recursive submodule. It must reject index flags that conceal changes and fail closed if a verification command or identity record write fails. Untracked and ignored files, including in submodules, are not verified: the operator must own the checkout and trust generated artifacts and all other untracked inputs, Git metadata/configuration, and the local environment. Do not use an untrusted or concurrently modified worktree. This check is neither independent authentication nor hermeticity.

After `b2 install`, the build root holds `boost-install.record`, containing the explicit pin and a NUL-delimited sorted inventory and SHA-256 checksums of every regular file under `boost-install/include` and `boost-install/lib`. Directories are inventoried too; symlinks and special entries in those trees are unsupported and rejected. GNU `find`, `sort`, `xargs` and `sha256sum` are required. Before stage 0 compilation the record is checked without rewriting it. A nonempty prefix must match the record before another install; missing or mismatched evidence requires a fresh build root, not automatic deletion. Interrupted installs similarly require a fresh root. Retained outputs and the build-root record are trusted local state, not authenticated provenance; no external writer may modify them during checking or building. The stage executables' path readability checks do not establish provenance. This is not an offline or hermetic build claim.

## Local development CLI
With an already source-built Boost install and its inventory at
`BOOTSTRAP_ROOT/boost-install` and `BOOTSTRAP_ROOT/boost-install.record`:

```sh
make bootstrap-dev BOOTSTRAP_ROOT=/absolute/local/root \
  BOOST_VERSION=<verified-version> BOOST_COMMIT=<verified-full-commit>
```

The target verifies the existing install and pin, without fetching or building
Boost. It compiles the existing CLI modules with the configured pinned GCC in
C++26 mode into a fresh private `dev/run.*` directory and prints its absolute
executable path after `cap build --help` succeeds. Failed directories are
retained. The local install, inventory, compiler and environment must be trusted
and not concurrently modified; this is not authenticated or hermetic. This
target neither changes the stage-0/1 protocol nor installs CAP or supplies a
Make backend for package builds. The development CLI builds standalone binaries
and static libraries. Use `--bin NAME` or `--lib` to narrow the selection. See
the [static library fixture](../projects/cap-cli/tests/fixtures/native-library/README.md)
for an example. In a local workspace, `--workspace` selects all members and
`--package NAME` selects one; otherwise use the manifest's default members or
the current member. Local static archive dependencies and declared SDK libraries
are supported in workspaces. With the supported GCC toolchain, workspace
targets can import C++ named modules provided by their local static-library
dependencies. The build uses target-private module files and exports the
actual compiler commands, including module mapper options, in the invocation's
compilation database. Cross-toolchain reuse and header units are unsupported.

## GitHub Actions Boost cache

Build and package CI share an exact-key cache for the pinned Boost install,
its inventory record, and the Boost license. The key binds the runner OS and
architecture, the full Boost revision, and the workflow and bootstrap recipes
that select the build container and flags. There are no fallback keys.

On a miss, CI builds Boost from source. On a hit, it verifies the installed
pin and whole recorded header/library inventory before compiling CAP. In both
cases CI separately checks the license digest. An invalid cache fails the job;
remove the bad entry and rerun rather than using its bytes. Only branch pushes
save caches; GitHub's branch and tag cache visibility rules still apply.

This cache does not store CAP binaries, compiler modules, or local settings.
It is a CI optimization, not a replacement for trusted build inputs or release
approval.
