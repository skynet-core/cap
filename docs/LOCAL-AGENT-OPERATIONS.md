# Agent development operations

The [`ai-workflow` profile](../tools/agent-harness/ai-workflow.yaml) is the normative development policy. Commit code changes and push them to isolated topic branches; run CI in addition to local checks. The local operator still performs the runner responsibilities below. Keep private review records, credentials, and unsanitized evidence local. [Agent Coordination](AGENT-COORDINATION.md) defines lane ownership and contract freeze. The workflow runs outside the AMP native build engine.

## Roles and authority

| Role | Responsibilities |
|---|---|
| Sol | Receives user prompts, researches and plans work, revises task assignments and reviews locally verified milestones. A fresh read-only review session receives the sanitized bundle and has no repository tools, shell or release credentials. |
| Terra | Implements assigned changes, prepares integrated candidates and requests review. The runner owns protected refs and performs staging and promotion. |
| Luna | Implements assigned changes and authors tests and documentation within the task's allowed paths. |
| Qwen | Fallback worker for scoped implementation and tests only, when verified provider usage limits block both Terra and Luna. Does not integrate, review or audit. |
| Astra | Audits the final delivery candidate in a separate read-only session after local verification and Sol review. Returns a structured verdict with findings and has no access to repository write tools, shell or release credentials. |
| Local operator (runner responsibilities) | Validates model identity, task claims and evidence, runs local checks and enforces limits and approval gates. |
| Production approver | An authorized human approves a specific audited candidate and its artifacts for a named production target. |

Terra and Luna are the only regular authors of code and tests. Only Sol's intake and planning session receives the raw user prompt. The runner supplies workers with a scoped task and the reviewers with a sanitized evidence bundle. Keep provider credentials, required checks, policy and writes to protected refs outside agent control. The runner records resolved model and session identity for each worker submission and verdict. An unavailable or unverifiable model blocks the affected step unless the code-and-test fallback below applies. Sol and Astra have no fallback.

## Freeze the request and acceptance

At intake, the runner records the immutable request and freezes Sol's acceptance criteria and required checks in a protected record. Each task references the request and acceptance revisions and includes a stable ID, lane, dependencies, allowed paths, base commit, contract revision, applicable checks, risk, toolchain metadata and evidence locations. Workers can add tests that demonstrate the criteria. The runner protects the request, criteria, required check definitions, baseline fixtures and approved waivers from worker changes. Sol can revise task decomposition, ordering and repair assignments within the frozen scope. A user-authorized scope or acceptance change creates a new frozen revision for the project and requires fresh evidence for any affected work.

Each claim reserves one ready task and one worker slot atomically. The runner checks dependencies, ownership, contract revision and claim expiry before dispatch. Same-milestone dependencies consume the verified prerequisite commit on the staging branch. Cross-milestone dependencies consume promoted commits. Each task has one active owner and an isolated worktree. Use the contract review in [Agent Coordination](AGENT-COORDINATION.md) for shared interfaces.

## Admission and repair limits

- **Concurrency.** At most eight active workers across the seven ownership lanes, including any Qwen sessions, with at most three Terra and five Luna sessions. Sol and Astra are outside the worker pool. Enforce the total and role caps in the same transaction as the claim, with slot release on completion or cancellation.
- **Authoring fallback.** Dispatch the pinned local Qwen worker only after the runner records provider-confirmed quota or rate limits that prevent both Terra and Luna from taking work. Full worker slots, generic errors, missing bindings and exhausted project limits do not qualify. Qwen gets the same scoped task, worktree, checks and applicable limits; it receives neither the raw user prompt nor review authority. Recheck eligibility before each dispatch and prefer Terra or Luna again as soon as either can take the work.
- **Revisions.** Count the initial pass as round zero. A round starts when the runner dispatches a repair plan for a returned submission. Repairs in the same plan may run in parallel. Persist a separate counter for each stable task ID across reassignments, request revisions and restarts. Preserve earlier repair records; do not split or re-ID a task to reset its counter. When a task needs a fourth round, block further repair dispatch for that task and promotion of any milestone that requires it.

AMP sets no project-wide or per-call model spending cap and does not require pricing or spend records for dispatch. Provider quotas and billing still apply.

