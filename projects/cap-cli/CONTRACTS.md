# Contracts — cap-cli

## M1-G04 local standalone build

`cap build` supports standalone binaries/static libraries and selected workspace
targets. Standalone packages still reject local dependencies and SDK libraries;
unsupported selections fail before probing tools. The explicit manifest must be an existing
regular `cap.toml` and must identify the loaded package exactly. Sources and
includes are canonical package-contained regular files and directories.
Unspecified standards default to C23 and C++26. Explicit absolute tools are
trusted; compiler child tools use the caller's trusted PATH. No shell, network,
external build system or built program is invoked.

Selected targets are validated before tool probing or execution. Canonical
source ownership must be unique across selected targets. Every invocation
uses a fresh private directory under the selected root.
The default `.cap/build` must stay within the package. Outputs use generated
names; multiple binaries use target-named subdirectories and the library uses
`lib.<name>`, while a single target retains the invocation directory. Libraries
produce `lib<name>.a` using the native archive executor and vetted public include
directories. A selected binary does not implicitly depend on the library.
One compilation database is
atomically published there only after all builds succeed. No workspace-root
database is written. Help performs no IO. Success prints an `artifact: PATH`
line per target in selection order, followed by `compdb: PATH`; errors use canonical
core diagnostics. This initial Linux implementation does not protect against
concurrent hostile mutation of package or build-root paths.

## M1-G05B workspace execution

An implicit workspace root or an explicit canonical `cap.toml` identifying
either the owning workspace or an actual member selects workspace mode.
Existing package/default-member and target selection precedence is preserved.
Dependency paths are validated by `workspace::resolve_dependencies`; only the
selected target closure is built, with required archives before requesting
binaries. Archive and binary sources/includes are canonical and package
contained; binaries inherit public include directories from required archives.
Standards come from each owning member. SDK names on required archives and
requested binaries are resolved before compilation by the selected C++ driver;
archive and SDK operand order comes only from `build::plan_link_inputs`.

Workspace outputs live under one fresh invocation directory in the owning
workspace build root, with deterministic member/target-separated directories.
Only requested roots are reported, in selection order. One compilation
database contains only actually executed compile actions and is atomically
published after every requested build succeeds. Invalid preflight or later
execution failure does not publish a database or replace prior invocations.
Each workspace target receives only verified module bindings from successfully
built archives in its prepared transitive member closure, merged in member
order. Duplicate visible module names fail with a diagnostic naming the module.
Workspace requests use an engaged module map even when empty; standalone
requests retain the disengaged behavior. Compilation actions remain the exact
actions returned by the executors.

## M1-G01 pure build selection

`cap.cli.build_options` parses only tokens following `build`. Separate
`--flag VALUE` tokens are required; `--help` must stand alone. Tool paths
`--cc`, `--cxx`, and `--ar` are required absolute, nonempty, NUL-free paths;
validation is syntactic only, not an executability check. Optional path and
selector values are nonempty and NUL-free. Unknown, repeated, missing or
incompatible flags fail with invalid-input diagnostics. Values are owned.

Selection accepts a validated, unmodified workspace from `load_workspace` and
uses `select_packages` unchanged. Target IDs refer to that workspace's member
and binary ordering; no binary index denotes its static library. Unfiltered
selection emits each library before its declared binaries; `--lib` filters to
libraries and `--bin NAME` requires exactly one match in the selected scope.
No targets is an error. IDs become invalid if the workspace is modified.
No filesystem, process, network, path resolution, dependency closure or build
execution occurs. No executable integration is implied.

## M1-G05A workspace target plan

`plan_workspace_targets` consumes selected target IDs and a dependency plan
from the same validated workspace. It validates all exposed indexes and plan
shape; output contains dependency libraries in the existing `build_order`,
deduplicated, followed by selected binaries in input order. A selected library
is itself a root whose transitive `edges` are included. A selected binary uses
only its exact `binary_edges`, then follows transitive library edges; unrelated
sibling binary dependencies are omitted. The function is pure and does not IO,
sort a new DAG, compile, link SDKs, or execute CLI commands.
If a required library is absent from `build_order`, planning fails with
`invalid_input` rather than returning a partial target list. Allocation failures
may escape as `std::bad_alloc` or `std::length_error`.

## Public boundary
Responsibility: Unified user-facing commands. Define narrow request/result interfaces in `include/cap/cli/`. Interfaces are specifications, not compiled headers yet.

## Required contract fields
- Request: typed input identity, configuration, target/host context as applicable, cancellation token, provenance.
- Result: typed value or structured diagnostic with code, location, causal chain, remediation.
- Determinism: stable ordering and canonical encoding for persisted/hashed output.
- Ownership: caller owns request; returned artifacts immutable or move-owned; no dangling filesystem or string views.
- IO: injected adapter for filesystem/process/network; no hidden global mutable state.
- Errors: invalid input, missing resource, unsupported capability, integrity failure, cancellation, transient IO.
- Compatibility: version all serialized protocols and files; migrations require ADR and golden fixtures.

## Module-specific invariant
Consistent diagnostics and exit codes; no implicit network in frozen; builtins win over plugins.

`cap install --standalone` selects the install root in order: `--root DIR`, `CAP_INSTALL_ROOT`, then `$HOME/.local`. On supported Unix targets use `bin/` and `lib/` under that root. Validate the complete runtime closure and destination before publication; see [ADR-0008](../../docs/ADR/0008-system-linkage-and-standalone-install.md).

## Contract tests
Help snapshots, argument parsing, ambiguous package, signal forwarding, color/no-color.

## Acceptance
Every public API has documented pre/postconditions, unit and integration tests, benchmark for hot paths and explicit failure behavior before implementation is considered complete.

## Exhaustive planned type inventory
See [API.md](API.md). Every type there requires a public contract and verification before merge.

## v1.4 normative additions
Render structured cache-miss reasons, resource limits and trace output without leaking secrets. Unsupported isolation modes or flags must return explicit diagnostics, never silently fall back.
