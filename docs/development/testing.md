# Testing

CTest is the common test runner. Tests are labelled by purpose:

- `unit`: deterministic, dependency-light logic checks;
- `gui`: requires a desktop windowing environment;
- `smoke`: proves a top-level composition path starts and exits successfully.

Current tests verify bounded job batches, concurrency and lifecycle protection; serial and controlled-parallel startup validation/order/affinity/barriers/rollback; Render API value validation; fake-backed renderer/application ordering and rollback; a real OpenGL 4.6 Core clear/present path; strict stage parsing and package encoding/decoding; stable WorldCompiler CLI behaviour; world-header compatibility; and timed rendered launches of both SDL3 applications. An application smoke succeeds only if SDL initialisation, OpenGL-capable native window creation, job/bootstrap creation, context verification, hidden first clear/present, startup-controlled showing, event pumping/rendering, reverse hiding/context shutdown, worker joining, and orderly destruction all complete.

Render API tests cover finite inclusive `[0, 1]` colour validation, stable lowercase backend text, and categorized errors. Fake Application tests prove the factory capability reaches window creation, renderer start/first frame precede show, start and initial-frame failures roll back without visibility, runtime frame failure hides before renderer shutdown, and invalid factory composition is rejected. The GUI backend smoke checks lifecycle misuse and reports the actual version/profile/vendor/device after a real clear/swap. Game and editor smoke tests then exercise the same production composition path. Set `GAMEEX_ENABLE_GUI_SMOKE_TESTS=OFF` when configuring a genuinely headless test environment; the non-GUI lifecycle evidence remains available.

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
