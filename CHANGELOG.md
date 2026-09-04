# Changelog

All notable Game_EX changes are recorded here. Detailed implementation evidence, discussion, limitations, and provenance are retained in the linked version reports.

## [Unreleased]

No changes recorded.

## [0.1.2] - 2026-09-05

### Added

- `GameEX::Startup`, an ordinarily owned subsystem graph with full pre-start validation, deterministic serial topological order, exact reverse shutdown, and partial-start rollback.
- Unit coverage for invalid registrations, missing dependencies, cycles, stable ordering, cleanup continuation, rollback, lifecycle misuse, and destructor cleanup.
- An application integration that controls native-window visibility through the startup lifecycle.
- A Czech Republic data-source strategy covering source roles, precedence, EPSG:5514 precision, inspect-before-combine staging, the Bystřice proof, and required provenance.
- Architecture decisions for the serial startup contract, renderer baselines/selection/parity, and the WorldCompiler repository split gate.

### Changed

- Engine and lifecycle documentation now distinguish the implemented serial graph from the later controlled-parallel job-system slice.

### Limitations

- Startup is deliberately serial; worker scheduling and thread-affinity policy are deferred to the next independently verified version.
- The data strategy records acceptance rules but imports no external world data in this version.

See the [`v0.1.2` technical report](docs/history/reports/v0.1.2.md).

## [0.1.1] - 2026-09-05

### Added

- Working agreement covering specification gates, SemVer release units, verification, Doxygen, provenance, and the definition of done.
- Chronological version-history index, reusable technical-report template, and reports for `v0.1.0` and `v0.1.1`.
- Retrospective foundation report anchored to commit `2ac180d2064c1a7e57e4631259df2a3884b57998` without rewriting history.
- Central project version propagation for consistent workspace, Engine, WorldCompiler, Game, desktop metadata, and compiler banners.
- Architecture decision requiring OpenGL and Vulkan backends behind one shared Render API/RHI.

### Changed

- Root and contribution documentation now link the release workflow and evidence history.

### Limitations

- No compiler ingestion, real-world data, rendering, or editor visualisation capability is part of this administrative/build-facing version.

See the [`v0.1.1` technical report](docs/history/reports/v0.1.1.md).

## [0.1.0] - 2026-09-04

### Added

- C++20 workspace composed from independent Engine, WorldCompiler, and Game CMake projects.
- Platform-neutral application/window contracts with a private pinned SDL3 backend.
- Separate SDL3 game and world-editor executables.
- Headless world-compiler placeholder and logical runtime `WorldHeader` contract.
- Unit and GUI smoke tests, strict compiler warnings, and warnings-as-errors Doxygen generation.
- Project, architecture, section, development, and architecture-decision documentation.

### Limitations

- Windows are blank and no renderer, simulation, editor workflow, real-world data pipeline, or persistent binary world schema is implemented.

See the retrospective [`v0.1.0` foundation report](docs/history/reports/v0.1.0.md) and its exact [foundation commit](https://github.com/Jenson-Bolton/Game_EX/commit/2ac180d2064c1a7e57e4631259df2a3884b57998).
