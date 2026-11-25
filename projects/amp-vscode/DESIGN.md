# VS Code extension design

## Goals
Provide amp build/run/test/bench/profile tasks, package and target selection, managed toolchain status, test explorer integration, diagnostics, debugger launch configuration, and clangd compilation database selection. Do not implement an LSP server.

## Technology
VS Code extension host APIs require TypeScript/JavaScript; the extension is TypeScript while **all amp native production modules remain C++26**. No native VS Code binary dependency.

## Architecture
Extension activation → amp executable locator → workspace controller → task/test/debug providers → compdb controller → clangd integration. Use `amp` CLI with machine-readable JSON output when available; no private database reads.

## Trust and security
Respect VS Code Workspace Trust. Do not run amp commands automatically in untrusted workspaces. Never change clangd paths or global user settings without consent. Redact credentials from logs.

## Failure handling
No amp executable, offline managed toolchain, corrupt JSON, workspace reload, cancellation, multi-root conflicts, remote/WSL workspaces.

## Dependencies
amp-cli JSON protocol; amp-compdb active configuration; amp-test discovery; amp-toolchain status.
