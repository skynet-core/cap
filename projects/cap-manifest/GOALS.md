# Goals — cap-manifest

## Mission
Manifest parsing and source selection.

## In scope
Versioned TOML schema; target entries; source globs; exclusion; platform suffix grammar; profiles; options; features.

## Non-goals
No external build-system delegation; no undocumented cross-module behavior; no unverified production readiness. Ownership is limited to this subsystem; shared interfaces require review.

## Success criteria
1. Public contract documented with success, error and cancellation semantics.
2. Unit, integration and negative tests cover: Windows/Linux/Darwin/FreeBSD and architecture suffixes, test ordering, unknown suffix, path traversal, profile cycles.
3. Meets root QUALITY.md coverage and sanitizer requirements; benchmark and profiling evidence for hot paths.
4. Deterministic behavior on pinned toolchain with explicit platform limitations.
