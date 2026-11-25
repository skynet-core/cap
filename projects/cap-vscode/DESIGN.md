# VS Code extension design

## Goals
Provide cap build/run/test/bench/profile tasks, package and target selection, managed toolchain status, test explorer integration, diagnostics, debugger launch configuration, and clangd compilation database selection. Do not implement an LSP server.

## Technology
VS Code extension host APIs require TypeScript/JavaScript; the extension is TypeScript while **all cap native production modules remain C++26**. No native VS Code binary dependency.

## Architecture
Extension activation → cap executable locator → workspace controller → task/test/debug providers → compdb controller → clangd integration. Use `cap` CLI with machine-readable JSON output when available; no private database reads.

## Trust and security
Respect VS Code Workspace Trust. Do not run cap commands automatically in untrusted workspaces. Never change clangd paths or global user settings without consent. Redact credentials from logs.

## Failure handling
No cap executable, offline managed toolchain, corrupt JSON, workspace reload, cancellation, multi-root conflicts, remote/WSL workspaces.

## Dependencies
cap-cli JSON protocol; cap-compdb active configuration; cap-test discovery; cap-toolchain status.
