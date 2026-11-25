# ADR-0008 — System libraries and standalone installation

Status: accepted requirement; implementation pending.

## System libraries

`[lib]` and `[[bin]]` accept an ordered `system-libraries` array of linker names, e.g. `["z", "ssl", "crypto"]`. These are external libraries supplied by the selected target toolchain, SDK or sysroot, not CAP packages. The manifest rejects paths, flags and empty or invalid names. The build planner resolves the declared inputs for the **target**, never falls back to host libraries for a cross-target build, and reports unresolved inputs. Preserve declared order within each list. A static library's system requirements propagate to final link consumers. Include resolved inputs in the link action and its fingerprint; no implicit package-manager, `pkg-config`, or shell invocation.

## Installation

`cap install --standalone` installs selected executables into `<root>/bin` and their runtime shared libraries into `<root>/lib`. `--root DIR` overrides `CAP_INSTALL_ROOT`, which overrides `$HOME/.local`. Resolve relative overrides from the invocation directory. An explicitly empty override or an unavailable default is an error. Installation does not change the manifest or lockfile and does not request elevation.

Inspect executable and library metadata without executing the artifacts or invoking `ldd`. Resolve the transitive shared-library dependency graph for the selected target, including system-linked dependencies, preserving loader names and necessary in-bundle aliases. A binary may declare `runtime-libraries` by exact shared-library filename for libraries it opens dynamically; include those libraries and their transitive dependencies too. Runtime-computed filenames and absolute application paths cannot be discovered or made relocatable automatically. The target OS runtime, such as its loader and ABI-provided libc, is a documented platform prerequisite, not a reason to skip other libraries just because they were found under a system path.

Use platform-relative loader paths for binaries **and** bundled libraries, including transitive dependencies. A moved bundle must run without build-tree paths or `LD_LIBRARY_PATH`/`DYLD_LIBRARY_PATH`. Keep bundle aliases relative and inside `lib`. Reject unresolved or incompatible dependencies, conflicting loader names, known redistribution restrictions, and escaping destinations before publishing. Stage and validate the install before replacing any CAP-owned files; do not overwrite unrelated files or report a partial install as successful. Garbage collection and system-wide package registration are out of scope.

The layout is shared by Unix targets. Implement Linux/ELF first and only claim other Unix binary formats after testing their loader rules. Unsupported targets fail explicitly. Standalone means relocatable on a compatible target OS and ABI, not independent of the operating system.

## Verification

- Link a target against a toolchain-supplied library; test declaration ordering, propagation from a static library, invalid names, missing libraries, and no fallback from target to host.
- Install an executable with a system-supplied shared library, a transitive dependency and a declared runtime-loaded library. Move the prefix, remove the original fixture search paths, clear loader overrides, and run the installed executable.
- Test CLI/environment/default root precedence and confirm missing libraries, conflicting loader names, destination escapes and existing unrelated files fail without publishing a partial install.
