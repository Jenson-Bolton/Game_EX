# Roadmap

This roadmap is directional. M0, the validated serial/controlled-parallel parts of M1, and the dependency-free package/codec portion of M4 are implemented. Later slices require specifications and architecture decisions before durable interfaces or dependencies are introduced.

## M0 — Foundation

Buildable three-project workspace, game window, world-editor window, world-format boundary, tests, Doxygen, and project documentation.

## M1 — Lifecycle and diagnostics

The validated serial startup graph, [bounded JobSystem, and controlled parallel-startup contract](../decisions/0008-bounded-jobs-controlled-parallel-startup.md) are implemented. Logging, configuration, crash reporting, wider diagnostics, profiling, and any broader frame-job model remain later work.

## M2 — Rendering foundation

Specify the shared Render API/RHI boundary and backend-selection policy. Establish the OpenGL and Vulkan development environments, render the same diagnostic scene through an OpenGL context and a Vulkan swapchain, and define render-world extraction without leaking either graphics API into game or editor code.

## M3 — Editor shell

Select the UI approach and establish panels, commands, selection, undo/redo, document state, and safe separation of editor-only and runtime code.

## M4 — World format/compiler proof (in progress)

The first portable slice defines and tests strict `.gexstage` input, deterministic `.gexworld` output, a runtime-only reader, coordinate/height precision, validity, provenance, integrity, and CLI inspection using synthetic data. Next, use the [Bystřice proof and source hierarchy](data-source-strategy.md) to introduce independently inspectable real layers through that narrow path. Resolve the [WorldCompiler repository split gate](../decisions/0005-world-compiler-repository-split-gate.md) before adding geospatial or ML acquisition dependencies.

## M5 — Simulation vertical slice

Specify deterministic clocks and domain data, then build a small headless transport/land-use slice with invariant and replay tests before national scale.
