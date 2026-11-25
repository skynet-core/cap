# VS Code integration specification

The official VS Code extension is `amp-vscode`, implemented in TypeScript and backed by the C++26 amp CLI. It integrates with existing clangd; amp does not implement an LSP server.

## Features
- Detect amp executable, toolchain and workspace; respect Workspace Trust.
- Build/run/test/bench/profile tasks and package/target/profile selection.
- Test Explorer and debugging, diagnostic parsing, active toolchain status.
- Generate and select `compile_commands.json` via amp-compdb and prompt before updating clangd configuration.
- Multi-root workspaces, remote/WSL awareness, cancellation, output channels.

## Roadmap
V0 command/tasks → V1 selection → V2 compdb/clangd → V3 tests/debug/profile → V4 release and compatibility.

## Dependencies
amp-cli stable JSON output, amp-compdb active configuration, amp-test test list, amp-toolchain info.
