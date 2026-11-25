# Contracts — cap-core

## Public boundary
Responsibility: Foundational primitives. Diagnostics and result aliases are exported by `cap.core.diagnostic`; the clock interface and adapter by `cap.core.clock`; the stop token by `cap.core.stop_token`; digest by `cap.core.digest`; logging by `cap.core.logger`; the filesystem interface and adapter by `cap.core.filesystem`; and process types by `cap.core.process`. Consumers import these modules and link their module objects. The rest of the inventory in API.md remains specification until implemented.

## Required contract fields
- Request: typed input identity, configuration, target/host context as applicable, cancellation token, provenance.
- Result: typed value or structured diagnostic with code, location, causal chain, remediation.
- Determinism: stable ordering and canonical encoding for persisted/hashed output.
- Ownership: caller owns request; returned artifacts immutable or move-owned; no dangling filesystem or string views.
- IO: injected adapter for filesystem/process/network; no hidden global mutable state. Process execution uses `execve` with the exact argv (no shell) and an env built from the allowlist (empty allowlist = empty environment). Filesystem writes are atomic (temp + fsync + rename in the target directory); enumerations are lexicographically sorted.
- Process stdin and both output streams progress concurrently. Early stdin closure is a normal child outcome. A positive timeout covers I/O and direct-child execution; on expiry the direct child is killed and reaped. After direct-child exit, available buffered output is captured without waiting for descendants that inherited streams. Descendants are not terminated.
- Process argv (including the executable) and cwd reject embedded NUL as `invalid_input` before spawning. Empty argv is invalid. Known `execve` failures return structured diagnostics (ENOENT: `missing_resource`, EACCES: `unsupported_capability`); child exit statuses remain results.
- The Linux stage-0 process adapter prepares argv and filtered environment before fork and requires kernel support and permission for `close_range(3, UINT_MAX, CLOSE_RANGE_CLOEXEC)`; failure is a structured diagnostic and prevents exec. On successful exec only descriptors 0, 1 and 2 are inherited. Closed parent stdio is unsupported: if an internal endpoint occupies descriptor 0, 1 or 2, it is closed and a structured diagnostic is returned before fork.
- Errors: invalid input, missing resource, unsupported capability, integrity failure, cancellation, transient IO.
- `IFileSystem::read` follows symlinks to an existing regular file. Missing paths (including dangling symlinks) return `missing_resource`; directories keep the `unsupported_capability` diagnostic and `enumerate()` remediation; other existing nonregular paths return `unsupported_capability` before open. This is a metadata preflight for stable paths, not a race-proof sandbox: concurrent hostile filesystem mutation is outside the contract.
- Compatibility: version all serialized protocols and files; migrations require ADR and golden fixtures.

## Canonical diagnostic encoding
`Diagnostic::format()` emits one deterministic line: `{severity} code={name}[ file={path}:{line}:{column}] message={text}[ causes={name,...}][ fix={text}]`.
Severity names are `info|warning|error|fatal`; error-code names are hyphenated and stable (`invalid-input`, `missing-resource`, `unsupported-capability`, `integrity-failure`, `cancellation`, `transient-io`, `internal`). `file` appears only when the span has a file; `causes` lists cause error names in chain order (root cause first); `fix` appears only when a remediation is set. Changing the encoding is a breaking contract change requiring an ADR and golden-fixture updates.

Values in `file`, `message`, and `fix` escape backslash as `\\`, ASCII controls 0x00–0x1f as `\0`, `\a`, `\b`, `\t`, `\n`, `\v`, `\f`, `\r` for their conventional bytes and uppercase `\xHH` for the rest; DEL (0x7f) is `\x7F`. Other bytes, including UTF-8, are unchanged. This deterministically guarantees no embedded LF/CR physical line breaks, not a general text or security parser; encoding changes require ADR and golden updates.

## Canonical digest encoding
`Digest` is SHA-256 per FIPS 180-4; the canonical encoding is exactly 64 lowercase hex characters. `Digest::parse` accepts case-insensitive 64-hex input and returns an `invalid_input` diagnostic otherwise (wrong length, non-hex). Known-answer vectors (empty input, "abc", 55/56/57-byte block-boundary messages, 1000-byte multi-block) are pinned in `tests/digest_test.cpp`; changing the algorithm or encoding is a breaking contract change requiring fixture updates.

## Stop, clock and log contracts
`StopToken` is a shared sticky flag: once `request_stop()` has run, `is_stop_requested()` returns true from any holder; wiring it into `IProcessRunner` is T05. `IClock::monotonic_now()` must be non-decreasing for the same observer. Callers must redact sensitive diagnostic fields before `ILogger::emit`; the logger cannot infer which arbitrary strings contain secrets. `CaptureLogger` performs no further redaction and records the canonical single-line `Diagnostic::format()` output.

Bootstrap formats its single failure diagnostic directly rather than capturing a
logger, avoiding an unnecessary logger module dependency and a GCC module
collision; the diagnostic encoding and stage prefixes are unchanged.
Bootstrap's private `cap.bootstrap.path_io` interface delegates canonicalization
and readable checks to this filesystem adapter in a separate implementation unit,
without importing the filesystem module into Boost-heavy bootstrap sources.

## Module-specific invariant
No dependency on other cap modules; stable error taxonomy; subprocess arguments never shell-expanded.

## Contract tests
Path normalization, atomic writes, process exit/signal handling, timeout and cancellation, error propagation.

## Acceptance
Every public API has documented pre/postconditions, unit and integration tests, benchmark for hot paths and explicit failure behavior before implementation is considered complete.

## Exhaustive planned type inventory
See [API.md](API.md). Every type there requires a public contract and verification before merge.

## v1.4 normative additions
Process cancellation must terminate descendant processes where supported; report unsupported capabilities. Isolation adapters must fail strict mode rather than silently downgrade; redact sensitive environment values.
