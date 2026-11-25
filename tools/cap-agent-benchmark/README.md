# CAP local coding-agent benchmark

This is a **standalone experimental harness**, not an CAP runtime feature. It follows the v1.4 `docs/AGENT-COORDINATION.md` principles: scoped changes, isolated Git worktrees, contract revision metadata, test evidence and independent review. It does not implement CAP's proposed task-claim service, dependency scheduler, or production security isolation.

## Prerequisites

Linux, Python 3.10+, Git, NVIDIA drivers and `nvidia-smi`, a running vLLM OpenAI-compatible endpoint, and a **trusted disposable repository**. Python dependencies: none. The model must emit a valid unified `git diff` patch. Verify the exact model checkpoint, supported context and GPU topology before configuring vLLM.

## Serving vLLM

```bash
export CAP_MODEL=/absolute/path/to/your/verified/qwen-checkpoint
# One 96 GB GPU; use tensor-parallel-size 2 for compatible 2x48 GB setups.
vllm serve "$CAP_MODEL" --served-model-name nvidia/Qwen3.8-27B-NVFP4 \
  --host 10.20.0.1 --port 11535 \
  --tensor-parallel-size 1 --max-model-len 32768 \
  --max-num-seqs 4 --gpu-memory-utilization 0.85 \
  --enable-prefix-caching
curl -fsS http://10.20.0.1:11535/v1/models
curl -fsS http://10.20.0.1:11535/metrics | head
```

Start at **3 agent workflows**, **32,768 max server tokens**, **4 concurrent sequences**. The harness requests up to 4,096 output tokens per attempt; ensure prompts plus output fit the server limit. On multi-GPU systems, compare tensor parallelism against independent replicas where the model fits. Do not assume 96 GB aggregated across GPUs behaves like one 96 GB device.

## Safe smoke test

```bash
bash scripts/make_fixture.sh /tmp/cap-bench-fixture
python3 scripts/benchmark.py --repo /tmp/cap-bench-fixture \
  --tasks tasks/example.json --workers 3 --model nvidia/Qwen3.8-27B-NVFP4 \
  --endpoint http://10.20.0.1:11535/v1
```

The example contains only **one task**, so it checks the pipeline but **does not measure three-way concurrency**. For a real 3-agent benchmark, prepare at least 12 independent, reviewed CAP tasks, each with its own allowed paths and test commands. Avoid overlapping file ownership, or the worktrees may produce misleading results.

## Task format

A JSON array. Each task needs `id`, `prompt`, `allowed_paths` (Git path patterns), and `checks` (arrays of argv, **never shell strings**). Optional `lane`, `contract_revision`, `context_files`. Each task starts at the same `--revision` baseline. No task dependencies are enforced by this harness; use independent tasks only. Every task gets a disposable Git worktree. Failed attempts are reset; patches and logs remain in the results directory.

**Security:** This is NOT a sandbox. `checks` execute arbitrary programs from trusted manifests and patches may modify code executed by those checks. Run in an isolated, unprivileged container or VM with no secrets, no host mounts beyond the disposable repository, and restricted network access. Do not expose the vLLM endpoint publicly. `git apply` checks and path restrictions are not a security boundary.

## Metrics

Results are saved to `results/<timestamp>/`:

- `summary.json`: tasks, test-pass rate, candidate passes/hour, independently accepted tasks/hour, request mean/p95 latency, GPU utilization, VRAM usage, KV cache utilization and token totals.
- `tasks.json`: attempts, inference errors, apply failures, checks, reviewer decisions.
- `telemetry.jsonl`: time-series GPU readings from `nvidia-smi` and vLLM Prometheus metrics (`/metrics`).
- `patches/`: generated patches for review.

**KV cache:** vLLM metrics names differ by version. The harness recognizes `vllm:kv_cache_usage_perc` and legacy `vllm:gpu_cache_usage_perc`; if neither is exposed, KV metrics remain `null`, not zero. GPU utilization is sampled, not a profiler. Request latency is full non-streaming API latency, **not time to first token**. For TTFT, add streaming instrumentation or inspect vLLM histogram metrics. A missing `nvidia-smi` also produces `null` GPU metrics.

A task passes tests only if **all declared checks run and exit zero**. It is accepted only after an independent reviewer approves it. The harness intentionally reports accepted tasks/hour as zero before approval; candidate passes/hour is available immediately.

To apply review decisions, create `approvals.json`:

```json
{"CAP-CORE-EXAMPLE-001": {"approved": true, "reviewer": "human-reviewer", "note": "Patch inspected"}}
```

Then run:

```bash
python3 scripts/review.py results/<run-id> --approvals approvals.json
```

## Benchmark plan

1. **Sanity:** Run the fixture and confirm inference, patch application, tests, telemetry and review process. Do not count this as an CAP result.
2. **Baseline:** Select 12–20 independent real CAP tasks spanning core, manifest, toolchain and build scaffolding, with frozen contracts, tests and bounded allowed paths. Use the same task set and same revision in each configuration.
3. **Warmup:** Run 2–3 unscored tasks after loading the model. Allow GPU temperatures and vLLM cache behavior to stabilize.
4. **Matrix:** Run `--workers 1`, `2`, `3`, `4` with `--max-model-len 32768` and `--max-num-seqs 4` on the server. Repeat each configuration three times with task order shuffled or balanced; keep other settings constant.
5. **Record:** Median accepted tasks/hour, candidate passes/hour, test pass rate, p95 request latency, mean/peak KV cache utilization, mean GPU utilization and peak VRAM. Also note compiler/test CPU saturation, failures, and patch review time.
6. **Select:** Prefer the highest accepted tasks/hour configuration whose test pass rate is not materially worse than the 1-agent baseline, whose p95 latency remains manageable and whose VRAM headroom is safe. If 4 workers improve only tokens/s but reduce acceptance, keep 3 or fewer.
7. **Context experiment:** Repeat the winner at 16K and 32K using the same tasks. Test 64K only if model support and memory profiling permit; restart vLLM with the corresponding max-model-len.

**Interpretation:** This harness measures total request latency and candidate patch outcomes. It does not automatically compute exact TTFT, per-token decode latency, scheduler queue time, CI CPU usage, or true task dependency throughput. The vLLM Prometheus data is preserved for more detailed external analysis.

## CAP-specific acceptance criteria

Follow `docs/AGENT-COORDINATION.md`: changed-file ownership, contract checks, targeted unit/contract tests, security/format/static analysis and sanitizer coverage where applicable. Do not count unrun checks as passing. The example fixture is intentionally minimal; production tasks must define their own relevant checks and reviewers.
