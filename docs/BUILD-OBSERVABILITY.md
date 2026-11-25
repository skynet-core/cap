# Build diagnostics and observability (normative, v1.4)

**Status:** planned. Owners: `cap-build` event emission, `cap-cache` cache reasons, `cap-cli` display, `cap-quality` benchmarks.

Emit a versioned structured event stream with stable action IDs and monotonic timestamps: plan, ready, queued, resource-admitted, started, cache hit/miss, process exit, artifact verified, cancelled and finished. Preserve deterministic diagnostic ordering in persisted reports; wall-clock durations are measurement metadata, not cache inputs. Redact tokens, credentials, private environment values and sensitive paths before logs leave the machine.

## Explainability
`cap build --explain` reports why an action rebuilt: changed source/header/module digest, toolchain, flags, environment, profile, features, sandbox policy, missing/corrupt artifact or protocol revision. `cap build --trace` emits a versioned JSON trace. `cap build --critical-path` reports the measured critical path and wait causes. `cap cache stats` reports local hit/miss, byte usage, verification errors and GC behavior. Commands are proposals until implemented and tested.

## Quality gates
Golden trace fixtures, redaction tests, cancellation traces, concurrent event ordering, high-volume tracing overhead benchmark, stable cache-miss reason codes, cross-platform clock behavior and an explicit trace schema migration policy. Trace output is diagnostic; failure to persist optional traces must not corrupt the build.
