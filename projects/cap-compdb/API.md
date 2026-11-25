# API and class design — cap-compdb

**Purpose:** Clang-compatible JSON compilation databases.

**Status:** M1 deterministic JSON projection implemented by `cap.compdb`.

## Component and class inventory

| Type | Kind | Planned methods / fields |
|---|---|---|
| `CompilationDatabaseExporter` | service | `export(BuildPlan,selection)->Result<Database>` |
| `CompilationEntry` | value | `directory, file, arguments, optional output` |
| `ActiveConfiguration` | value | `workspace, target, profile, features, test mode` |
| `CompilationDatabaseWriter` | service | `write_atomic(path,entries)` |
| `CompdbValidator` | service | `validate(entries,plan)` |
| `CompdbSelector` | service | `activate(config), list_configs()` |

## M1 JSON projection

`cap::compdb::to_json(const std::vector<cap::build::Action>&) -> cap::core::Result<std::string>`
serializes compile actions in supplied order. Non-compile actions are ignored.
Each compile action must have absolute, nonempty, NUL-free UTF-8 directory,
primary input and output, and absolute nonempty UTF-8 argv[0]. Remaining argv
values may be empty but must be NUL-free valid UTF-8. It uses only the frozen
action projection (`directory`, `inputs.front()`, `arguments`, `outputs.front()`).
The result is a JSON array with exactly `directory`, `file`, `arguments`, and
`output`; errors use `invalid_input`. The function is pure and thread-safe.
Tests cover planner-produced actions, filtering, order, escaping and malformed
projections. No writer, activation, sorting, or flag planning is provided.

## Interface and lifecycle contract

- Public request values are immutable or owned; avoid borrowed views that outlive a call.
- Fallible methods return `std::expected<T, Diagnostic>` or a documented error domain.
- Inject filesystem, process, clock, network and store dependencies through narrow interfaces (DIP).
- Interfaces represent one responsibility; concrete adapters implement platform-specific behavior (SRP/ISP).
- No implicit shell commands or untracked mutable process-global state.
- Cancellation, timeouts, resource cleanup, and concurrency safety must be specified for I/O APIs.
- Serialization formats and persisted identifiers are versioned; deterministic ordering is mandatory.

## Invariants

Derived from actual compile actions; no compilation needed; active config explicit.

## Verification requirements

- **Unit / property tests:** spaces in paths, test inclusion, platform filtering, modules, clangd fixture.
- **Benchmarks:** export 100k translation units.
- **Coverage:** >=85% line, >=75% branch where available; >=95% line for critical algorithms.
- **Profiling:** capture baseline, flamegraph or equivalent sample evidence, and regression comparison for hot paths.
- **Sanitizers:** ASan/UBSan, TSan for concurrency, MSan on supported instrumented toolchain; document unsupported combinations.
- **SOLID review:** explain each new public type responsibility, interface segregation, and substitution tests.

## Definition of done

Every public type has an implementation, compile-time API test, negative-path tests, ownership and thread-safety documentation, a contract fixture, and performance evidence where relevant. An inventory entry is not evidence that code exists.
