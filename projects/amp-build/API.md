# API and class design — amp-build

**Purpose:** Native immutable build DAG and execution.

**Status:** `amp.build.actions` M1 action-planning contract frozen for implementation; scheduler and workspace APIs remain proposals.

## M1 `amp.build.native_binary`

Exports `NativeBinaryRequest { directory, link_driver, executable, compiles,
link_inputs, workspace_modules }`,
`NativeBinaryResult { executable, compile_actions }`, and
`run_native_binary(request, process_runner, filesystem, check_output)`. It
accepts one binary in a caller-created exclusive canonical directory, explicit
trusted absolute compiler/link-driver paths, and ordered compile requests.
Compile cwd must equal that directory; object and executable outputs must be
direct children. It reserves `compile_commands.json` and `gcm.cache`, rejects
all path collisions including inputs with outputs, scans module-enabled C++
actions before compiling, orders parsed P1689 units, then executes the original
planned compile argv serially. Link inputs retain caller request order because
module dependency order governs compilation, not link argv. It verifies scan
reports and action outputs through the metadata-only callback; the engine
never reads or executes produced binaries. Only scan-report bytes are read
through `IFileSystem`. Each process is bounded to two minutes. `ProcessSpec`
allowlists the `PATH` variable name but does not set its value or restrict
compiler child-tool discovery; the caller/runner must supply a trusted `PATH`.
This local executor is explicitly non-hermetic. Failures stop subsequent
processes and preserve exit/timeout/stderr context. Tests:
`tests/native_binary_test.cpp`, `tests/native_binary_gcc_smoke_test.cpp`, and
`tests/native_binary_link_gcc_smoke_test.cpp`.
`link_inputs` is an owned, default-empty `LinkInputs`; its ordered archive
paths and SDK operands must be absolute and valid for `plan_link`. The executor
passes each unchanged to the terminal action and rejects collisions with
object, executable, or generated scan-report paths before any process starts.
SDK repeats are preserved.

## M1 `amp.build.native_binary` static archive execution

Exports `NativeArchiveRequest { directory, archiver, archive, compiles,
workspace_modules }`,
`NativeArchiveResult { archive, compile_actions, provided_modules }`, and
`run_native_archive(request, process_runner, filesystem, check_output)`.
It shares the standalone binary executor's path preflight, P1689 scan/order,
serial compile, bounded process and metadata output verification behavior.
The terminal action is produced only by `plan_archive`; archive output is a
direct child of the private directory and compile outputs/inputs may not
collide with it. The archiver is an explicit trusted absolute path. Success
returns the verified archive and exact ordered compile actions actually
executed with verified local BMI bindings in `provided_modules`; failure
returns a diagnostic and stops. The engine never reads or executes the archive.
Tests: `tests/native_archive_test.cpp`, `tests/native_archive_gcc_smoke_test.cpp`,
and `tests/native_workspace_modules_gcc_smoke_test.cpp`.
The appended optional `workspace_modules` defaults disengaged. When engaged it
enables GCC mapper handoff, checks external BMI files, writes one private mapper,
and maps local providers to ordinal BMI outputs. Archive results return only
verified local bindings. The disengaged path retains standalone argv and
filesystem behavior. C-only targets remain unchanged even when the optional is
engaged. Mapper writes use the injected filesystem's atomic-write operation.
When a C++ action reaches any external provider, its metadata includes the full
approved external BMI set, deduplicated in deterministic path order, plus its
required local BMI paths.

## Component and class inventory

