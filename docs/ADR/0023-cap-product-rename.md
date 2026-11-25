# ADR-0023 — Rename AMP to CAP

Status: operator and affected owners approved the R1 naming contract.

## Decision

The product is **CAP — C++ Awesome Packager**. Make the name change before
the first release. The command becomes `cap`; bootstrap executables become
`cap-bootstrap` and `cap-stage1`. Rename project-owned component and library
names, C++ namespaces and named modules to `cap`. Keep third-party names,
external model identifiers, user-defined build targets, and algorithms
unchanged. This does not rename existing refs or rewrite any history.

CAP discovers `cap.toml`, refers to `cap.lock`, `cap-toolchain.lock` and
`cap-plugins.lock`, and keeps its generated and local state in `.cap/`.
CAP-prefixed public environment names replace AMP-prefixed names. Rename
product-owned protocol fields, including `amp_core` in the paired bootstrap
driver and status program. Do not emit two names or auto-read the old
locations. An explicit `--manifest-path` naming `amp.toml` is invalid.
Keep old private and build paths ignored to prevent accidental staging.
Manifest fields and numeric format versions do not change.

Keep canonical workspace and member path validation, tool probing, action
order and compilation-database format unchanged. For default build roots,
only the private `.amp/build` spelling changes to `.cap/build`. Keep the
existing fresh per-invocation output directory and module mapper behavior;
never read or reuse old build or module state. Compare each new raw
compilation database with the compiler calls made by the new executable,
not with old path strings. This rename adds no cache path or cache behavior.

The Debian and Fedora package is `cap-cli` and installs `/usr/bin/cap`.
The tar archive contains `bin/cap` relative to its extraction prefix.
Update the root README, package metadata, workflows, fixture manifests,
schemas, and owned code together. CI may produce candidate packages; owner
review and green checks do not authorize publication of a release.

The verification pins are GCC 16.2.0, Clang/clang-format 23.1.6, and
Boost 1.92.0 at `afdfa32505af73e3d208144b3f623f0096cb62b6`. The first
packages target Linux x86-64 / glibc 2.41 or newer. The standalone
executable uses the system loader and libc/libm; Boost and the GCC
runtime are linked statically. Do not change toolchain ABI settings,
compiler flags, supported targets, or user-project linkage as part of
the naming change.

## Migration

- Rename all workspace and member manifests from `amp.toml` to `cap.toml`,
  and update explicit manifest paths in scripts and editor settings.
- Rename any applicable lockfiles without changing their contents. Move
  local overrides to `.cap/` and update their settings or environment
  names. Do not copy generated build or cache state.
- Update direct C++ imports, namespaces, executable names, and bootstrap
  status consumers together. Prior compiler module interfaces are not
  interchangeable with CAP's interfaces.

## Acceptance

- Per-lane API and contract names, schemas, and fixtures agree with these
  rules. Positive controls cover CAP discovery and an explicit `cap.toml`;
  the old manifest name produces an early failure without tool or output
  publication. Legacy private paths remain ignored, not used.
- Pinned GCC and Clang compile the renamed named modules in C++26 mode.
  Run existing core/bootstrap/native tests, static checks, compdb
  regression fixtures, and representative imported hover/reference checks.
  Build and run all four native modes; compare raw compdb with the actual
  compiler invocations, preserving argv, cwd, source and object.
- `.deb`, `.rpm`, and `.tar.gz` contain the same CAP executable and license
  texts with verified checksums. Install and remove native packages in
  minimal Debian and Fedora containers, and relocate the unprivileged tar
  install. Check that the ELF needs only OS runtime libraries; no toolchain
  or Boost is needed for `cap build --help`.
- Push only changes based on the clean public preview to a topic branch.
  Verify hosted build and packaging jobs; do not publish a tag or release
  or update the default branch as part of the rename.
