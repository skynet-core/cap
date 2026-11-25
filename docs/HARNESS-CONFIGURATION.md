# Agent harness configuration

## Workflow profile

[`ai-workflow.yaml`](../tools/agent-harness/ai-workflow.yaml) defines role permissions, prompt access and project limits for the planned runner. The `model` values are policy labels. Before use, the runner must bind each label to a verified provider model ID and session, verify allowed data sharing, and fail closed when a binding is unavailable. A client-side agent prompt alone cannot enforce those controls.

Keep the effective agent configuration, private acceptance records, review identities and credentials in protected local state. The local operator is responsible for claims, path ownership, workspace isolation, worker and revision limits, exact-commit checks and verdicts, and production approval. Unattended agent orchestration remains optional; Git topic branches and CI are required independently of it. See [Local Agent Operations](LOCAL-AGENT-OPERATIONS.md) for the controls and acceptance cases.

## OpenCode workflow sessions

Use [`opencode-workflow.json`](../tools/agent-harness/opencode-workflow.json) with OpenCode 2.3.2. The default agent, `amp-sol`, handles intake and planning. Workers `amp-terra` and `amp-luna` and reviewers `amp-sol-review` and `amp-astra` are hidden from interactive selection and launched by explicit agent ID in fresh sessions. Only workers receive edit permissions. Their shell commands require approval. The milestone review and audit profiles deny all tools and use the supplied evidence bundle.

The launcher must verify that each configured OpenAI model ID exists and resolves to the matching policy model before sending prompts. Store the workflow profile in a runner-controlled OpenCode configuration outside the worker worktree. Start isolated instances with `--standalone` and pass both `--agent` and the verified `--model` on scripted runs. The runner must send the raw user prompt only to `amp-sol`, derived task context to workers, and sanitized evidence to each reviewer in a clean directory. It owns worker scheduling, revision tracking, and promotion. See the [harness README](../tools/agent-harness/README.md) and [OpenCode agent configuration](https://opencode.ai/docs/agents/).

## Local Qwen fallback and smoke tests

The vLLM OpenCode and Codex profiles exercise connectivity and tool calls against local Qwen. They do not authorize fallback work: a future runner must verify the model binding and enforce the [authoring fallback policy](LOCAL-AGENT-OPERATIONS.md#admission-and-repair-limits) before dispatch. The OpenAI workflow session profile does not launch Qwen. Verify the checkpoint, tokenizer, vLLM version and supported tool-call parser locally. The 32,768-token cap includes prompt and generated tokens, so leave output headroom. See the [local harness README](../tools/agent-harness/README.md) for commands.

### OpenCode
Copy `tools/agent-harness/opencode.json` into a disposable task worktree. It declares an `@ai-sdk/openai-compatible` provider pointing to vLLM `/v1` and separate implementation and review agents. Review mode disables write and edit tools. Set `enabled_providers`, `model` and each agent's model to match the provider and the model ID reported by your vLLM endpoint. Keep credentials in a local environment variable. Check `opencode --version`, `opencode models` and tool calls against your installed version.

### Codex CLI
Copy `tools/agent-harness/codex-config.toml` into an isolated Codex home as `config.toml` (see README). It defines a custom `vllm` provider with `wire_api = "responses"`; verify your vLLM supports `/v1/responses` and your checkpoint supports required tool calls. If Responses support is missing, this sample is **not** plug-and-play; update vLLM/Codex or choose OpenCode's Chat Completions path. Codex's tool permissions and sandbox mode must be checked for your installed CLI version. Do not use full-access or bypass approvals to make an incompatible setup appear functional.

### Serving example
`tools/agent-harness/serve-vllm.sh` prints/executes a baseline command with `AMP_MODEL` required and `AMP_TP` configurable. Bind to localhost. Check `nvidia-smi` and vLLM `/v1/models` and `/metrics` before running the benchmark. A 96 GB total across devices does not guarantee a 96 GB contiguous KV cache.

The [standalone benchmark](../tools/amp-agent-benchmark/README.md) measures experiments with model-generated patches in a disposable environment.
