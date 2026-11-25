# Agent workflow

1. Read root AGENTS.md and the assigned module's AGENTS, DESIGN, CONTRACTS and TASKS documents.
2. Use the assigned task's frozen dependencies, acceptance criteria and approved contracts.
3. Add failing `*_test.cpp` and integration fixtures; implement with C++26 and SOLID; use dependency injection for IO.
4. Run targeted tests, sanitizer matrix, coverage and benchmarks as applicable; attach measured output and environment metadata.
5. Update contract docs, decisions and task state. Request integration review for public interfaces.
6. Do not claim success without execution evidence; mark blocked features and unsupported toolchains explicitly.

Task template: Goal / Inputs / Outputs / Dependencies / Design constraints / Test cases / Coverage / Benchmark and profiler baseline / Security checks / Definition of done / Evidence.

## v1.4 parallel workflow
Use `AGENT-COORDINATION.md` for task claims, ownership, contract freeze, cross-lane review and integration gates. `cap dev` coordination commands are proposals, not executable commands in this archive. Never claim a task is complete solely because its Markdown checkbox was changed.

## Role-based development workflow

`Sol plans → Terra and Luna implement → local exact-commit checks → independent Sol review`

- Only Sol receives the user prompt. Freeze the request and acceptance criteria before dispatch, then give workers scoped tasks and isolated worktrees. Terra coordinates integration and requests review. Luna implements, tests and documents.
- Qwen can implement and test only when verified provider usage limits block both Terra and Luna, under the same project limits. It cannot review or audit.
- The [workflow profile](../tools/agent-harness/ai-workflow.yaml) allows up to eight concurrent workers (three Terra and five Luna) and three revision rounds per task after its initial pass.
- Promote internal milestones after all applicable required checks pass and a fresh, read-only Sol session approves the exact commit. Astra independently audits the final delivery candidate. A human approves the audited candidate and its artifacts before production.

Commit and push each scoped code change to its topic branch and check required CI on the exact candidate. See [Local Agent Operations](LOCAL-AGENT-OPERATIONS.md) for the normative rules and [Harness Configuration](HARNESS-CONFIGURATION.md) for optional agent tooling.
