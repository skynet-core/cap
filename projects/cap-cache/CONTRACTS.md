# Contracts — cap-cache

## Public boundary
Responsibility: Content-addressed source and artifact cache. Define narrow request/result interfaces in `include/cap/cache/`. Interfaces are specifications, not compiled headers yet.

## Required contract fields
- Request: typed input identity, configuration, target/host context as applicable, cancellation token, provenance.
- Result: typed value or structured diagnostic with code, location, causal chain, remediation.
- Determinism: stable ordering and canonical encoding for persisted/hashed output.
- Ownership: caller owns request; returned artifacts immutable or move-owned; no dangling filesystem or string views.
- IO: injected adapter for filesystem/process/network; no hidden global mutable state.
- Errors: invalid input, missing resource, unsupported capability, integrity failure, cancellation, transient IO.
- Compatibility: version all serialized protocols and files; migrations require ADR and golden fixtures.

## Module-specific invariant
Digest includes toolchain/flags/env/inputs; atomic writes; cache cannot change semantics; pin active artifacts.

## Contract tests
Cache hit/miss, corruption, race, GC during build, env invalidation, module cache isolation.

## Acceptance
Every public API has documented pre/postconditions, unit and integration tests, benchmark for hot paths and explicit failure behavior before implementation is considered complete.

## Exhaustive planned type inventory
See [API.md](API.md). Every type there requires a public contract and verification before merge.

## v1.4 normative additions
All fingerprints include isolation policy and complete toolchain/SDK identities; remote-cache reuse requires strict-mode conformance. Publish verified outputs atomically, reject corrupt hits, explain misses with stable reason codes. Never upload secrets or trust unverified remote results.
