# API and class design — cap-core

**Purpose:** Shared primitives and platform-neutral infrastructure.

**Status:** proposed interfaces. No signatures below should be considered an implemented or ABI-stable API.

## Component and class inventory

| Type | Kind | Planned methods / fields |
|---|---|---|
| `Diagnostic` | value | `code, severity, span, causes; format()` |
| `Result<T>` | alias | `std::expected<T,Diagnostic>` |
| `IFileSystem` | interface | `read(path), write_atomic(path,bytes), enumerate(path), canonicalize(path)` |
| `IProcessRunner` | interface | `run(ProcessSpec)->Result<ProcessResult>` (StopToken wiring is T05) |
| `ProcessSpec` | value | `argv, cwd, env_allowlist, stdin, timeout` |
| `Digest` | value | `from_bytes(), parse(), hex()` |
| `StopToken` | value | `is_stop_requested(), request_stop()` |
| `IClock` | interface | `monotonic_now()` |
| `SystemClock` | adapter | `IClock` over `std::chrono::steady_clock` |
| `ILogger` | interface | `emit(Diagnostic)` |
| `CaptureLogger` | adapter | deterministic in-memory `ILogger` sink; `lines()` |

## Implementation status

Implemented and verified (`make core-test`, pinned brew GCC 16, C++26):
- `Severity`, `ErrorCode`, `Span`, `Diagnostic` with deterministic `format()` — named module `cap.core.diagnostic`, `diagnostic.cppm`.
- `Result<T>` (`std::expected<T, Diagnostic>`) and `Status` (`std::expected<void, Diagnostic>`) — named module `cap.core.diagnostic`; consumers use `import cap.core.diagnostic;` and link its object.
- `IFileSystem` + `LocalFileSystem` (read, atomic write via temp+fsync+rename, sorted enumerate, canonicalize) — named module `cap.core.filesystem`, `filesystem.cppm`; consumers import it and link its object. `read(path)` follows symlinks; missing paths return `missing_resource`, directories retain the `unsupported_capability` diagnostic and `enumerate()` remediation, and other existing nonregular paths return `unsupported_capability` without opening them. The check is metadata preflight, not race-proof: callers must rely on a stable filesystem path and this adapter is not a sandbox. POSIX-only stage-0 adapter; see module shortcut note.
- `IProcessRunner` + `LocalProcessRunner` (`ProcessSpec`: argv direct exec, cwd, env allowlist via execve, stdin, timeout; `ProcessResult`: exit code, captured streams, timed_out) — named module `cap.core.process`, `process.cppm`; consumers import it and link its object. Executed via `execve`, never a shell. Empty argv and embedded NUL in argv or cwd return `invalid_input` before spawning. Known exec failures return diagnostics (ENOENT `missing_resource`, EACCES `unsupported_capability`), not child exit results.
  The Linux stage-0 adapter requires `close_range` with `CLOSE_RANGE_CLOEXEC` support and permission; failure prevents exec and returns a diagnostic. Successful exec inherits only descriptors 0/1/2. Closed parent stdio is unsupported; an internal endpoint allocated as fd 0/1/2 is cleaned up and rejected with a diagnostic before fork. Argument and environment pointers are prepared before fork.
  Stdin and both outputs progress concurrently; early stdin closure is normal. A positive timeout covers I/O and kills and reaps only the direct child. After its exit, buffered output is captured without waiting for inherited streams.
- `Digest` (SHA-256 per FIPS 180-4; 64-char lowercase hex; `parse()` rejects wrong length and non-hex input) — named module `cap.core.digest`, `digest.cppm`. Vectors: empty, "abc", 55/56/57-byte block-boundary messages, 1000-byte multi-block; critical-algorithm threshold >=95% line coverage.
- `StopToken` (shared sticky stop flag; polling seam) — named module `cap.core.stop_token`, `stop_token.cppm`; consumers use `import cap.core.stop_token;`. Wiring into `IProcessRunner` is T05.
- `IClock` + `SystemClock` (monotonic, non-decreasing `monotonic_now()`) — named module `cap.core.clock`, `clock.cppm`; consumers use `import cap.core.clock;`.
- `ILogger` + `CaptureLogger` (records canonical `Diagnostic::format()` single lines; deterministic). Callers redact sensitive fields before `emit`; implementations cannot infer secrets in arbitrary strings, and CaptureLogger does no further redaction — named module `cap.core.logger`, `logger.cppm`.

Contract fixtures: `tests/golden/diagnostic_format.txt` checked by `tests/diagnostic_test.cpp`; filesystem and process behavior checked by `tests/filesystem_test.cpp` and `tests/process_test.cpp`; digest vectors by `tests/digest_test.cpp`; clock/logger/stop-token behavior by `tests/primitives_test.cpp` (shared helpers in `tests/test_util.hpp`). The v1.4 proposed API additions remain unimplemented.

## Interface and lifecycle contract

- Public request values are immutable or owned; avoid borrowed views that outlive a call.
- Fallible methods return `std::expected<T, Diagnostic>` or a documented error domain.
- Inject filesystem, process, clock, network and store dependencies through narrow interfaces (DIP).
- Interfaces represent one responsibility; concrete adapters implement platform-specific behavior (SRP/ISP).
- No implicit shell commands or untracked mutable process-global state.
- Cancellation, timeouts, resource cleanup, and concurrency safety must be specified for I/O APIs.
- Serialization formats and persisted identifiers are versioned; deterministic ordering is mandatory.

## Invariants

No shell interpolation; atomic writes; no hidden global state.

## Verification requirements

- **Unit / property tests:** process quoting, symlink traversal, atomic write interruption, cancellation, UTF-8 paths.
- **Benchmarks:** process spawn latency, hashing throughput.
- **Coverage:** >=85% line, >=75% branch where available; >=95% line for critical algorithms.
- **Profiling:** capture baseline, flamegraph or equivalent sample evidence, and regression comparison for hot paths.
- **Sanitizers:** ASan/UBSan, TSan for concurrency, MSan on supported instrumented toolchain; document unsupported combinations.
- **SOLID review:** explain each new public type responsibility, interface segregation, and substitution tests.

## Definition of done

Every public type has an implementation, compile-time API test, negative-path tests, ownership and thread-safety documentation, a contract fixture, and performance evidence where relevant. An inventory entry is not evidence that code exists.

## v1.4 proposed API additions (not implemented)
- `IProcessTreeController` (interface): terminate/await process trees with timeout and escalation.
- `IIsolationAdapter` (interface): query strict/audit support and launch restricted actions.
