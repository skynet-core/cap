# Complete planned component and class inventory

Status: **proposed design contracts, not implemented APIs**. Each listed class is planned; changes require an ADR and inventory update. See `projects/<module>/API.md` for method contracts, ownership, errors and tests.

- **amp-core** — Shared primitives and platform-neutral infrastructure; 8 planned types.
- **amp-bootstrap** — Self-hosting without external build systems; 5 planned types.
- **amp-meta** — Reflection-assisted schemas and serialization; 5 planned types.
- **amp-manifest** — TOML project schema and source conventions; 7 planned types.
- **amp-workspace** — Multi-project workspace membership and graph; 6 planned types.
- **amp-toolchain** — Download, verify, install, and select compilers and SDKs; 7 planned types.
- **amp-build** — Native immutable build DAG and execution; 8 planned types.
- **amp-cache** — Content-addressed artifact and metadata storage; 6 planned types.
- **amp-resolver** — Version/feature solving and override precedence; 6 planned types.
- **amp-sources** — Registry/Git/path/workspace source acquisition; 6 planned types.
- **amp-lock** — Deterministic TOML dependency/toolchain/plugin locks; 5 planned types.
- **amp-test** — Go-style test source selection and runners; 5 planned types.
- **amp-quality** — Sanitizers, coverage, benchmarks, profiling and PGO; 6 planned types.
- **amp-registry** — Authenticated package publishing and retrieval; 5 planned types.
- **amp-registry-server** — Standalone public/private registry server; 6 planned types.
- **amp-plugin** — Cargo-like external command discovery and lifecycle; 6 planned types.
- **amp-plugin-sdk** — Language-neutral plugin protocol and C++ SDK; 6 planned types.
- **amp-cli** — User-facing command parsing and orchestration; 6 planned types.
- **amp-compdb** — Clang-compatible JSON compilation databases; 6 planned types.
- **amp-vscode** — VS Code extension integrating amp without replacing clangd; 10 planned types.
