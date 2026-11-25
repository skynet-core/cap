# Design — cap-bootstrap

## Responsibilities
Auditable stage-0 C++ source; pinned compiler acquisition; hash/signature verification; stage-1 build; self-host.

## Invariants
No CMake/Meson/Make/Ninja; bootstrap must state required host compiler/runtime/network; reproducible pinned input.

## Design approach
Use narrow, dependency-injected C++26 interfaces. Separate pure domain operations from filesystem, network and process adapters. Use `std::expected`-style structured errors, RAII, explicit cancellation and stable identities. Define input validation at the boundary; retain provenance for diagnostics. Do not expose implementation-specific Boost types in public cross-module APIs unless approved. Keep parsing, planning and execution separate. Respect root architecture and SOLID policy.

## Data flow
Caller supplies validated request -> module validates its own preconditions -> pure transformation or injected backend operation -> typed result and diagnostics -> caller persists/acts. Failure must not leave partially committed state; operations modifying disk use atomic staging when possible.

## Integration
Depends on `cap-core` for diagnostics/process/path/digests; other dependencies must be explicitly declared in CONTRACTS and reviewed. No implicit call to external build systems. Instrument metrics and trace IDs for profileable operations.

## Failure and security design
Handle malformed input, unsupported host/target capability, cancellation, filesystem races, resource exhaustion and inconsistent cached metadata. Never log secrets. Treat downloaded archives, manifest paths and plugin executables as untrusted until validated.

## Verification design
Unit, integration and negative fixtures: Fresh host, offline with preseeded bundle, tamper rejection, stage-1/2 consistency, cross-host smoke. Use deterministic fixtures, concurrency/fault injection when relevant. Report line/branch coverage, sanitizer capability matrix, benchmark baseline and profiling trace.

## Open questions
List unimplemented capabilities, compiler/toolchain compatibility gaps and public contract changes in TASKS.md; do not silently guess semantics.


## v1.3 mandatory bootstrap update
Implement and test the root `Makefile` stages `boost-fetch`, `boost-bootstrap`, `boost-build`, `bootstrap`, and `self-host`. Boost must be compiled from source, not merely downloaded as headers. The default is latest stable Boost, with exact tag/commit pinned for releases. Add negative tests for missing tools, bad commit, interrupted downloads, incompatible compiler ABI and absent bootstrap source. See `docs/BOOTSTRAP.md` and `docs/BOOST-POLICY.md`.
