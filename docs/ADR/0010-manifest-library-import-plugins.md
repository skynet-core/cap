# ADR-0010 — Manifest-declared library import plugins

Status: accepted requirement; implementation pending (M5).

## Decision

A package may declare a third-party library dependency prepared by an
independently installed importer plugin. The plugin protocol must support
importers for CMake, Meson and Autotools libraries without adding those tools,
their generators or their build formats to CAP core. A dependency uses the
usual CAP dependency name at the link boundary:

```toml
[dependencies]
example = { import = { plugin = "cmake-import", path = "vendor/example", target = "example" } }
```

This is proposed syntax, not supported manifest input today. `plugin` is the
ID of a workspace-approved, installed importer, not an executable path, a
command line or a request to install it. `path` identifies source within the
package; absolute paths, traversal and symlink escapes are rejected. `target`
selects a library in that source tree; it is an opaque identifier, not a flag
or shell expression. Meson and Autotools importers use the same declaration
with their own IDs. No manifest may grant permission to execute a plugin.

## Execution boundary

An explicit import operation (proposed `cap import`) prepares a declared
dependency through a versioned, out-of-process protocol. The consuming
workspace approves the plugin and external tools. Validate the locked plugin
identity before launch; the importer runs with declared inputs and a private
output directory under enforced isolation, with explicit tools, no ambient
secrets or network, and bounded resources. Fail closed if the required
isolation is unavailable. Ordinary manifest discovery, dependency resolution,
`cap build`, and no-build compilation-database export never run importers or
foreign build tools. Missing or stale imports produce an explicit diagnostic
telling the operator to prepare the import. Frozen builds consume only
verified, locked local artifacts and must not execute a plugin, use the
network, or change locks.

The plugin returns typed library metadata and staged artifacts, not shell
commands, raw compiler/linker flags or native DAG mutations. CAP validates
library kind, target/ABI and toolchain compatibility, checksums, declared
artifact and public header paths, and supported link requirements before
publishing. Such requirements refer only to validated imported dependencies
or declared SDK libraries. Lock the plugin/protocol, input and tool identities,
target/configuration and validated artifact digests. Consumer compilation and
linking remain native CAP actions. The compilation database includes only
real CAP compile actions, never invented rows for imported artifacts.

## Local acceptance

- Prepare and consume one local library fixture each for CMake, Meson and
  Autotools. Link the libraries into an CAP-built executable and run it.
  Prove the native consumer build invokes no importer or foreign build tool.
- Prove that a frozen build reuses verified local artifacts without network
  access and fails on missing or corrupt artifacts, input changes, incompatible
  ABI or lock mismatch. Reject missing or unapproved plugins, escaping paths,
  malformed responses, unsupported link requirements and failed imports
  without publishing partial results.
