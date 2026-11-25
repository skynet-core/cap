# Build isolation and action identity (normative, v1.4)

**Status:** design contract, not implemented. Owners: `amp-build` (execution policy), `amp-cache` (fingerprints/CAS), `amp-core` (platform adapters), `amp-toolchain` (toolchain provenance).

## Isolation levels
- `strict`: supported-platform sandbox with declared read-only inputs, writable staging outputs, restricted environment, no network, explicit process allowlist. Required before advertising hermetic/remote execution.
- `audit`: record observed file/process/network accesses and report undeclared dependencies; not considered hermetic.
- `off`: permitted for developer diagnostics, clearly marked non-hermetic; never share its results as trusted remote-cache hits.

Unsupported strict isolation must fail clearly if requested; never silently downgrade. Platform-specific adapters must document limitations, including Windows, macOS, Linux and FreeBSD. Source generators are actions with declared tools and outputs; discovery outputs must be frozen before dependent compile actions execute. External SDKs and signing credentials require explicit non-cacheable actions or audited declared access.

## Fingerprints and artifacts
Versioned canonical encoding includes action kind, toolchain binary and SDK identities, target triple, normalized argv/cwd, relevant environment (never secret values), declared input content digests, discovered include/module dependency digests, profile/features, output mapping, sandbox policy and action protocol version. Normalize paths without conflating distinct paths or case semantics. Reject undeclared reads in strict mode. Exclude timestamps and incidental host paths where safely virtualized; where they cannot be virtualized, include them or disable cross-host reuse. Cache entries publish atomically only after output verification; readers verify digests. GC respects leases. Secret-bearing actions are non-cacheable by default.

## Remote readiness
Remote cache may be introduced only after fingerprint conformance, integrity validation and authentication are tested. Remote execution requires strict sandboxing, worker capability negotiation, authenticated transport, bounded retries of idempotent actions, and platform/toolchain matching. Never treat remote output as trusted solely because a worker returned it.

## Acceptance
Undeclared header read, ambient environment leak, network attempt, generated header race, symlink/path traversal, compiler identity change, SDK change, concurrent writer, interrupted publish, corrupt CAS entry, cache miss explanation and offline rebuild fixtures. Define golden action-key fixtures with a version and migration policy.
