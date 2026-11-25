# Roadmap — cap-compdb

## M0: Contracts and fixtures
Freeze read-only compile-action view with cap-build. Build fixture: workspace library + two binaries + Linux/Windows sources + `_test.cpp`; establish clangd integration test and reference JSON fixtures.

## M1: Exporter MVP
Emit deterministic standard JSON with `arguments`, `directory`, `file`; atomic writes and JSON validation. Generate from plan without compiling.

## M2: Configuration matrix
Support managed toolchain paths, profiles, feature definitions, local overrides, per-package exports, test inclusion and explicit activation.

## M3: Modules and generated files
Module interface commands, generated include dirs, actionable missing-BMI/generated-input diagnostics.

## M4: Verification hardening
Coverage gates, sanitizers, clangd smoke tests on supported platforms, concurrency/race tests, large-workspace benchmarks, profiling baselines and regression thresholds.
