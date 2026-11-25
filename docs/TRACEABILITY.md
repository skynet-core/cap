# Feature-to-owner traceability

- **amp-core** — Diagnostic/result types; filesystem paths; cryptographic digests; cancellation; process execution; logging; environment capture; tests: Path normalization, atomic writes, process exit/signal handling, timeout and cancellation, error propagation.
- **amp-meta** — Reflection-assisted TOML mapping; typed schema descriptors; CLI option metadata; validation metadata; tests: Optional fields, enum mappings, unknown fields, invalid values, nested structures, compiler conformance.
- **amp-manifest** — Versioned TOML schema; target entries; source globs; exclusion; platform suffix grammar; profiles; options; features; tests: Windows/Linux/Darwin/FreeBSD and architecture suffixes, test ordering, unknown suffix, path traversal, profile cycles.
- **amp-workspace** — Virtual roots; member discovery; default members; shared dependencies; target ownership; package selection; tests: Nested member conflicts, duplicate names, target cycles, workspace=true inheritance, glob exclusions.
- **amp-toolchain** — Signed catalog; platform-specific bundles; digest verification; download resume; install/verify/uninstall; compiler capabilities; tests: Tampered archive, zip-slip, interrupted downloads, unsupported sanitizer/target, offline install, concurrent installs.
- **amp-build** — Compile/link/archive actions; module scanning; target source graph; scheduler; compiler backend; compilation database; tests: Incremental header edits, module BMI invalidation, linker order, parallel cancellation, cyclic graph, platform filtering.
- **amp-cache** — CAS; action fingerprint; SQLite metadata; garbage collection; artifact integrity; workspace compdb index; tests: Cache hit/miss, corruption, race, GC during build, env invalidation, module cache isolation.
- **amp-resolver** — SemVer constraints; PubGrub-like conflict explanation; additive features; patches; local overrides; dev scopes; tests: Conflicts, diamond deps, optional feature activation, dev-only isolation, overrides of multiple sources.
- **amp-sources** — Registry archives; Git commit fetch; local path and workspace; credentials; source integrity; tests: Git branch pin, private SSH, checksum mismatch, offline fetch, source alias collisions.
- **amp-lock** — Canonical package IDs; dependency edges; checksum; feature resolution; lockfile diff; local effective state; tests: Round trip, lock merge, concurrent writes, source replacement, frozen missing cache.
- **amp-test** — *_test.cpp discovery; per-target test build; Google Test and Catch2 adapters; filtering; runner; reports; tests: Linux-only tests, multiple binaries, fixture linking, filter, cross-target runner, failure reporting.
- **amp-quality** — ASan/UBSan/TSan/MSan capability checks; coverage; perf/PGO; benchmark runner; baseline reports; tests: ASan and TSan incompatible, MSan instrumented deps, benchmark regressions, PGO stages.
- **amp-registry** — HTTP index; package download; publish/yank; credential helpers; signatures and provenance; tests: Auth failure, token redaction, upload retry, immutable release, malicious archive.
- **amp-registry-server** — Package metadata; namespace ownership; version immutable storage; index; authentication; yanking; audit; tests: Concurrent publish, duplicate version, authorization, index pagination, rollback, rate limits.
- **amp-plugin** — amp-* discovery; installed command registry; plugin manifest; install/update/uninstall; workspace plugin locks; tests: Name collision, exit propagation, malformed manifest, version conflict, malicious executable, workspace lock.
- **amp-plugin-sdk** — Protocol negotiation; argument forwarding; optional JSON messages; structured diagnostics; C++ SDK; tests: Version mismatch, malformed JSON, stderr separation, cancellation, binary paths with spaces.
- **amp-cli** — new/init/add/remove/build/run/test/bench/profile/check/clean/update/fetch/publish/toolchain/plugin/cache/override; tests: Help snapshots, argument parsing, ambiguous package, signal forwarding, color/no-color.
- **amp-bootstrap** — Auditable stage-0 C++ source; pinned compiler acquisition; hash/signature verification; stage-1 build; self-host; tests: Fresh host, offline with preseeded bundle, tamper rejection, stage-1/2 consistency, cross-host smoke.

- VS Code extension → amp-vscode/API.md, docs/VSCODE-EXTENSION.md, docs/ROADMAP.md.
- Complete planned public class/interface inventory → docs/COMPONENT-INVENTORY.md and projects/*/API.md.

## v1.4 requirement ownership
- Scheduler CPU/memory/fairness/cancellation: `amp-build` + `amp-core`; `docs/SCHEDULER.md`.
- Strict/audit isolation and fingerprint versioning: `amp-build`, `amp-cache`, `amp-core`, `amp-toolchain`; `docs/BUILD-ISOLATION.md`.
- Explain/trace/cache diagnostics: `amp-build`, `amp-cache`, `amp-cli`, `amp-quality`; `docs/BUILD-OBSERVABILITY.md`.
- Parallel agent ownership and integration: all modules, root `AGENTS.md`; `docs/AGENT-COORDINATION.md`.
- Verified pinned Boost bootstrap: `amp-bootstrap`, root Makefile; `docs/BOOST-POLICY.md`.
- Future remote CAS/worker: proposed `amp-remote`/`amp-worker` only, pending ADR and module specifications; `docs/REMOTE-EXECUTION.md`.
