# Release checklist

- [ ] All 20 module design/API/contract/task documents reviewed against implementation.
- [ ] System compiler passes C++26 reflection feature probe and all ABI checks.
- [ ] Boost newest stable release identified; exact Git tag, commit and submodule revisions recorded and verified.
- [ ] `make boost-fetch`, `make boost-bootstrap`, `make boost-build` pass on supported hosts.
- [ ] `make bootstrap`, native self-host build, and offline/preseeded bootstrap pass.
- [ ] Unit/integration/property/negative tests pass; >=85% line and >=75% branch coverage (critical >=95% line).
- [ ] Sanitizers, benchmarks, profiling and regression comparisons recorded.
- [ ] `compile_commands.json` matches actual build plan; clangd smoke tests pass.
- [ ] Plugin and VS Code extension integration tests pass.
- [ ] Security checks, credentials isolation, download provenance, registry and toolchain verification pass.
- [ ] Every public class/interface documented and contract-tested.
- [ ] All frozen, applicable release checks pass on the exact candidate commit. Required evidence includes the acceptance revision and artifact provenance.
- [ ] Independent Sol milestone approval and Astra final audit identify the verified candidate. The audit includes the production artifacts and target.
- [ ] An authorized human approves the audited commit, artifact digests and target before production publication or deployment. Record the approval with the release evidence. See [Agent Development Operations](LOCAL-AGENT-OPERATIONS.md).
