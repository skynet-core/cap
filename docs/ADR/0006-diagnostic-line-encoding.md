# ADR-0006 Diagnostic line encoding and logger redaction boundary

Status: accepted.

## Context

Diagnostic fields are caller-controlled strings. Embedded ASCII control bytes, especially LF and CR, could violate the documented one-line format. A generic logger cannot reliably identify secrets in arbitrary strings.

## Decision

Escape backslash and ASCII control bytes in file, message, and remediation values in the canonical diagnostic format. Use conventional short escapes for NUL, alert, backspace, tab, LF, vertical tab, form feed, and CR; encode remaining C0 controls and DEL as uppercase `\xHH`. Preserve all other bytes unchanged, including UTF-8. This guarantees only that formatted output contains no embedded LF/CR physical line breaks; it is not a general text or security parser.

Callers redact sensitive fields before emitting diagnostics. `ILogger` has no secret-detection guarantee; `CaptureLogger` records the canonical formatted line without further redaction.

## Consequences

Ordinary diagnostic text remains unchanged. Control-containing fields have a stable escaped representation. Golden fixtures pin the encoding. Logger callers own redaction because only they know the semantics of their strings.
