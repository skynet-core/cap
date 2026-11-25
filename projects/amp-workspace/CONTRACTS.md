# Contracts — amp-workspace

## M1-C03 frozen local graph boundary

The dependency module consumes only validated M1-C02 workspace values, M1-B01
manifest values and injected core filesystem operations. Canonical path
containment is not a sandbox and assumes no concurrent filesystem mutation.
Every declared local edge and every binary selector is validated before any
plan is returned; cycles anywhere fail even if unselected. A path is relative
to its declaring package; `..` is permitted only if canonical containment and
membership hold. Dependency keys must equal target package names, and targets
must have static libraries. Direct edges preserve declaration order.

Selections require matching canonical root and package name. Explicit binary
selectors name declared dependencies only; an empty selector means all.
Own-library selection is explicitly rejected in this slice. Build order contains
only selected packages and their needed prerequisites, dependency first, without
duplicates; see API.md for library and binary closure semantics. No compiler,
process, network, registry, Git, inheritance, version or output resolution is
performed. Underlying filesystem diagnostics are preserved.

## M1-C02 discovery and selection

The local loader consumes the frozen `amp.manifest` parser and
`amp.core.filesystem::IFileSystem`, without process or network access. A start
directory and all members must exist; empty and NUL-containing start paths fail
before any filesystem call. Member paths are relative directories,
without `..`, absolute roots, or wildcard syntax except a final `/*` on members;
glob children expand in lexical order, and an unmatched wildcard is an error.
Excludes and defaults are explicit
relative directories, resolved canonically, must refer to declared members,
and defaults may not be excluded. Duplicate canonical members or package names,
nested workspaces and escaped canonical directories or manifest files fail.
The mixed root package is included automatically. Manifest files are resolved
canonically and must remain directly inside their canonical member directory.
Enclosing workspace ambiguity and nonmember packages are errors.
At the workspace root, implicit selection uses defaults when present, otherwise
all members; inside a nested member it uses that member. Selected results follow
member declaration order, not default-member declaration order.

This is a Unix local-worktree implementation, not a race-proof sandbox: callers
must prevent concurrent mutations while loading. Path dependency traversal and
topological ordering are provided separately by M1-C03; this API is not a build plan and
must not be used as one. No existing build consumes it. No source discovery,
lockfile, toolchain or execution is performed.

## Public boundary
Responsibility: Multi-package workspace planning. Define narrow request/result interfaces in `include/amp/workspace/`. Interfaces are specifications, not compiled headers yet.

## Required contract fields
- Request: typed input identity, configuration, target/host context as applicable, cancellation token, provenance.
- Result: typed value or structured diagnostic with code, location, causal chain, remediation.
- Determinism: stable ordering and canonical encoding for persisted/hashed output.
- Ownership: caller owns request; returned artifacts immutable or move-owned; no dangling filesystem or string views.
- IO: injected adapter for filesystem/process/network; no hidden global mutable state.
- Errors: invalid input, missing resource, unsupported capability, integrity failure, cancellation, transient IO.
- Compatibility: version all serialized protocols and files; migrations require ADR and golden fixtures.

## Module-specific invariant
One workspace lock; no recursive workspace ambiguity; source ownership explicit; workspace inheritance deterministic.

## Contract tests
Nested member conflicts, duplicate names, target cycles, workspace=true inheritance, glob exclusions.

## Acceptance
Every public API has documented pre/postconditions, unit and integration tests, benchmark for hot paths and explicit failure behavior before implementation is considered complete.

## Exhaustive planned type inventory
See [API.md](API.md). Every type there requires a public contract and verification before merge.
