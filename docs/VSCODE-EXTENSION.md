# VS Code integration specification

The official VS Code extension is `cap-vscode`, implemented in TypeScript and backed by the C++26 cap CLI. It integrates with existing clangd; cap does not implement an LSP server.

## Features
- Detect cap executable, toolchain and workspace; respect Workspace Trust.
- Build/run/test/bench/profile tasks and package/target/profile selection.
- Test Explorer and debugging, diagnostic parsing, active toolchain status.
- Generate and select `compile_commands.json` via cap-compdb and prompt before updating clangd configuration.
- Multi-root workspaces, remote/WSL awareness, cancellation, output channels.

## Roadmap
V0 command/tasks → V1 selection → V2 compdb/clangd → V3 tests/debug/profile → V4 release and compatibility.

## Dependencies
cap-cli stable JSON output, cap-compdb active configuration, cap-test test list, cap-toolchain info.
