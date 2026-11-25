# API and class design — cap-vscode

**Purpose:** VS Code extension integrating cap without replacing clangd.

**Status:** proposed interfaces. No signatures below should be considered an implemented or ABI-stable API.

## Component and class inventory

| Type | Kind | Planned methods / fields |
|---|---|---|
| `CapExtension` | service | `activate(context), deactivate()` |
| `CapExecutableLocator` | service | `resolve(workspace,settings)->Path` |
| `CapTaskProvider` | service | `provideTasks(), resolveTask()` |
| `CapDebugConfigurationProvider` | service | `provideDebugConfigurations()` |
| `CapWorkspaceController` | service | `selectPackage(), selectTarget(), selectProfile()` |
| `CapCompdbController` | service | `generate(), activateConfiguration(), notifyClangd()` |
| `CapTestController` | service | `discoverTests(), runTests(), debugTests()` |
| `CapDiagnosticsBridge` | service | `parseCapDiagnostics(), publish()` |
| `CapStatusBar` | view | `render(active package,target,profile,toolchain)` |
| `CapTerminalRunner` | service | `spawnCap(args,workspace)->ExitCode` |

## Interface and lifecycle contract

- Public request values are immutable or owned; avoid borrowed views that outlive a call.
- Fallible methods return `std::expected<T, Diagnostic>` or a documented error domain.
- Inject filesystem, process, clock, network and store dependencies through narrow interfaces (DIP).
- Interfaces represent one responsibility; concrete adapters implement platform-specific behavior (SRP/ISP).
- No implicit shell commands or untracked mutable process-global state.
- Cancellation, timeouts, resource cleanup, and concurrency safety must be specified for I/O APIs.
- Serialization formats and persisted identifiers are versioned; deterministic ordering is mandatory.

## Invariants

Extension written in TypeScript for VS Code API; cap core remains C++26; no custom LSP; avoid overriding user settings silently.

## Verification requirements

- **Unit / property tests:** multi-root workspace, cancellation, missing cap, profile switch, Windows paths, clangd handoff.
- **Benchmarks:** activation time, refresh latency, test discovery.
- **Coverage:** >=85% line, >=75% branch where available; >=95% line for critical algorithms.
- **Profiling:** capture baseline, flamegraph or equivalent sample evidence, and regression comparison for hot paths.
- **Sanitizers:** ASan/UBSan, TSan for concurrency, MSan on supported instrumented toolchain; document unsupported combinations.
- **SOLID review:** explain each new public type responsibility, interface segregation, and substitution tests.

## Definition of done

Every public type has an implementation, compile-time API test, negative-path tests, ownership and thread-safety documentation, a contract fixture, and performance evidence where relevant. An inventory entry is not evidence that code exists.
