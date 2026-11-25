# ADR-0009 — Lua build scripts

Status: accepted requirement; implementation pending.

A package can opt in to a Cargo `build.rs`-style build step by declaring a host-run Lua script in its manifest. The default convention is `build.lua`, but a file with that name is never executed merely because it exists.

```toml
[build-script]
path = "build.lua"
inputs = ["schema/messages.toml"]
outputs = ["generated/messages.hpp"]
```

`path` and `inputs` are package-relative; `outputs` are relative to a dedicated build output directory for the current configuration. Paths must stay inside their respective roots, including after symlink resolution. The script can read declared inputs, write declared outputs, and read CAP-provided host, target, profile and feature information. Targets opt in to consuming generated sources or headers. Use generated configuration headers for compile-time settings and existing manifest fields for system linkage; arbitrary compiler flags, native Lua extensions and shell execution are not part of the initial interface.

Run Lua with a pinned host interpreter and an CAP API limited to those operations. No ambient environment, credentials, subprocesses or network. Enforce M3 isolation and bound time and resources; an unsupported required sandbox fails closed. Require explicit authorization from the consuming workspace before running any dependency's script. Manifest discovery, resolution and no-build compile database export never run scripts.

Model the script as a native build action. Fingerprint the interpreter, script, declared input contents, output mapping, host and target contexts, profile, features and isolation policy. Publish verified outputs only after success. Generated files must be ready before dependent include and module scanning; freeze the resulting compile actions before execution. Script failures, missing outputs and attempted undeclared access produce structured diagnostics and stop dependent actions.

Verify a local fixture generating a header consumed by a native C++ target; a no-op rebuild reuses the result, changing a declared input regenerates it, and cross builds run Lua on the host with the target context. Negative fixtures cover unauthorized dependency scripts, path escapes, undeclared reads and writes, network and process attempts, and failed or timed-out scripts publishing no outputs.
