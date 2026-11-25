# TOML schema contracts (proposed v1)

## Manifest
`manifest-version = 1`; `[package] name, version, license, publish`; `[language] c, cpp`; `[workspace] members, exclude, default-members, resolver`; `[workspace.dependencies]`; `[workspace.toolchain]`; `[lib] name,type,sources,headers,public-include-dirs`; repeated `[[bin]] name,entry,sources,exclude,include-dirs,depends-on`; `[dependencies]`, `[dev-dependencies]`, `[features]`, `[options]`, `[profile.NAME]`, `[toolchain]`, `[target.TRIPLE]`, `[test]`, `[registries.NAME]`, `[workspace.plugins]`.

`[lib]` and `[[bin]]` may declare ordered `system-libraries` (bare linker names). `[[bin]]` may also declare `runtime-libraries` (exact shared-library filenames for dependencies absent from loader metadata). Neither field accepts paths or flags. See [ADR-0008](ADR/0008-system-linkage-and-standalone-install.md).

Optional `[build-script] path, inputs, outputs` declares a host-run Lua generator. Inputs are package-relative and outputs are relative to the configuration's build output directory. See [ADR-0009](ADR/0009-lua-build-scripts.md).

Planned: an imported library dependency uses an `import` source with `plugin`, `path` and `target` under `[dependencies]`. The plugin is an installed, approved ID, and the source path is package-relative. Import preparation and locked artifact consumption are separate operations. See [ADR-0010](ADR/0010-manifest-library-import-plugins.md); the current parser does not accept this source kind.

## Source-selection grammar
Parse extension first (.c,.cc,.cpp,.cxx,.m,.mm and recognized module extensions); remove `_test` if terminal before extension; then optional `_ARCH` and `_OS` from known tables. Canonical `<stem>[_<os>][_<arch>][_test].<ext>`. Unknown suffixes are normal stem text. Legacy `stem_linux_test.cpp` works. `stem_test_linux.cpp` is not canonical and requires explicit manifest inclusion unless alias compatibility enabled. Platform-specific headers are not automatically compiled. Explicit `sources` still undergo suffix selection unless `platform-filter=false` is explicitly set. Generated sources must be declared.

## Local overrides
Ignored `.amp/config.local.toml` at workspace root, `[overrides]` entries keyed by canonical package selector; `{path=...}` or `{git=...,rev=...}` or `{version=...,registry=...}`. CLI > local > committed `[patch]` > dependency resolution. Local override affects effective local resolution and cache fingerprint, never committed `amp.lock` silently. `amp publish` refuses unresolved local overrides. Local Git branches resolve to commit hashes for each build.

## Lockfiles
`amp.lock`: `lock-version`, canonical `[[package]] id,name,version,source,checksum,dependencies`, optional features; sorted deterministic encoding. `amp-toolchain.lock`: host, target, component version, bundle digest, catalog trust identity. `amp-plugins.lock`: command, version, platform artifact digest, source. `--locked` no changes; `--frozen` no network and no lock changes.

## Typed options
`[options]` bool/enum/int/string with default and validation. Options distinct from additive features. Option-to-definition or generated-header mapping explicit; no arbitrary TOML scripting. Platform manifest conditions use target-qualified tables with precise precedence and conflict validation.

## Plugin manifest
`plugin-version=1`, `[plugin] name,version,description`, `[command] name,entry`, `[compatibility] api`, `[distribution] targets,digests`. Native plugins are trusted code; permissions are informational until enforced sandbox exists. PATH discovery off by default.

## Schema governance
Unknown fields errors; versioned migrations; no breaking schema change without ADR and fixture; deterministic TOML serialization; clear diagnostics with span/field/path and suggested fix.
