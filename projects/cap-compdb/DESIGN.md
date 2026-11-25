# Design — cap-compdb

## Ownership
Consumes a read-only immutable planned-action view from `cap-build`. `cap-compdb` never infers compiler flags from TOML independently, invokes compilers, or owns the build scheduler. Compiler actions are exported using the standard JSON compilation database fields `directory`, `file`, `arguments`, and optionally `output`. Prefer `arguments` to shell-escaped `command`. Never emit secrets from environment or command arguments.

## Configuration identity
A configuration key includes workspace identity, host/target, resolved toolchain digest, profile, feature resolution, test inclusion, and dependency override fingerprint. Distinct configurations get distinct databases under `.cap/compdb/configurations/<digest>.json`. The workspace-root `compile_commands.json` is an atomic copy or link to an explicitly selected active configuration; do not silently switch it as a side effect of an unrelated build. Provide opt-in per-member exports. Gitignore generated database files by default but allow committing if the team chooses.

## Source selection and conflicts
Export only source translation units eligible for the selected target and configuration. Include `_test.cpp` only in test-enabled configurations. Multiple compilation variants for one source can coexist in a database, but clangd may choose one unpredictably; warn and provide target-specific exports. For header navigation, rely on clangd inference and documented `compile_flags.txt` fallback, not invented entries. Include real generated include directories and compile definitions from the plan.

## Modules and generated inputs
Export C++ module interface compile actions with compiler-specific arguments; maintain a separate module-dependency metadata view for cap. JSON compilation databases do not encode full module ordering or guarantee existing BMIs. Emit actionable diagnostics if clangd cannot index until prebuild artifacts exist. Generate compdb before full build where inputs can be planned; never claim generated artifacts exist if they do not.

## Reliability and performance
Canonical JSON formatting, sorted stable output, atomic replacement, no rewrite if content is unchanged, workspace locking for concurrent writers, clear diagnostics for invalid/unavailable actions. Benchmark large workspaces and incremental refresh; profile serialization and plan extraction. Validate path handling (spaces, Unicode, Windows drive paths), argument boundaries and security-sensitive flags.

## SOLID
Single responsibility for projection and publication; depend on narrow read-only plan and filesystem abstractions, injected JSON writer/atomic publisher; support future export formats without modifying build planner.
