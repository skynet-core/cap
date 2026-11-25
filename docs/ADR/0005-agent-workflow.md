# ADR-0005 Role-based agent development workflow

Status: accepted for the workflow specification. Runner implementation is pending.

## Context

The v1.5 development workflow specified a local Qwen coordinator and three Qwen workers, with Sol reviewing milestones. The requested `ai-workflow` profile sets role-specific permissions, shared limits and a human production gate. AMP has bootstrap and core code, local agent smoke-test profiles and a standalone benchmark. It has no unattended runner.

## Decision

Adopt [the `ai-workflow` profile](../../tools/agent-harness/ai-workflow.yaml). Sol owns planning and reviews milestones in an independent session. Terra and Luna author changes within existing module ownership and contract rules. Astra audits final delivery candidates. An authorized human approves production promotion after exact-commit verification, Sol review and Astra audit. Internal milestone promotion follows local and CI verification and Sol review. The [branch policy](../LOCAL-AGENT-OPERATIONS.md#branch-policy) requires remote topic commits and CI without enabling unattended agent orchestration.

Local Qwen is the only authoring fallback, and only for code and tests when verified provider usage limits block both Terra and Luna. It uses the same project limits and cannot replace Sol or Astra.

The runner owns the frozen request and acceptance records, worker admission, revision limits, evidence validation and promotion. The profile sets one revision counter for the project, shared across roles and request revisions. This supersedes the original spending cap: neither project-wide nor per-call model spend is an admission gate. [Local Agent Operations](../LOCAL-AGENT-OPERATIONS.md) defines these controls and their acceptance cases.

## Consequences

The profile supersedes the v1.5 Qwen orchestration policy. The local vLLM examples and experimental benchmark remain available for their existing use. The local operator carries out the runner's responsibilities until unattended automation is enabled. Verified provider bindings remain required. The workflow has no AMP runtime dependencies.
