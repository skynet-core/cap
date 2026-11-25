# Native scheduler contract (normative, v1.4)

**Status:** proposed; no scheduler implementation exists in this specification bundle. Owner: `amp-build`; adapters in `amp-core`, cache cooperation in `amp-cache`.

## Action model and concurrency
The planner freezes a versioned, acyclic action DAG before execution. Each action declares input digests, outputs, argv (never shell interpolation), working directory, approved environment, toolchain identity, target platform, resource class, estimated peak memory, CPU weight, cancellation token and timeout. The scheduler may vary execution order; output bytes and cache keys must not depend on that order. Unknown memory estimates use a conservative default; measured estimates may be persisted as advisory data, never as build inputs.

`--jobs=N` caps concurrently admitted CPU-weighted work; `--max-memory=SIZE` caps estimated aggregate peak memory. Define default limits from available CPU and memory, with explicit platform probes and fallback. Support separate class quotas for compile, link, scan, codegen and network fetch (fetch never occurs in the frozen execution phase). One oversized action may run alone under a documented policy; never deadlock on impossible resource requests. No unlimited process spawning.

## Scheduling and cancellation
Maintain ready, running, completed, failed and cancelled states; dependency counters are updated atomically. Prefer critical-path actions when estimates exist; provide bounded fairness so independent targets cannot starve. Determinism applies to inputs, fingerprints, final artifacts and diagnostic ordering, **not** identical thread scheduling. On failure, stop admitting dependent work; optional keep-going may finish independent subgraphs. Cancellation propagates to child process trees, releases resources and leases, and leaves no partial published outputs. Implement timeout and shutdown escalation with platform-specific adapters.

## Interfaces (proposals)
`ScheduleOptions{jobs,max_memory,class_limits,keep_going}`; `ActionResources{cpu_weight,estimated_peak_memory,class}`; `ActionScheduler::execute(BuildPlan, IActionExecutor, ScheduleOptions, StopToken)->Result<BuildReport>`; `BuildReport{action_states,resource_peak,critical_path,diagnostics}`. Final signatures require `projects/amp-build/API.md` review and contract fixtures.

## Acceptance fixtures
10k-node DAG scheduling benchmark; diamond dependency, cycle rejection, oversized link, memory throttling, fairness, deterministic output, cancellation during compiler process, cancellation with descendants, concurrent failure, TSan stress, and no partial output publication. Record CPU/RAM/OS/compiler and compare no-op and full build baselines. See `BUILD-OBSERVABILITY.md`.
