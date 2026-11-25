# API and class design — amp-registry-server

**Purpose:** Standalone public/private registry server.

**Status:** proposed interfaces. No signatures below should be considered an implemented or ABI-stable API.

## Component and class inventory

| Type | Kind | Planned methods / fields |
|---|---|---|
| `RegistryApi` | service | `metadata(), download(), publish(), yank()` |
| `PackageRepository` | interface | `put_if_absent(), get(), list()` |
| `BlobStore` | interface | `put(digest,stream), get(digest)` |
| `AuthPolicy` | interface | `authorize(principal,operation,namespace)` |
| `AuditLog` | interface | `append(event)` |
| `RegistryConfig` | value | `storage, auth, limits, tls` |

## Interface and lifecycle contract

- Public request values are immutable or owned; avoid borrowed views that outlive a call.
- Fallible methods return `std::expected<T, Diagnostic>` or a documented error domain.
- Inject filesystem, process, clock, network and store dependencies through narrow interfaces (DIP).
- Interfaces represent one responsibility; concrete adapters implement platform-specific behavior (SRP/ISP).
- No implicit shell commands or untracked mutable process-global state.
- Cancellation, timeouts, resource cleanup, and concurrency safety must be specified for I/O APIs.
- Serialization formats and persisted identifiers are versioned; deterministic ordering is mandatory.

## Invariants

Atomic publish; namespace ownership; audit trail; version immutability.

## Verification requirements

- **Unit / property tests:** concurrent publish, unauthorized upload, malformed archive, pagination, rate limits.
- **Benchmarks:** read QPS and publish latency.
- **Coverage:** >=85% line, >=75% branch where available; >=95% line for critical algorithms.
- **Profiling:** capture baseline, flamegraph or equivalent sample evidence, and regression comparison for hot paths.
- **Sanitizers:** ASan/UBSan, TSan for concurrency, MSan on supported instrumented toolchain; document unsupported combinations.
- **SOLID review:** explain each new public type responsibility, interface segregation, and substitution tests.

## Definition of done

Every public type has an implementation, compile-time API test, negative-path tests, ownership and thread-safety documentation, a contract fixture, and performance evidence where relevant. An inventory entry is not evidence that code exists.
