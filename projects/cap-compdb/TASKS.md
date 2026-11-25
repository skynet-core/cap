# Agent task backlog — cap-compdb

- [ ] Define compile-action projection in cap-build CONTRACTS without exposing scheduler internals.
- [ ] Define canonical configuration fingerprint and stable output naming.
- [ ] Implement JSON projection preserving exact argument vector and cwd.
- [ ] Implement deterministic ordering, atomic write and unchanged-file optimization.
- [ ] Implement `cap compdb generate/list/activate/check` CLI integration.
- [ ] Wire build/check/test to configuration export, without changing active database implicitly.
- [ ] Test source eligibility across OS/architecture suffixes and `_test.cpp` exclusion/inclusion.
- [ ] Test source shared by multiple targets; warn about clangd ambiguity.
- [ ] Test paths with spaces/Unicode, Windows escaping, response files, generated headers and modules.
- [ ] Test workspace with managed toolchain and local dependency override.
- [ ] Add clangd indexing smoke tests, malformed-plan negative tests, concurrency tests.
- [ ] Publish coverage report, sanitizer results, benchmark baseline and profiling report.

Every task needs a focused test, acceptance evidence and documentation update. Never add an independent compiler-flag resolver.
