# Remote caching and execution (future capability, v1.4)

Remote cache is a post-local-CAS milestone; remote execution follows strict isolation and verified action keys. Proposed owners: `amp-remote` (protocol/client) and `amp-worker` (worker process). **These are future modules, not part of the original 20-module implemented or specified inventory.** No network protocol, service or working code is included in this bundle.

Require authenticated encrypted transport, authorization scoped to workspace and artifact, worker capability negotiation (OS/arch/toolchain/SDK), input digest verification, output digest verification, cancellation, leases, bounded concurrency and idempotent retry policy. Separate cache read/write rights; reject untrusted artifact metadata and avoid uploading secrets. A remote failure must allow a documented local fallback when configured, without silently accepting incompatible workers.

Rollout: (1) deterministic local action keys and strict sandbox; (2) optional authenticated remote CAS; (3) cross-machine cache conformance tests; (4) worker protocol ADR and threat model; (5) remote execution canary and benchmarks. Do not promise reproducible cross-platform outputs without toolchain/SDK identity and equivalence tests.
