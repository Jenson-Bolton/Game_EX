# Changelog

All notable Game_EX changes are recorded here. Detailed implementation evidence, discussion, limitations, and provenance are retained in the linked version reports.

## [Unreleased]

No changes recorded.

## [0.1.6] - 2026-09-05

### Added

- `GameEX::RenderVulkan`, a real Vulkan 1.3/synchronization2 transfer-clear
  backend with deterministic device, queue, sRGB format, extent, image-count,
  composite-alpha, and FIFO presentation policy.
- One-frame-in-flight submission, transactional swapchain generations,
  per-image present-wait semaphores, zero-extent/surface-change deferral, typed
  failures, optional/required validation, and neutral presentation diagnostics.
- Strict `--renderer=opengl|vulkan|auto` parsing for game and editor; omission
  means auto, explicit choices never fall back, and auto uses a fresh composition
  only after an eligible hidden Vulkan failure.
- Default-on Vulkan SDK discovery with an explicit opt-out, pure policy/fallback
  tests, real repeated-presentation Vulkan and validation smokes, and explicit
  OpenGL/Vulkan game/editor smokes.
- ADR 0011 and updated rendering, application, build, testing, scope, roadmap,
  section, and history documentation.

### Changed

- `Renderer::render_frame` now reports presented, zero-extent deferred, or
  surface-change deferred outcomes; native-frame exceptions make the renderer
  terminal until shutdown.
- Application records monotonic visibility evidence and timed runs require at
  least one successful presentation rather than timer survival.
- Both applications now provide the same exact linear diagnostic frame to real
  OpenGL and Vulkan implementations; this is semantic/presentation parity, not
  a pixel-equality claim.
- The project version is now `0.1.6`. Existing GLAD 2.0.8 hashes and LF controls
  are unchanged.

### Limitations

- This slice adds no shaders, SPIR-V, meshes, resources, terrain loading,
  world-editor visualisation, gameplay, or simulation.
- The committed world fixture is still synthetic; no real source adapter is
  accepted or combined.
- Vulkan evidence is Windows/MSVC/NVIDIA only. Conventional swapchain recreation
  does not claim fully signalled WSI retirement without a future maintenance1
  presentation-fence decision.

See the [`v0.1.6` technical report](docs/history/reports/v0.1.6.md).

## [0.1.5] - 2026-09-05

### Added

- `GameEX::Render`, a backend-neutral, ordinarily owned lifecycle for one validated linear-colour diagnostic clear frame and backend-neutral context diagnostics.
- `GameEX::RenderOpenGL`, an SDL/OpenGL 4.6 Core backend that requests and verifies a double-buffered sRGB-capable framebuffer, loads and checks its used procedures through GLAD, queries drawable pixels, clears, presents, and checks OpenGL errors.
- A private checked SDL native-window bridge that keeps SDL and OpenGL types out of public Engine, game, and editor headers.
- Fake-backed lifecycle tests plus a real OpenGL GUI smoke test reporting the actual API version, profile, vendor, and device.
- ADR 0010, a rendering architecture document, reproducible GLAD provenance, and this version report.

### Changed

- Both game and world editor now directly compose the same OpenGL backend and display the same dark-blue diagnostic clear frame.
- Application startup creates and renders through the renderer before showing the window; reverse shutdown hides the window before releasing graphics state.
- The project version is now `0.1.5`, and the Engine can be configured independently with GUI smoke registration enabled or disabled.

### Limitations

- This slice deliberately has no shaders, meshes, resource API, world-package visualisation, editor UI, gameplay, or simulation.
- Vulkan 1.3 and explicit `--renderer=opengl|vulkan|auto` policy remain the next renderer slice; both applications intentionally select OpenGL directly for now.
- OpenGL 4.6 Core and an sRGB-capable default framebuffer are hard requirements; older or headless environments must disable GUI smoke registration but cannot run the current desktop renderer.

See the [`v0.1.5` technical report](docs/history/reports/v0.1.5.md).

## [0.1.4] - 2026-09-05

### Added

- A strict version-1 `.gexstage` manifest for one EPSG:5514 terrain layer/tile, including explicit source and staged-data provenance, byte sizes, SHA-256 digests, vertical reference, axis order, units, no-data convention, and transformation history.
- The first concrete `.gexworld` 0.2 wire contract: canonical little-endian encoding, CRC-32/ISO-HDLC integrity, bounded reader allocations, tile-local binary32 heights, double-precision world origins, and a one-byte validity value per sample.
- `GameEX::WorldCompilerCore` for dependency-free validation and deterministic compilation, plus a runtime-only `GameEX::WorldFormat` reader usable without the compiler.
- Stable `validate`, `compile`, and `inspect` CLI commands with categorized exit codes and refusal to overwrite output.
- Unit/integration coverage for deterministic round trips, manifest/path/hash validation, malformed packages, canonical offsets, corruption, version rejection, provenance retention, and CLI behaviour.
- A project-authored synthetic terrain fixture, a normative format specification, ADR 0009, and a forensic audit of the rejected legacy Bystřice coursework data.

### Changed

- World format version `0.2` now identifies an implemented serialized package rather than the earlier logical-only `0.1` header.
- Project documentation now distinguishes the implemented portable compiler/reader from the still-pending source adapters, real-data acceptance, editor visualisation, and layer reconciliation.

### Limitations

- The compiler accepts only one small normalised terrain tile represented by text height and validity staging files; it has no acquisition, CRS transformation, LAS/LAZ, GDAL/PROJ, Python, network, or TESSERA adapter.
- The fixture is synthetic and grants no external reuse rights; it is not evidence for real-world correctness.
- The game and editor do not load or display `.gexworld` yet, and OpenGL/Vulkan rendering remains a subsequent milestone.

See the [`v0.1.4` technical report](docs/history/reports/v0.1.4.md).

## [0.1.3] - 2026-09-05

### Added

- `GameEX::Jobs`, an ordinarily owned fixed worker pool with a 1–32 bound, conservative hardware recommendation, synchronous indexed batches, deterministic exception results, and explicit shutdown.
- Controlled parallel startup with safe-by-default main-thread affinity, bounded lexical worker admission, dependency barriers, deterministic primary failure, and reverse logical rollback.
- Unit tests covering actual overlap, exact-once worker execution, invalid and nested/concurrent use, reordered failures, affinity, dependency barriers, stopped admission, and owner-thread cleanup.
- ADR 0008 documenting the bounded job and controlled parallel-startup contract.

### Changed

- `Application` now owns its worker pool as serial bootstrap infrastructure and uses the controlled startup path; native window visibility remains main-thread-affine.

### Limitations

- The job system is intentionally a synchronous startup primitive, not a general frame scheduler; it has no priorities, cancellation, work stealing, futures, nested submission, or dynamic resizing.
- No rendering, world compilation, or external dataset is added in this version.

See the [`v0.1.3` technical report](docs/history/reports/v0.1.3.md).

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
