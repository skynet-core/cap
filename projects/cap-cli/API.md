# API and class design — cap-cli

## M1-G04 standalone command boundary

`cap.cli.standalone` exports `run_standalone(span<const string_view>) ->
core::Result<StandaloneOutput>`. The owned output contains ordered artifact
paths and a compdb path, or help text for a help request. The caller renders errors using
`Diagnostic::format()`. This local Linux command performs filesystem and
process IO; it is not a stable ABI or a general command router.
The private `SelectedTarget` value holds validated canonical sources and
include directories for a single selected binary or static library until
execution. Archives use the selected native archiver and are reported as
`lib<name>.a`; selection and publication ordering are unchanged.

## M1-G01 implemented boundary

`cap.cli.build_options` exports `BuildOptions` (owned strings for tools and
optional paths/selectors, plus workspace, lib and help booleans), `TargetId`
(`member` index and optional `bin` index), `parse_build_options(span<const
string_view>) -> core::Result<BuildOptions>`, `select_build_targets(const
workspace::Workspace&, const BuildOptions&) -> core::Result<vector<TargetId>>`,
and `build_help() -> string_view`. Parsing accepts tokens after `build` only.
Selection requires options returned by the parser and an unmodified validated
workspace; help is not a build request. Results own their values, apart from
the static help text. Errors carry `invalid_input` or the unchanged workspace
selection diagnostic. Calls have no global mutable state and are thread-safe
for unmodified inputs. No ABI stability or serialization is promised.
Separate tests cover API types, options ownership, selection and errors; this
small front end is not a measured hot path.

## M1-G05A workspace target planning

`cap.cli.workspace_targets` exports
`plan_workspace_targets(span<const TargetId>, const workspace::DependencyPlan&) ->
core::Result<vector<TargetId>>`. Inputs must describe selected targets and a
dependency plan from the same validated workspace. It includes selected library
roots and the transitive library closure of selected binaries' exact
`binary_edges`, then emits needed libraries in `build_order` followed by selected
binaries in selection order. Malformed member/bin indexes or plan shape,
including a required library omitted from `build_order`, return `invalid_input`.
The operation is pure and performs no IO or graph sorting. Allocation failure
and container length overflow may escape as `std::bad_alloc` and
`std::length_error`, respectively.

## M1-G05B workspace execution

`run_standalone` also executes a selected workspace from its root or an
explicit canonical member `cap.toml`. Workspace roots use the existing
`select_build_targets`, `workspace::resolve_dependencies`, and
`plan_workspace_targets` contracts. Archives execute in validated dependency
order; only selected binaries execute and are reported in selection order.
Each compile action uses its owning member's canonical sources, language
standards, own include paths, and required dependency libraries' public include
paths. Binary link operands are planned before compilation with
`build::plan_link_inputs`, preserving requester-first archive and SDK order.
One fresh private invocation directory holds collision-free per-member/target
directories and one exact compilation database; only selected root targets
produce artifact paths. Workspace targets see verified named-module bindings
only from archives in their prepared transitive closure, merged in member order;
duplicate visible names fail with a diagnostic. Standalone module behavior is
unchanged. This handoff adds no public interface.
The coordinator resolves its context, validates inputs, prepares output/link
plans, executes targets, and publishes the database in that order.

**Purpose:** User-facing command parsing and orchestration.

**Status:** proposed interfaces. No signatures below should be considered an implemented or ABI-stable API.

## Component and class inventory

| Type | Kind | Planned methods / fields |
|---|---|---|
| `CommandRouter` | service | `dispatch(argv)->ExitCode` |
| `ArgumentParser` | service | `parse(schema,argv)->Result<Options>` |
| `CommandContext` | value | `workspace, config, services, stop token` |
| `DiagnosticRenderer` | interface | `render(Diagnostic,format)` |
| `ConfigLoader` | service | `merge(global,workspace,local,cli)` |
| `ProgressReporter` | interface | `start(), update(), finish()` |

## Interface and lifecycle contract

- Public request values are immutable or owned; avoid borrowed views that outlive a call.
- Fallible methods return `std::expected<T, Diagnostic>` or a documented error domain.
- Inject filesystem, process, clock, network and store dependencies through narrow interfaces (DIP).
- Interfaces represent one responsibility; concrete adapters implement platform-specific behavior (SRP/ISP).
- No implicit shell commands or untracked mutable process-global state.
- Cancellation, timeouts, resource cleanup, and concurrency safety must be specified for I/O APIs.
- Serialization formats and persisted identifiers are versioned; deterministic ordering is mandatory.

## Invariants

Thin CLI; no business logic in handlers; stable exit codes.

## Verification requirements

- **Unit / property tests:** invalid args, config precedence, help, JSON diagnostics, plugin dispatch.
- **Benchmarks:** cold start and help latency.
- **Coverage:** >=85% line, >=75% branch where available; >=95% line for critical algorithms.
- **Profiling:** capture baseline, flamegraph or equivalent sample evidence, and regression comparison for hot paths.
- **Sanitizers:** ASan/UBSan, TSan for concurrency, MSan on supported instrumented toolchain; document unsupported combinations.
- **SOLID review:** explain each new public type responsibility, interface segregation, and substitution tests.

## Definition of done

Every public type has an implementation, compile-time API test, negative-path tests, ownership and thread-safety documentation, a contract fixture, and performance evidence where relevant. An inventory entry is not evidence that code exists.

## v1.4 proposed CLI surfaces (not implemented)
- `cap build --jobs=N --max-memory=SIZE --explain --trace --critical-path`.
- `cap cache stats`; `cap doctor --repair` only after safe repair semantics are specified.
- `cap dev ...` is an optional future agent-coordination command family, not part of current runnable CLI.
