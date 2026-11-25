# ADR-0004 Separate test units and platform source selection
Status: accepted.
Decision: `_test.cpp` tests are separate units; OS/architecture filename suffixes select sources. No amp inline-test preprocessor macro. Optional feature preprocessor definitions remain supported. Consequences: explicit target source ownership and scanner ordering rules.
