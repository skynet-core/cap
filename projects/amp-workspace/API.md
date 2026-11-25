# API and class design — amp-workspace

## M1-C03 local dependency graph

`amp.workspace.dependencies` exports `DependencyPlan` with `edges` (one vector
of workspace-member indices per member, in dependency declaration order),
`binary_edges` (per member, per manifest binary, indices in depends-on order,
or declaration order if omitted), and `build_order` (member indices). It exports
`resolve_dependencies(const Workspace&, const std::vector<PackageMember>&,
const core::IFileSystem&) -> core::Result<DependencyPlan>`.

Inputs must be an unmodified validated workspace and selections from it; selected
records are identified by both canonical root and package name. The complete
declared graph is validated, including unselected nodes and binary selectors.
Only selected packages and their needed prerequisites appear in build order,
once each, prerequisites first. A package with a library (or no binaries)
requires all declared dependencies; otherwise it requires the union of binary
edges. Prerequisite libraries always require all their declared dependencies.
Callers may derive a requester-before-prerequisite archive order by topologically
ordering the relevant edges; reversing a dependency-first order works for
diamonds. No archive paths or implicit system libraries are returned.

Empty, absolute, NUL, escaped, nonmember and mismatched paths, non-static or
missing libraries, duplicate edges, unknown or duplicate binary selectors and
cycles fail. Own-library binary selection is not supported. Filesystem failures
propagate unchanged. Results own their data and refer to the input workspace's
member ordering; they must not be reused with a modified workspace. No global
state, cancellation, serialization or ABI stability is provided; concurrency
depends on the injected filesystem. Tests cover graph and filesystem errors.
Benchmark/profile evidence is required before treating this as a hot path.

## M1-C02 local loader contract

`amp.workspace` exports `PackageMember { std::string root; manifest::Manifest
manifest; }` and `Workspace { std::string root; manifest::Manifest manifest;
std::vector<PackageMember> members; std::vector<std::string> defaults;
std::optional<std::string> current; }`. Roots and defaults/current are canonical
absolute directory paths. Values own their contents; members retain declaration
order, with the root package first. `manifest` is the root manifest.

`load_workspace(std::string_view start, const core::IFileSystem&) ->
core::Result<Workspace>` discovers the nearest manifest and its unique enclosing
workspace, validates members, defaults and exclusions, and performs read-only
injected IO. Empty or NUL-containing start paths fail before filesystem calls.
`select_packages(const Workspace&, std::optional<std::string_view>
name, bool all) -> core::Result<std::vector<PackageMember>>` returns owned
records in workspace member declaration order (root package first), even when
defaults are declared in a different order; name and all are mutually exclusive.
Its precondition is an unmodified, validated `Workspace` returned by
`load_workspace`. Implicit selection uses the current nested member, otherwise
defaults, otherwise all members. A mixed workspace root is not a current member;
its package can be selected by name.
Unknown names and conflicting selectors fail with structured diagnostics.

Calls hold no global state and may run concurrently only when their filesystem
adapter permits it; no cancellation is provided. Filesystem and parse errors
propagate; invalid membership and selection yield invalid-input diagnostics.
No serialized format or ABI stability is promised. Unit tests cover values,
selection, filesystem validation and failure cases. Workspace traversal is not
currently a measured build hot path; performance evidence remains a release
gate if that changes.

**Purpose:** Multi-project workspace membership and graph.

**Status:** proposed interfaces. No signatures below should be considered an implemented or ABI-stable API.

## Component and class inventory

| Type | Kind | Planned methods / fields |
|---|---|---|
| `WorkspaceLocator` | service | `find_root(start)->Result<Path>` |
| `WorkspaceLoader` | service | `load(root)->Result<Workspace>` |
| `MemberExpander` | service | `expand(globs,excludes)->Members` |
| `WorkspaceGraph` | value | `members, target edges, default members` |
| `TargetOwnerIndex` | service | `owner(source)->Result<TargetId>` |
| `WorkspaceSelector` | service | `select(packages,flags)->TargetSet` |

## Interface and lifecycle contract

- Public request values are immutable or owned; avoid borrowed views that outlive a call.
- Fallible methods return `std::expected<T, Diagnostic>` or a documented error domain.
- Inject filesystem, process, clock, network and store dependencies through narrow interfaces (DIP).
- Interfaces represent one responsibility; concrete adapters implement platform-specific behavior (SRP/ISP).
- No implicit shell commands or untracked mutable process-global state.
- Cancellation, timeouts, resource cleanup, and concurrency safety must be specified for I/O APIs.
- Serialization formats and persisted identifiers are versioned; deterministic ordering is mandatory.

## Invariants

One workspace lockfile; cycles diagnosed; unique target ownership.

## Verification requirements

- **Unit / property tests:** nested workspaces, duplicate names, cycles, missing members, default members.
- **Benchmarks:** load 1000-package workspace.
- **Coverage:** >=85% line, >=75% branch where available; >=95% line for critical algorithms.
- **Profiling:** capture baseline, flamegraph or equivalent sample evidence, and regression comparison for hot paths.
- **Sanitizers:** ASan/UBSan, TSan for concurrency, MSan on supported instrumented toolchain; document unsupported combinations.
- **SOLID review:** explain each new public type responsibility, interface segregation, and substitution tests.

## Definition of done

Every public type has an implementation, compile-time API test, negative-path tests, ownership and thread-safety documentation, a contract fixture, and performance evidence where relevant. An inventory entry is not evidence that code exists.