| Type | Kind | Planned methods / fields |
|---|---|---|
| `BuildPlanner` | service | `plan(Workspace,Resolution,Toolchain)->Result<BuildPlan>` |
| `BuildPlan` | value | `immutable actions, edges, outputs` |
| `Action` | value | `kind, argv, cwd, env, inputs, outputs, toolchain digest` |
| `DependencyScanner` | interface | `scan(source,flags)->Dependencies` |
| `ModulePlanner` | service | `order_interfaces(import_graph)->Result<DAG>` |
| `ActionScheduler` | service | `execute(plan,executor,stop)->BuildReport` |
| `IActionExecutor` | interface | `execute(Action)->Result<ActionResult>` |
| `LinkPlanner` | service | `link_request(target,objects,libs)->LinkAction` |
| `ModuleUnit` | value | `object, provides, required_modules` |
| `plan_module_scan` | pure function | `Result<vector<string>>(Action, report_path)` |
| `parse_module_scan` | pure function | `Result<ModuleUnit>(report, expected_object)` |
| `order_module_units` | pure function | `Result<vector<ModuleUnit>>(units)` |
| `ModuleMap` | value | sorted logical module names to absolute lexically canonical `.gcm` paths; file existence is not established |
| `format_module_map` | pure function | `Result<string>(ModuleMap)` |
| `order_module_units` | pure function | `Result<vector<ModuleUnit>>(units, external_providers)` |
| `Action` | value | `kind, directory, arguments, inputs, outputs` |
| `CompileRequest` | value | `tool, directory, source, output, standard, include_dirs, cxx_modules` |
| `ArchiveRequest` | value | `tool, directory, output, objects` |
| `LinkRequest` | value | `tool, directory, output, objects, dependent_archives, system_libraries` |
| `plan_compile` | pure function | `Result<Action>(CompileRequest)` |
| `plan_archive` | pure function | `Result<Action>(ArchiveRequest)` |
| `plan_link` | pure function | `Result<Action>(LinkRequest)` |
| `ArchiveLinkFact` | value | `id, output, direct_prerequisites, system_libraries` |
| `LinkInputs` | value | `ordered_archives, system_libraries` |
| `plan_link_inputs` | pure function | `Result<LinkInputs>(direct_ids, archive_facts, system_libraries)` |
| `NativeBinaryRequest` | value | `directory, link_driver, executable, compiles, link_inputs, workspace_modules` |
| `NativeBinaryResult` | value | `executable, compile_actions` |
| `run_native_binary` | executor | injected process, report filesystem, metadata output check |
| `NativeArchiveRequest` | value | `directory, archiver, archive, compiles, workspace_modules` |
| `NativeArchiveResult` | value | `archive, compile_actions, provided_modules` |
| `run_native_archive` | executor | injected process, report filesystem, metadata output check |

## M1 `amp.build.actions` frozen contract

The named module `amp.build.actions` exports the owned requests, link-input
values, `ActionKind`, `Action`, and the pure planner functions above.
`Action.arguments` is a direct-exec
argv vector including absolute executable at index zero; planners never invoke a
process, shell, filesystem, or backend. Every path (tool, directory, source,
include, output, object, archive) must be nonempty, NUL-free and absolute. Empty
argv elements are rejected. Compiler/archiver/linker paths are supplied by the
trusted caller and are copied unchanged. Planner-produced paths are declared in
action inputs/outputs; directories contain the primary source/output but are not
implicit inputs.

Compile standards are exactly `c11`, `c17`, `c23`, `c++20`, `c++23`, or `c++26`;
the supported source extensions are `.c`, `.cc`, `.cpp`, `.cxx`, and `.cppm`.
The source language is inferred from the extension and standard mismatch or
unknown extensions are rejected. With `cxx_modules=true`, every C++ translation
unit, including ordinary `.cpp` importers/providers, receives `-fmodules`;
module mode is rejected for C. `.cppm` requires module mode and receives explicit
`-x c++`. Include directories become ordered `-I` arguments. Raw caller flags
are not accepted. Compile argv order is tool, `-std=...`, optional
`-fmodules`, optional `-x c++` for `.cppm`, each `-I <directory>` pair in
request order, then `-c <source> -o <output>`.

The canonical compile-action projection is read directly from the same action:
`directory=Action.directory`, `file=Action.inputs.front()`,
`arguments=Action.arguments`, `output=Action.outputs.front()`. Compile actions
always have exactly one primary source input and one primary output; no command
is regenerated for compilation-database consumers.

Archive argv uses deterministic `rcs`, followed by output and the caller's
ordered objects. `LinkRequest.system_libraries` is an ordered list of absolute,
NUL-free SDK operands returned by target-toolchain resolution. Link argv uses
output, objects, dependent archives, then those operands unchanged.
`Action.inputs` contains objects, archives, then the same operands in argv order,
retaining duplicates. Operands may be linker scripts; amp-build does not inspect
or resolve them.

`plan_link_inputs` accepts ordered direct archive IDs and caller-resolved facts.
It validates all fact identities, outputs and SDK paths, and validates references
only in the reachable closure. Closure discovery follows declared depth-first
order; among archives whose reachable dependents have all been emitted, the
earliest discovered is emitted first. Each archive is emitted once before all
its prerequisites, including prerequisites also named as direct roots. The result's system paths are binary requirements
first, then reachable archive requirements in archive order, preserving
duplicates. Reachable cycles fail with `invalid_input`. This API only plans
ordered inputs; it does not archive, link, discover tools or resolve SDK paths.

