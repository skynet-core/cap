# Contracts — cap-bootstrap

## Public boundary
Responsibility: External-build-system-free bootstrap. Define narrow request/result interfaces in `include/cap/bootstrap/`. Interfaces are specifications, not compiled headers yet.

## Required contract fields
- Request: typed input identity, configuration, target/host context as applicable, cancellation token, provenance.
- Result: typed value or structured diagnostic with code, location, causal chain, remediation.
- Determinism: stable ordering and canonical encoding for persisted/hashed output.
- Ownership: caller owns request; returned artifacts immutable or move-owned; no dangling filesystem or string views.
- IO: injected adapter for filesystem/process/network; no hidden global mutable state.
- Errors: invalid input, missing resource, unsupported capability, integrity failure, cancellation, transient IO.
- Compatibility: version all serialized protocols and files; migrations require ADR and golden fixtures.

## Module-specific invariant
No CMake/Meson/Make/Ninja; bootstrap must state required host compiler/runtime/network; reproducible pinned input.

## Opt-in compiler feature probes
`make bootstrap-probes` verifies actual compiler support, independently of the
lightweight `prerequisites` check. It compiles a named `.cppm` module in C++26
mode and a separate `*_test.cpp` importer, then runs the importer. It also
uses a new `mktemp` directory under `BOOTSTRAP_ROOT/bootstrap-probes` and
retains that directory as evidence; stale objects or module BMIs cannot produce
a pass. A missing configured compiler or any failed compile/run is an explicit
target failure. Reflection is not part of this target or a prerequisite for
unrelated lightweight checks. A separate GCC reflection smoke succeeded
locally, but its fixture is deferred outside the repository because clice
cannot parse it; see `docs/BOOTSTRAP.md`.

## Stage-1 status validation (private)
The private C++20 named module `cap.bootstrap.stage_status`, compiled in C++26 mode before its implementation unit and importers, exports only `bool valid_stage_status(const std::string&)`; Boost.JSON is confined to the implementation unit.
After a zero exit, stage 0 parses all stdout as one JSON document, allowing JSON whitespace including a final LF. The object must have exactly four fields in any order: `stage` integer 1, `cap_core` string `ok`, `boost` string equal to the stage-0 build's `BOOST_LIB_VERSION`, and `self_check` string `passed`. Extra fields and duplicate decoded member names are rejected, including equivalent escaped names and equal values. Invalid JSON, trailing data, multiple documents, wrong shapes, types or values fail with the existing structured `integrity_failure`. A matching version is not proof of installed Boost provenance.

## Installed Boost integrity (bootstrap-only)
The build root and its install record are trusted local state. After a pinned source build, record the explicit version, full commit, and sorted inventory with SHA-256 digests of regular files under installed `include` and `lib`. Reject missing checked directories, symlinks and special entries. Before stage 0 compilation, compare the installed tree and explicit pin to the record without rewriting it. Before reusing a nonempty prefix, require that same verification; otherwise require a fresh build root, never delete the prefix. No external writer may modify the root or install during verification or building. This detects changes relative to the local record, not record authentication or independent upstream provenance.

## Local development CLI bootstrap
`make bootstrap-dev` requires an explicit exact Boost version and full 40-hex
commit and verifies the existing install and record in `BOOTSTRAP_ROOT` before
compilation. It does not fetch or build Boost. It compiles existing named
modules with the configured C++26 compiler in a fresh private directory and
reports the absolute development executable path only after `cap build --help`
succeeds. Failed directories remain for inspection. This is bootstrap-only,
not a package backend, installation, stage protocol, or self-host claim.
The existing trusted-local-state and no-concurrent-writer assumptions apply.

## Contract tests
Fresh host, offline with preseeded bundle, tamper rejection, stage-1/2 consistency, cross-host smoke.

Stage-1/2 consistency compares the same named-module fixture, ordered compile arguments after root normalization, and executed fixture output. Follow [ADR-0003](../../docs/ADR/0003-bootstrap.md#stage-comparison-design) for inputs, mismatch handling and evidence. This comparison is pending self-host implementation.

## Acceptance
Every public API has documented pre/postconditions, unit and integration tests, benchmark for hot paths and explicit failure behavior before implementation is considered complete.

## Exhaustive planned type inventory
See [API.md](API.md). Every type there requires a public contract and verification before merge.


## v1.3 mandatory bootstrap update
Implement and test the root `Makefile` stages `boost-fetch`, `boost-bootstrap`, `boost-build`, `bootstrap`, and `self-host`. Boost must be compiled from source, not merely downloaded as headers. The default is latest stable Boost, with exact tag/commit pinned for releases. Add negative tests for missing tools, bad commit, interrupted downloads, incompatible compiler ABI and absent bootstrap source. See `docs/BOOTSTRAP.md` and `docs/BOOST-POLICY.md`.

## v1.4 normative additions
Explicit pinned Boost version/full commit are required for all fetch/build stages. Reject mismatched preseeded checkout; validate offline and stage comparison fixtures. The root Makefile is bootstrap-only.
