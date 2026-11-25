# API and class design — cap-resolver

**Purpose:** Version/feature solving and override precedence.

**Status:** proposed interfaces. No signatures below should be considered an implemented or ABI-stable API.

## Component and class inventory

| Type | Kind | Planned methods / fields |
|---|---|---|
| `VersionRequirement` | value | `parse(), matches(version)` |
| `PackageResolver` | service | `resolve(request,source_index)->Result<Resolution>` |
| `FeatureResolver` | service | `unify(package_graph,requested)->FeatureGraph` |
| `OverrideResolver` | service | `effective(cli,local,workspace,registry)->Overrides` |
| `ConflictExplainer` | service | `explain(incompatibility)->Diagnostic` |
| `Resolution` | value | `resolved package ids, features, dependency edges` |

## Interface and lifecycle contract

- Public request values are immutable or owned; avoid borrowed views that outlive a call.
- Fallible methods return `std::expected<T, Diagnostic>` or a documented error domain.
- Inject filesystem, process, clock, network and store dependencies through narrow interfaces (DIP).
- Interfaces represent one responsibility; concrete adapters implement platform-specific behavior (SRP/ISP).
- No implicit shell commands or untracked mutable process-global state.
- Cancellation, timeouts, resource cleanup, and concurrency safety must be specified for I/O APIs.
- Serialization formats and persisted identifiers are versioned; deterministic ordering is mandatory.

## Invariants

Overrides precede normal resolution; local overrides do not mutate committed lock.

## Verification requirements

- **Unit / property tests:** diamond graph, cycles, conflict explanation, optional/dev feature scopes, override precedence.
- **Benchmarks:** large dependency graph resolution.
- **Coverage:** >=85% line, >=75% branch where available; >=95% line for critical algorithms.
- **Profiling:** capture baseline, flamegraph or equivalent sample evidence, and regression comparison for hot paths.
- **Sanitizers:** ASan/UBSan, TSan for concurrency, MSan on supported instrumented toolchain; document unsupported combinations.
- **SOLID review:** explain each new public type responsibility, interface segregation, and substitution tests.

## Definition of done

Every public type has an implementation, compile-time API test, negative-path tests, ownership and thread-safety documentation, a contract fixture, and performance evidence where relevant. An inventory entry is not evidence that code exists.
