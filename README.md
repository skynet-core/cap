# CAP

**C++ Awesome Packager** is a native C/C++ package manager and build engine in development. The M1 preview builds single binaries, multi-binary packages, static libraries, and local multi-project workspaces. It does not yet fetch packages from a registry or implement `cap install`.

## Build from source

The verified preview configuration is Linux x86‑64 with GCC 16.2.0, GNU binutils, Git, GNU Make, and standard POSIX utilities. CAP uses C++20 named modules compiled in C++26 mode. The initial build downloads and builds the pinned Boost release from source.

```sh
git clone https://github.com/skynet-core/cap.git
cd cap
CC=$(command -v gcc-16 || command -v gcc)
CXX=$(command -v g++-16 || command -v g++)
export BOOST_VERSION=1.92.0
export BOOST_COMMIT=afdfa32505af73e3d208144b3f623f0096cb62b6
make CC="$CC" CXX="$CXX" bootstrap-probes
make CC="$CC" CXX="$CXX" boost-build
make CC="$CC" CXX="$CXX" bootstrap-dev
```

The last command prints `development cap: /absolute/path/to/cap`. Use that executable directly or copy it to a directory on your `PATH`. It is the development CLI, not the result of `make self-host`. See the [bootstrap guide](docs/BOOTSTRAP.md#local-development-cli) for preseeded and offline builds.

## Install from a release

Download the package for your distribution and `SHA256SUMS` from [GitHub Releases](https://github.com/skynet-core/cap/releases). Develop prereleases include `CHANGELOG.md` with manual checks; use them for testing, not as stable releases. The first package targets are Debian 13 and Fedora 44 on Linux x86‑64. Verify the checksum before installing or extracting:

```sh
sha256sum --check --ignore-missing SHA256SUMS
```

| Format | Install |
| --- | --- |
| Debian package (`.deb`) | `sudo apt install "./cap-cli_${VERSION}_amd64.deb"` |
| Fedora package (`.rpm`) | `sudo dnf install "./cap-cli-${VERSION}-1.x86_64.rpm"` |
| Linux archive (`.tar.gz`) | `mkdir -p "$HOME/.local" && tar -xzf "cap-cli-${VERSION}-linux-x86_64.tar.gz" -C "$HOME/.local"` |

Set `VERSION` to the version in the filenames you downloaded. Add `$HOME/.local/bin` to `PATH` if you used the archive. Verify the installation with `cap build --help`. Packaging does not include a compiler or archiver; you need them to build your own projects.

See the [release packaging workflow](docs/RELEASE-PACKAGING.md) for target platforms and install checks.

## Use CAP

Set `CC` and `CXX` to the absolute compiler paths as shown above. With `cap` on your `PATH`, try the shipped named-module example from the root of this repository:

```sh
cap build --cc "$CC" --cxx "$CXX" --ar "$(command -v ar)" \
  --manifest-path "$PWD/projects/cap-cli/tests/fixtures/native-standalone/cap.toml"
```

Run the executable at the printed `artifact:` path. The `compdb:` path is the generated `compile_commands.json` for editor tooling.

- To start your own project, copy the [standalone manifest](projects/cap-cli/tests/fixtures/native-standalone/cap.toml) and change `name`, `entry` and `sources` to match your project. Give `--manifest-path` the path to that manifest.
- Add more `[[bin]]` entries for a multi-binary package; use `--bin NAME` to build one. See the [multi-binary example](projects/cap-cli/tests/fixtures/native-multibin/cap.toml).
- Define `[lib]` with `type = "static"` for a library; use `--lib` to select it. The [library example](projects/cap-cli/tests/fixtures/native-library/README.md) shows how a separate program links the archive.
- Point `--manifest-path` at a [workspace manifest](projects/cap-compdb/fixtures/workspace/cap.toml) and add `--workspace` to build all members or `--package NAME` to select one. Workspaces support local static-library dependencies, including named-module imports across those dependencies on the tested GCC configuration.

Upgrading from the AMP preview? [Migrate manifests and local settings](docs/ADR/0023-cap-product-rename.md#migration) before running `cap`.

## Frozen decisions

- No CMake, Meson, Autotools, Ninja, Bazel or other build backend in CAP core; GNU Make is bootstrap-only for CAP. Manifest dependencies may use independently installed [library import plugins](docs/ADR/0010-manifest-library-import-plugins.md) to prepare third-party libraries separately from native builds (planned).
- Managed downloadable toolchains; versioned catalog and immutable verified installs. Apple and Microsoft SDK licensing must be respected.
- TOML manifests and lockfiles; arbitrary project directory layouts; explicit executable entry points and source sets.
- `*_test.cpp` translation units (not inline tests); Go-style `_linux`, `_windows`, `_darwin`, `_freebsd`, and architecture suffixes.
- Minimize preprocessor use; optional features may map to preprocessor definitions. Existing C/C++ ecosystem and header guards are not forbidden.
- Workspace-wide multi-package graph and lockfile; ignored `.cap/config.local.toml` for local overrides.
- Cargo-style external executable plugins, optional structured protocol later; never load arbitrary C++ ABI plugins.
- SOLID, C++26, tests, coverage, benchmarks, profiling, sanitizer checks and security review are mandatory.
- Bootstrap uses a system C++26 compiler, Git, GNU Make, and Boost built from source using upstream bootstrap.sh/b2; cap builds never delegate to external build systems.

## Reading order

1. `docs/ARCHITECTURE.md`
2. `docs/FEATURE-CATALOG.md`
3. `docs/SCHEMAS.md`
4. `docs/BOOTSTRAP.md`
5. `docs/QUALITY.md`
6. `docs/ROADMAP.md`
7. Each `projects/<module>/` directory (DESIGN, GOALS, CONTRACTS, ROADMAP, TASKS, AGENTS)

There are 18 subsystem specifications. Interfaces and TOML examples are **proposals requiring conformance tests**; toolchain versions are not claims of availability.

Contributors: see the [C++ source documentation rules and local reference generation](docs/API-DESIGN-RULES.md#source-documentation).

CAP's own code and documentation are licensed under [Apache-2.0](LICENSE);
see [NOTICE](NOTICE) for attribution. Third-party components retain their own
licenses.

## Editor / LSP compatibility
`cap-compdb` is the 19th module. It exports exact native compile actions to clangd-compatible `compile_commands.json`, with explicit configuration activation. See `projects/cap-compdb/`.

## v1.2 additions
- 20 planned modules including `cap-vscode` (TypeScript extension).
- Every module has an `API.md` enumerating planned classes/interfaces, responsibilities, contracts and quality gates.
- See `docs/COMPONENT-INVENTORY.md`, `docs/API-DESIGN-RULES.md`, and `docs/VSCODE-EXTENSION.md`.
- This is a design specification, **not a complete implementation or finalized method-by-method API**.

## v1.3 bootstrap and Boost
`Makefile` includes `boost-fetch`, `boost-bootstrap`, `boost-build`, and `bootstrap`. The v1.3 policy used latest stable Boost by default; v1.4 supersedes this with explicit pinned version and verified commit for every Boost build. See `docs/BOOST-POLICY.md`. The M0 bootstrap produces the stage-1 status driver; it is distinct from `bootstrap-dev`.

## v1.4 engineering revision (specification only)
This bundle adds normative scheduler, isolation, agent coordination and observability contracts, a staged release plan and future remote execution design. It strengthens bootstrap reproducibility: **Boost version and full commit must be explicitly provided**, rather than silently resolving `latest`. Obtain and authenticate a Boost pin before running Boost targets. Read `docs/RELEASE-PLAN-v1.4.md`, `docs/SCHEDULER.md`, `docs/BUILD-ISOLATION.md`, `docs/AGENT-COORDINATION.md`, `docs/BUILD-OBSERVABILITY.md`, and `docs/REMOTE-EXECUTION.md`. The original twenty-module scope remains intact; `cap-worker` and `cap-remote` are future proposals only.

## Agent development workflow

The development workflow assigns intake, planning and independent milestone review to Sol. Up to three Terra and five Luna workers share the seven ownership lanes and work against frozen acceptance criteria. Qwen may author code and tests only when verified provider usage limits block both primary models. Astra audits final delivery, and production requires human approval. Commit and push code changes to `origin` topic branches. The [Linux build and test workflow](.github/workflows/ci.yml) checks pushes and pull requests in addition to [local verification](docs/QUALITY.md#merge-and-release-gates). See the [agent workflow](docs/AGENT-WORKFLOW.md) and [harness configuration](docs/HARNESS-CONFIGURATION.md).
