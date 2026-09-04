# Testing

CTest is the common test runner. Tests are labelled by purpose:

- `unit`: deterministic, dependency-light logic checks;
- `gui`: requires a desktop windowing environment;
- `smoke`: proves a top-level composition path starts and exits successfully.

Current tests verify startup-graph validation/order/rollback/lifecycle behaviour, world-header compatibility, and timed launches of both SDL3 applications. A window smoke test succeeds only if SDL initialisation, native window creation, startup-controlled showing, event pumping, reverse shutdown/hiding, and orderly destruction all complete.

Future domain tests should favour fixed inputs, explicit seeds, state invariants, replay checksums, and geographically separated validation data where machine-learning or semantic classifiers are evaluated. Long-running whole-world tests must not replace small reproducible vertical slices.
