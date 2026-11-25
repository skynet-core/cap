# AMP agent harness

[`ai-workflow.yaml`](ai-workflow.yaml) defines the [development workflow](../../docs/LOCAL-AGENT-OPERATIONS.md). OpenCode provides role sessions. The local operator records checks on the exact commit and applies the project limits and promotion gates without CI or an automated runner.

## OpenCode workflow profile

Use [`opencode-workflow.json`](opencode-workflow.json) with OpenCode 2.3.2. Authenticate the OpenAI provider in the isolated environment. Set the four variables to your account's verified model IDs, using just the part after `openai/`.

| Variable | Session profiles |
|---|---|
| `AMP_OPENCODE_SOL_MODEL_ID` | `amp-sol` for intake and planning, `amp-sol-review` for independent milestone review |
| `AMP_OPENCODE_TERRA_MODEL_ID` | `amp-terra` for implementation and integration |
| `AMP_OPENCODE_LUNA_MODEL_ID` | `amp-luna` for implementation, tests and documentation |
| `AMP_OPENCODE_ASTRA_MODEL_ID` | `amp-astra` for final audit |

From the repository root, prepare isolated OpenCode configuration, data, cache and state directories.

```bash
: "${AMP_OPENCODE_SOL_MODEL_ID:?Set the verified Sol model ID}"
: "${AMP_OPENCODE_TERRA_MODEL_ID:?Set the verified Terra model ID}"
: "${AMP_OPENCODE_LUNA_MODEL_ID:?Set the verified Luna model ID}"
: "${AMP_OPENCODE_ASTRA_MODEL_ID:?Set the verified Astra model ID}"
export AMP_OPENCODE_SOL_MODEL_ID AMP_OPENCODE_TERRA_MODEL_ID
export AMP_OPENCODE_LUNA_MODEL_ID AMP_OPENCODE_ASTRA_MODEL_ID

amp_opencode_dir=$(mktemp -d)
export XDG_CONFIG_HOME="$amp_opencode_dir/config"
export XDG_DATA_HOME="$amp_opencode_dir/data"
export XDG_CACHE_HOME="$amp_opencode_dir/cache"
export XDG_STATE_HOME="$amp_opencode_dir/state"
mkdir -p "$XDG_CONFIG_HOME/opencode"
cp tools/agent-harness/opencode-workflow.json "$XDG_CONFIG_HOME/opencode/opencode.json"
opencode debug agents --standalone
opencode models --standalone
```

Use `opencode --standalone` for intake after confirming `amp-sol` and the intended model. For a worker or review session, the runner uses `opencode run --standalone --agent <agent-id> --model <verified-provider/model>` and supplies only the appropriate task or evidence. Run workers inside the claimed task worktree. Launch Sol review and Astra audit in a fresh directory with only their sanitized evidence and their own session. The [harness configuration](../../docs/HARNESS-CONFIGURATION.md) defines what the runner must enforce.

## Local smoke tests

Use a working vLLM endpoint, a verified Qwen checkpoint with tool calling, OpenCode or Codex, and an isolated disposable worktree and container.

```bash
nvidia-smi --query-gpu=index,name,memory.total --format=csv
export AMP_MODEL=/absolute/path/to/verified/model
export AMP_TP=1   # 2 only for compatible two-GPU model parallelism
bash tools/agent-harness/serve-vllm.sh
# In another shell:
curl -fsS http://10.20.0.1:11535/v1/models
curl -fsS http://10.20.0.1:11535/metrics | head
export VLLM_API_KEY=dummy
```

For a vLLM smoke test, copy [`opencode.json`](opencode.json) into a disposable task worktree. It points at the live endpoint `http://10.20.0.1:11535/v1` serving `nvidia/Qwen3.8-27B-NVFP4`; the serve script above reproduces that deployment. Use `opencode models --standalone` to confirm model resolution, then run a small manual tool-call test with `amp-implement` or `amp-review`.

Codex (avoid changing your normal settings):

```bash
mkdir -p /tmp/amp-codex-home
cp tools/agent-harness/codex-config.toml /tmp/amp-codex-home/config.toml
export CODEX_HOME=/tmp/amp-codex-home
export VLLM_API_KEY=dummy
codex
```

**Codex caveat:** This config requires vLLM's Responses API, not merely Chat Completions. Verify `/v1/responses` and model tool calling; otherwise use the OpenCode sample or upgrade/configure a compatible serving stack. Run Codex inside an approved sandbox and keep approvals enabled.

The serving example caps the local vLLM endpoint at four sequences. For local benchmark comparisons, start with 1–4 concurrent worker sessions and measure throughput and GPU capacity before increasing the cap.

Experimental measurement scripts from the prior standalone benchmark are copied under `tools/amp-agent-benchmark/`; see its README. These scripts generate and execute model patches: only run them in an unprivileged disposable container/VM without secrets and with restricted networking.
