# Proposed milestone plan v1.4

This is an updated implementation ordering, not evidence of completed code. Existing module phase details remain in `ROADMAP.md`.

- **M0 reproducible bootstrap:** explicit verified Boost version+commit, preseed/offline validation, C++26 feature probes, stage-1 executable, stage comparison design.
- **M1 native vertical slice:** manifest/workspace, toolchain, one compile+link target including declared system libraries, exact compile command export.
- **M2 fast local engine:** immutable DAG, memory-aware scheduler, C/C++ dependency scanning, local CAS, rebuild explanations and traces.
- **M3 isolated reproducibility:** strict/audit sandbox capability matrix, frozen/offline dependency graph, deterministic lockfiles, generated source dependencies, and opt-in [`build.lua` actions](ADR/0009-lua-build-scripts.md).
- **M4 developer workflow:** test/quality runners, clangd/VS Code integration, sanitizer and profiler matrix, trace inspection, and [standalone Linux/ELF installation](ADR/0008-system-linkage-and-standalone-install.md).
- **M5 ecosystem and distribution:** registry, plugin protocol, [manifest-declared library imports](ADR/0010-manifest-library-import-plugins.md) after isolation gates, verified publishing; optional remote CAS after isolation gates; remote workers only after security review.
- **M6 release hardening:** multi-host compatibility, fault injection/fuzz, offline bootstrap, stage-2 self-host, signing and measured regression budgets.

Cross-cutting gates: frozen versioned contracts before parallel agent lanes; every milestone must show executable fixtures and passing local and CI verification on its exact candidate commit. Development code changes are committed and pushed to `origin` topic branches. CI does not imply AMP has distributed build infrastructure.
