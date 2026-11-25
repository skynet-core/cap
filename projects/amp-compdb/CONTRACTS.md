# Contracts — amp-compdb

## Dependencies
Required: `amp-core` (paths, errors, atomic filesystem), `amp-build` (read-only planned compile action projection). Optional: `amp-workspace` for member metadata; `amp-cli` invokes this module, never reverse.

## M1 frozen exporter slice
The `amp.compdb` named module exports `amp::compdb::to_json` over read-only
`amp::build::Action` values. It emits only compile actions using exactly
`directory`, `inputs.front()`, ordered `arguments`, and `outputs.front()`.
Actions retain input order; argv is never reordered. Missing primary values,
empty argv, invalid argv[0], NULs, or invalid UTF-8 in serialized values return
`invalid_input`, not partial JSON. Empty argv elements after argv[0] are
preserved. Output is standard JSON with exactly
`directory`, `file`, `arguments`, and `output`. This slice performs no file IO,
activation, plan selection, or argument regeneration.

## Conceptual C++26 interfaces
```cpp
namespace amp::compdb {
struct ConfigurationId { std::string digest; };
struct Request {
    ConfigurationId configuration;
    std::filesystem::path workspace_root;
    bool include_tests = false;
    std::optional<std::string> package;
};
struct Entry {
    std::filesystem::path directory;
    std::filesystem::path file;
    std::vector<std::string> arguments;
    std::optional<std::filesystem::path> output;
};
struct ExportResult {
    std::filesystem::path database_path;
    std::size_t entry_count;
    std::vector<Diagnostic> warnings;
};
class CompileActionView {
public:
    virtual ~CompileActionView() = default;
    virtual auto compile_commands(const ConfigurationId&)
      const -> Result<std::vector<Entry>> = 0;
};
class Exporter {
public:
    virtual ~Exporter() = default;
    virtual auto generate(const Request&, const CompileActionView&)
      -> Result<ExportResult> = 0;
    virtual auto activate(const ConfigurationId&, const std::filesystem::path& workspace)
      -> Result<void> = 0;
};
}
```
Illustrative API; `Result` and `Diagnostic` are shared core types. Implementations must preserve action argument order, including response-file semantics (materialize or preserve valid response files).

## CLI contract
`amp compdb generate [--workspace] [--package NAME] [--target TRIPLE] [--profile NAME] [--features LIST] [--include-tests]`; `amp compdb activate --configuration ID`; `amp compdb list`; `amp compdb check`. `amp build/check/test` generate/update the matching configuration's database by default; only explicit activation updates the workspace-root active file. `amp compdb check` validates freshness, path existence where required, and schema, not compilation success.

## JSON example
```json
[{"directory":"/work/app","file":"/work/app/src/main.cpp","arguments":["/home/user/.amp/toolchains/llvm/bin/clang++","-std=c++26","-c","/work/app/src/main.cpp","-o","/work/app/.amp/build/main.o"],"output":"/work/app/.amp/build/main.o"}]
```

## M0 reference fixture

The local fixture is a proposed-v1 workspace containing a static named-module
library and an app with two executables. Its Linux, Windows, and `_test.cpp`
sources exercise selection; a source directory contains a space. The reference
JSON uses `@ROOT@` and `@BUILD@` placeholders and ordered `arguments`, not a
shell command. The local check substitutes absolute roots, rejects any changed
argument or order, and verifies the selected Linux test-enabled translation
units against the commands it actually executes. It compiles the module before
its importers, archives the library, and runs both executables and the test.
This is fixture evidence only: it neither implements an exporter nor verifies an
amp-build plan or freezes a cross-module interface. Windows is selection-only.
The reference run is pinned to the documented local GCC and archive drivers.

## Exhaustive planned type inventory
See [API.md](API.md). Every type there requires a public contract and verification before merge.
