# Complete planned component and class inventory

Status: **proposed design contracts, not implemented APIs**. Each listed class is planned; changes require an ADR and inventory update. See `projects/<module>/API.md` for method contracts, ownership, errors and tests.

- **cap-core** — Shared primitives and platform-neutral infrastructure; 8 planned types.
- **cap-bootstrap** — Self-hosting without external build systems; 5 planned types.
- **cap-meta** — Reflection-assisted schemas and serialization; 5 planned types.
- **cap-manifest** — TOML project schema and source conventions; 7 planned types.
- **cap-workspace** — Multi-project workspace membership and graph; 6 planned types.
- **cap-toolchain** — Download, verify, install, and select compilers and SDKs; 7 planned types.
- **cap-build** — Native immutable build DAG and execution; 8 planned types.
- **cap-cache** — Content-addressed artifact and metadata storage; 6 planned types.
- **cap-resolver** — Version/feature solving and override precedence; 6 planned types.
- **cap-sources** — Registry/Git/path/workspace source acquisition; 6 planned types.
- **cap-lock** — Deterministic TOML dependency/toolchain/plugin locks; 5 planned types.
- **cap-test** — Go-style test source selection and runners; 5 planned types.
- **cap-quality** — Sanitizers, coverage, benchmarks, profiling and PGO; 6 planned types.
- **cap-registry** — Authenticated package publishing and retrieval; 5 planned types.
- **cap-registry-server** — Standalone public/private registry server; 6 planned types.
- **cap-plugin** — Cargo-like external command discovery and lifecycle; 6 planned types.
- **cap-plugin-sdk** — Language-neutral plugin protocol and C++ SDK; 6 planned types.
- **cap-cli** — User-facing command parsing and orchestration; 6 planned types.
- **cap-compdb** — Clang-compatible JSON compilation databases; 6 planned types.
- **cap-vscode** — VS Code extension integrating cap without replacing clangd; 10 planned types.