Failures return `amp::core::Result` with `invalid_input` for malformed requests.
These functions have no mutable state and are deterministic for equal requests.
Behavioral tests live in `tests/actions_test.cpp`. The real Linux compile/archive/
link/run smoke is `tests/actions_smoke_test.cpp` with checked-in inputs under
`tests/fixtures/actions-smoke/`; pass explicit tool paths and the fixture
directory, as shown by the verification command documented in `CONTRACTS.md`.

## M1 GCC module scan contract

`amp.build.modules` consumes a canonical C++ `Action` from `plan_compile` with
`-fmodules` enabled. `plan_module_scan` retains the driver, standard, module
switch, preprocessing options, source and action directory, replacing only
`-c SOURCE -o OBJECT` with `-M -MF /dev/null -fdeps-format=p1689r5
-fdeps-target=OBJECT -fdeps-file=REPORT SOURCE`. The report path must be
absolute and caller-owned. The returned argv is direct-exec only; scanning it
does not execute a process or touch files.

`parse_module_scan` accepts GCC 16 P1689 JSON version/revision 0/0, rejects
duplicate decoded JSON keys at every object depth, requires exactly one rule,
and requires a `primary-output` exactly equal to the expected object path. It
returns owned logical names for provides/requires; header-unit and other
unsupported metadata is rejected. `order_module_units` validates names and
objects, rejects duplicate providers, missing imports and cycles, and returns
a deterministic topological order using a lexical ready set (object-path tie
break). The one-argument form resolves imports only among local units. The
overload accepts caller-validated external providers as a sorted map of module
names to absolute lexically canonical `.gcm` paths; these satisfy imports
without adding compile units, but collide with any local provider of the same
name. Ordinary translation units may provide no module. `format_module_map`
emits one deterministically sorted `NAME PATH` row per binding, with spaces
supported and a final newline when nonempty; it rejects invalid names,
lexically unnormalized or relative paths, and control bytes. Scan visibility
and artifact publication are out of scope. Tests: `tests/modules_test.cpp`.
The report-to-order integration check is `tests/modules_gcc_smoke_test.cpp`;
it consumes reports emitted by the explicit local GCC 16 probe.
`tests/module_mapper_gcc_smoke_test.cpp` builds the shipped provider and
importer in distinct private directories, formats and supplies the real GCC
mapper, checks an empty-map failure, and uses `amp.compdb::to_json` over the
actual GCC compile actions to create the exact argv/cwd database consumed by
clangd with experimental module support and the GCC query driver. It links the
GCC objects and runs the result. Run it with absolute GCC, clangd, fixture-root
and fresh scratch-directory arguments.

## Interface and lifecycle contract

- Public request values are immutable or owned; avoid borrowed views that outlive a call.
- Fallible methods return `std::expected<T, Diagnostic>` or a documented error domain.
- Inject filesystem, process, clock, network and store dependencies through narrow interfaces (DIP).
- Interfaces represent one responsibility; concrete adapters implement platform-specific behavior (SRP/ISP).
- No implicit shell commands or untracked mutable process-global state.
- Cancellation, timeouts, resource cleanup, and concurrency safety must be specified for I/O APIs.
- Serialization formats and persisted identifiers are versioned; deterministic ordering is mandatory.

## Invariants

No external build systems; complete action inputs; deterministic graph; no double source ownership.

## Verification requirements

- **Unit / property tests:** header invalidation, modules, cycles, link ordering, cancellation, parallel races.
- **Benchmarks:** incremental no-op, 10k-action scheduling.
- **Coverage:** >=85% line, >=75% branch where available; >=95% line for critical algorithms.
- **Profiling:** capture baseline, flamegraph or equivalent sample evidence, and regression comparison for hot paths.
- **Sanitizers:** ASan/UBSan, TSan for concurrency, MSan on supported instrumented toolchain; document unsupported combinations.
- **SOLID review:** explain each new public type responsibility, interface segregation, and substitution tests.

## Definition of done

Every public type has an implementation, compile-time API test, negative-path tests, ownership and thread-safety documentation, a contract fixture, and performance evidence where relevant. An inventory entry is not evidence that code exists.

## v1.4 proposed API additions (not implemented)
- `ScheduleOptions` (value): `jobs`, `max_memory`, resource-class quotas, keep-going policy.
- `ActionResources` (value): CPU weight, estimated peak memory, class.
- `BuildTraceSink` (interface): versioned action lifecycle events and redaction policy.
- `ActionIsolationPolicy` (value): strict/audit/off with capability validation.
- Extend `ActionScheduler::execute` to accept scheduling options and report resource peaks, cancellation and critical-path evidence. Signature requires contract freeze.
