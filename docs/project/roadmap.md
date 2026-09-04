# Roadmap

This roadmap is directional. M0 and the validated serial-startup slice of M1 are implemented; later slices require specifications and architecture decisions before durable interfaces or dependencies are introduced.

## M0 — Foundation

Buildable three-project workspace, game window, world-editor window, world-format boundary, tests, Doxygen, and project documentation.

## M1 — Lifecycle and diagnostics

Specify and implement logging, configuration, crash reporting, the bootstrap sequence, job primitives, startup DAG validation/scheduling, affinity, profiling, and reverse shutdown.

## M2 — Rendering foundation

Specify the shared Render API/RHI boundary and backend-selection policy. Establish the OpenGL and Vulkan development environments, render the same diagnostic scene through an OpenGL context and a Vulkan swapchain, and define render-world extraction without leaking either graphics API into game or editor code.

## M3 — Editor shell

Select the UI approach and establish panels, commands, selection, undo/redo, document state, and safe separation of editor-only and runtime code.

## M4 — World format/compiler proof

Use the [Bystřice proof and source hierarchy](data-source-strategy.md) to introduce independently inspectable layers through one narrow data path. Define the package format and remaining coordinate rules before broader Czech Republic ingestion, and resolve the [WorldCompiler repository split gate](../decisions/0005-world-compiler-repository-split-gate.md) before adding geospatial or ML acquisition dependencies.

## M5 — Simulation vertical slice

Specify deterministic clocks and domain data, then build a small headless transport/land-use slice with invariant and replay tests before national scale.
