# Boost dependency and bootstrap policy (normative, v1.4)

## Explicit pin required (supersedes v1.3 latest default)
Every Boost fetch/build requires `BOOST_VERSION=<exact-stable-version>` and `BOOST_COMMIT=<full-40-hex-verified-commit>`. No moving `latest` default and no unverified fallback. **This specification does not assert or ship an independently verified Boost revision.** An authorized maintainer must select an upstream release, verify its provenance and record the tag/commit in local build evidence and applicable release metadata. A separately invoked upgrade workflow may propose a newer stable release and run compatibility tests, but must not silently update pinned inputs.

## Mandatory stages
Root GNU Make targets: `boost-fetch` clones the Boost superproject with submodules (or verifies a preseeded checkout), `boost-bootstrap` runs upstream `bootstrap.sh`, and `boost-build` runs `b2` to compile/install selected components. GNU Make and Boost.Build are allowed for bootstrap/Boost only, never as backends for AMP package builds. `BOOST_JOBS` controls Boost build concurrency; `threading=multi` selects Boost's multithreaded variant and is not an AMP scheduler setting.

## Trust and reproducibility
Verify tag/commit provenance independently of the download (trusted signed tag or approved trusted release metadata); a recorded hash alone is not authentication. Verify checkout HEAD against the required commit on every invocation, including preseeded/offline checkouts. Record submodule SHAs, compiler/stdlib identity, Boost component list, build flags and install digest. Pin source inputs in local and release builds. Cached build products require identity verification before reuse. Do not silently reuse an existing checkout at the wrong revision. Network fetch must be explicit; preseeded verified checkouts support offline bootstrap where dependencies are available.

Source reuse checks tracked files and index flags recursively before executing Boost code, as specified in `BOOTSTRAP.md`. They deliberately allow ignored and untracked generated artifacts for normal reruns; those artifacts and other untracked inputs must be trusted by the operator. Installed headers and libraries are checked against the trusted build-root inventory before stage 0 and before reuse; this local record does not authenticate the pin or establish independent provenance. See `BOOTSTRAP.md` for scope and concurrency requirements.

## Components and platform caveats
Initial compiled components: Filesystem, Program_options, JSON. Header dependencies such as Asio, Beast, Graph and Process require compatibility validation. The provided Makefile assumes POSIX shell; native Windows bootstrap remains a separate work item. C++26 flag acceptance does not prove reflection support. SDK licenses and ABI/toolchain compatibility remain binding.

## Example
```sh
make prerequisites
make boost BOOST_VERSION=<verified-version> BOOST_COMMIT=<verified-40-hex-sha> BOOST_JOBS=8
make bootstrap BOOST_VERSION=<verified-version> BOOST_COMMIT=<verified-40-hex-sha>
```

`bootstrap/main.cpp` is present; its presence does not establish that a complete bootstrap has been validated for a particular Boost release.