## Verification and promotion

1. The runner gives an admitted worker a clean worktree and bounded context. Run allowlisted checks with explicit argument vectors in a restricted disposable environment without secrets. Every author, including fallback Qwen, must pass the [pre-review static checks](QUALITY.md#pre-review-static-checks) before submitting code. Verify paths, contract revision and task checks before staging. Record exit status, environment, logs and repeatable performance evidence where applicable.
2. The local operator runs frozen local checks on the exact candidate commit. Record the SHA, toolchain, commands, exit statuses, local logs and required CI run URLs. Do not send the candidate to review until all applicable checks pass, including required CI for that SHA; rerun after candidate changes. A missing, pending, failed or unrun required check blocks review and promotion. Record capability exclusions and waivers under [the quality policy](QUALITY.md) when acceptance is frozen.
3. A fresh, read-only Sol session reviews the candidate SHA, frozen acceptance revision and criteria, scoped diff and sanitized local verification evidence. It returns `approve` or `reject` for that candidate, with findings on rejection. The runner verifies the reviewer, revision and SHA and requires `approve` before promoting an internal milestone or sending a final candidate to Astra.
4. Astra audits the final delivery candidate against the same frozen acceptance, exact-commit verification evidence and Sol verdict. Before final delivery, the runner requires a verified `approve` verdict for the candidate SHA and acceptance revision. For production, the verdict also binds the artifact digests and target.
5. For a production release, package publication or deployment, verify artifact provenance against the candidate and require recorded approval from an authorized human for the audited commit, artifact digests and target before executing the action. If the commit or target base changes, repeat verification and review on the new candidate. If the production artifacts or target change, reverify the release inputs, repeat the final audit and obtain a new human approval.
6. Return check failures or review findings to Sol for repair within the limits above. A stale or malformed verdict cannot authorize promotion. Record blocked work and the reason when no permitted worker or reviewer can proceed, a project limit would be exceeded, or required evidence is missing.

High-risk changes to the scheduler, sandbox, cache keys, resolver, toolchain security or public APIs require integration tests, fault injection where relevant and explicit coverage in Sol's milestone review. For external model calls, use the configured data-sharing policy to limit source and evidence to the task and strip credentials and unrelated files.

## Branch policy

- **Topic branches carry development code.** Commit and push every scoped code change to its assigned branch on `origin`. Keep credentials outside the worktree and put private local evidence in ignored `.amp/private/` or outside Git. Before a public push, review the staged paths and all newly reachable commits for secrets, generated outputs, and private evidence. Ignore rules do not protect tracked files or old commits. If history is unsafe, export only reviewed changes onto a clean public base; do not push or rewrite the original branch.
- **`develop` integrates promoted milestones; `main` carries releases.** The operator requires the **Linux build and test** status check on both protected branches. Only update protected refs after their approval gates pass. Routine topic pushes do not authorize release tags, package publication, or deployment.

## Runner acceptance and measurement

Before enabling unattended operation, exercise simultaneous claims at the 8/3/5 limits, worktree ownership violations, worker attempts to change frozen acceptance, independent task counters and exhaustion after three revisions on the same task, missing required checks, invalid model bindings and stale verdicts. Verify that a production candidate remains pending until human approval and that releasing a changed artifact requires a new audit and approval. The harness examples and benchmark [have separate setup instructions](HARNESS-CONFIGURATION.md).

Verify that provider limits on both primary workers admit Qwen only for code and tests, that a limit on just one model or full worker slots do not trigger it, and that new work returns to Terra or Luna on recovery. Verify that the fallback cannot bypass project limits or stand in for Sol or Astra.

Verify that a warning, lint or formatting failure, missing tool, incomplete clice diagnostic result or static-check evidence for a different commit prevents review dispatch. Check that a change touching multiple languages requires every applicable tool.

Measure independently accepted tasks per elapsed hour using verified tasks in promoted milestones. Record candidate passes separately before review. Capture test pass rate, revision count, request latency, token usage, spend and test/compile time. Preserve unavailable telemetry as unknown. Benchmark at progressively higher worker counts while holding the task set, models and required checks constant. The local vLLM benchmark tracks GPU metrics for its own experiments.
