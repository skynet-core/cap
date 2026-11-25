# API and class design — amp-cache

**Purpose:** Content-addressed artifact and metadata storage.

**Status:** proposed interfaces. No signatures below should be considered an implemented or ABI-stable API.

## Component and class inventory

| Type | Kind | Planned methods / fields |
|---|---|---|
| `ActionFingerprint` | service | `compute(action,inputs,environment)->Digest` |
| `ContentStore` | interface | `put(bytes), get(digest), verify(digest)` |
| `ActionCache` | service | `lookup(key), record(key,outputs)` |
| `CacheIndex` | service | `transaction(), pin(), unpin()` |
| `CacheGarbageCollector` | service | `collect(roots,leases)->GcReport` |
| `CacheLease` | value | `owner, expiry, artifact` |

## Interface and lifecycle contract

- Public request values are immutable or owned; avoid borrowed views that outlive a call.
- Fallible methods return `std::expected<T, Diagnostic>` or a documented error domain.
- Inject filesystem, process, clock, network and store dependencies through narrow interfaces (DIP).
- Interfaces represent one responsibility; concrete adapters implement platform-specific behavior (SRP/ISP).
- No implicit shell commands or untracked mutable process-global state.
- Cancellation, timeouts, resource cleanup, and concurrency safety must be specified for I/O APIs.
- Serialization formats and persisted identifiers are versioned; deterministic ordering is mandatory.

## Invariants

Canonical key encoding; verified hits; atomic concurrent writers.

## Verification requirements

- **Unit / property tests:** corruption, interrupted write, concurrent readers, GC leases, env sensitivity.
- **Benchmarks:** CAS throughput, hit lookup p95, GC scalability.
- **Coverage:** >=85% line, >=75% branch where available; >=95% line for critical algorithms.
- **Profiling:** capture baseline, flamegraph or equivalent sample evidence, and regression comparison for hot paths.
- **Sanitizers:** ASan/UBSan, TSan for concurrency, MSan on supported instrumented toolchain; document unsupported combinations.
- **SOLID review:** explain each new public type responsibility, interface segregation, and substitution tests.

## Definition of done

Every public type has an implementation, compile-time API test, negative-path tests, ownership and thread-safety documentation, a contract fixture, and performance evidence where relevant. An inventory entry is not evidence that code exists.

## v1.4 proposed API additions (not implemented)
- `CacheMissReason` (versioned enum): input/toolchain/flags/environment/policy/protocol change, absent/corrupt output.
- `ActionKeySchemaVersion` (value): canonical encoding and migration policy.
- `ArtifactPublisher` (service): verify digests and atomically publish output sets; maintain GC leases.
