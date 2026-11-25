# Contracts — cap-manifest

## M1-B02 source selection boundary

The selector takes an existing canonical absolute package root and injected
`IFileSystem`, and returns sorted, unique package-relative source paths. Only
explicit sources and a binary's required entry are considered; there is no
implicit discovery or inter-target ownership check. Binary excludes subtract
from its sources and entry; an excluded, missing, unsupported or filtered entry
is an error, as is an empty target.

Paths use `/`, cannot be absolute, empty, contain NUL, `.` or `..` segments.
Patterns support `*` within one segment and `**` as a complete segment; `?`,
brackets, backslashes, terminal `**`, and other uses of `**` are errors. Exact paths must
exist. Wildcards can match nothing. All resolved paths must remain within the
root. Glob traversal does not follow directory aliases (a canonicalized child
must equal its lexical path); explicitly named in-root directory aliases are
accepted. Source-file aliases are rejected with an `invalid_input` diagnostic,
including aliases encountered by a wildcard, so an alias cannot conceal a
filtered test or platform filename. This prevents alias cycles and suffix
bypasses. Filesystem errors are propagated except a
not-a-directory result while exploring glob candidates.

The fixed host is Linux x86_64. Accepted extensions are `.c`, `.cc`, `.cpp`,
`.cxx`, `.cppm`. A terminal `_test` on `.cpp` is excluded. Recognized terminal
OS suffixes linux/windows/darwin/freebsd and architecture suffixes
amd64/x86_64/arm64 are filtered in canonical OS-then-architecture order;
unknown suffixes remain ordinary names. No other platform, generated-source or
test-target support is implied. Filesystem state can change after selection;
the caller must account for races before compilation.

## M1-B01 reader boundary

The first native reader is a pure `cap.manifest` named module. It takes manifest
bytes and a source identity, returns owned typed values or an `cap.core`
structured diagnostic, and performs no filesystem, process, network, source
discovery, path canonicalization, or symlink resolution. Callers retain their
inputs; results do not borrow them. Declaration order and string contents must
be preserved. The parser must reject unknown fields and unsupported forms
rather than silently ignoring them. Diagnostics identify the supplied source
and the offending line and column.

The first slice must accept the checked-in cap-compdb workspace fixtures,
including virtual workspaces, package and workspace declarations together,
static libraries, repeated binary targets, and local path dependencies. It
must reject malformed input, duplicate fields or tables and binary names,
invalid or missing version and required fields, invalid names, embedded NUL,
wrong types and unsupported value forms. Lexical validation is not proof that a
path stays within a workspace: logical root and realpath enforcement belong to
later workspace/source validation.

Only this TOML subset is accepted: bare ASCII keys, double-quoted single-line
strings with `\"`, `\\`, `\n`, `\r`, `\t`, `\b`, `\f` escapes, booleans,
the integer `1`, string arrays (including multiline, comments and trailing
comma), and dependency inline tables containing exactly one `path` string.
Whitespace is space or tab, with LF or CRLF line endings; comments start with
`#` outside strings. Table headers are `[package]`, `[language]`, `[workspace]`,
`[dependencies]`, `[lib]`, and repeated `[[bin]]`. The only root key is
`manifest-version`, required and equal to 1. The fields are those listed in
API.md. Package name/version, library name/sources, workspace members and binary
name/entry are required; library type defaults to `static`, the only supported
type. Names start with an ASCII letter or digit and contain only ASCII letters,
digits, `_` or `-`; the same restriction applies to system-library names. Versions are
nonempty; full semantic-version validation is deferred. Paths and glob strings
are retained verbatim, not interpreted as safe filesystem paths. No other TOML
forms are supported. Unsupported schemas and malformed input are errors.
In particular, literal or multiline strings, Unicode escapes, dotted or quoted
keys, nested tables, numbers other than the schema integer, inline tables
other than dependency paths, and bare CR line endings are unsupported.
`[language]`, `[dependencies]`, `[lib]` and `[[bin]]` require a `[package]`
somewhere in the document; a virtual workspace may not contain them. Table
order does not affect this rule. A package and workspace may coexist.

## Public boundary
Responsibility: Manifest parsing and source selection. Define narrow request/result interfaces in `include/cap/manifest/`. Interfaces are specifications, not compiled headers yet.

## Required contract fields
- Request: typed input identity, configuration, target/host context as applicable, cancellation token, provenance.
- Result: typed value or structured diagnostic with code, location, causal chain, remediation.
- Determinism: stable ordering and canonical encoding for persisted/hashed output.
- Ownership: caller owns request; returned artifacts immutable or move-owned; no dangling filesystem or string views.
- IO: injected adapter for filesystem/process/network; no hidden global mutable state.
- Errors: invalid input, missing resource, unsupported capability, integrity failure, cancellation, transient IO.
- Compatibility: version all serialized protocols and files; migrations require ADR and golden fixtures.

## Module-specific invariant
No fixed layout; deterministic sorted globs; test files never in production; unknown fields errors.

Parse and validate target-level `system-libraries` and executable `runtime-libraries` according to [ADR-0008](../../docs/ADR/0008-system-linkage-and-standalone-install.md). Preserve link declaration order and source locations; resolving actual libraries is not the parser's responsibility.

Parse optional `[build-script]` declarations with explicit paths, inputs and outputs. Parsing does not discover or execute `build.lua`. See [ADR-0009](../../docs/ADR/0009-lua-build-scripts.md).

## Contract tests
Windows/Linux/Darwin/FreeBSD and architecture suffixes, test ordering, unknown suffix, path traversal, profile cycles.

## Acceptance
Every public API has documented pre/postconditions, unit and integration tests, benchmark for hot paths and explicit failure behavior before implementation is considered complete.

## Exhaustive planned type inventory
See [API.md](API.md). Every type there requires a public contract and verification before merge.
