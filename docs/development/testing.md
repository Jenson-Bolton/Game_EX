# Testing

CTest is the common test runner. Tests are labelled by purpose:

- `unit`: deterministic, dependency-light logic checks;
- `gui`: requires a desktop windowing environment;
- `smoke`: proves a top-level composition path starts and exits successfully.

Current tests verify bounded job batches, concurrency and lifecycle protection; serial and controlled-parallel startup validation/order/affinity/barriers/rollback; strict stage parsing and package encoding/decoding; stable WorldCompiler CLI behaviour; world-header compatibility; and timed launches of both SDL3 applications. A window smoke test succeeds only if SDL initialisation, native window creation, job-pool/bootstrap creation, startup-controlled showing, event pumping, reverse shutdown/hiding, worker joining, and orderly destruction all complete.

JobSystem tests use explicit worker counts and controlled gates rather than elapsed time as evidence. They cover invalid bounds, true two-worker overlap, exact-once/off-owner execution, input-indexed failures despite an inverted controlled failure-point order, nested/concurrent/foreign rejection, waiting for in-flight work, rejection of later startup admission, main-thread affinity, barrier dependencies, and serial reverse-logical rollback.

WorldCompiler tests use a project-authored 2x2 EPSG:5514 fixture with a
negative double-precision origin, non-zero vertical origin, local float heights,
and one invalid sample. They lock the complete WorldFormat 0.2 bytes to a
checked-in textual golden, load that golden through an executable linked only to
`GameEX::WorldFormat`, and check deterministic compilation across directories,
complete provenance round trip, fixed stage-semantic enforcement on both encode
and read, the standard CRC-32/ISO-HDLC `123456789` vector, corruption,
truncation, unsupported versions, non-canonical offsets, unsafe paths, staged
size/hash tampering, strict whole-file manifest grammar, finite coordinate
bounds, output/foreign-lock/legacy-part preservation, successful owned-artifact
cleanup, a controlled same-output compiler race, and stable CLI exit categories.
The fixture proves codec behaviour only; real-data acceptance requires a
separate source-specific report.

Future domain tests should favour fixed inputs, explicit seeds, state invariants, replay checksums, and geographically separated validation data where machine-learning or semantic classifiers are evaluated. Long-running whole-world tests must not replace small reproducible vertical slices.
