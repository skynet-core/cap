# ADR-0002 Local overrides do not mutate committed lock
Status: accepted.
Decision: local `.cap/config.local.toml` is ignored, applied to effective resolution with separate local state and action fingerprints. Publishing disallows unresolved local overrides. Consequences: local reproducibility requires exporting the effective override configuration separately.
