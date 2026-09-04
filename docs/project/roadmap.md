# Roadmap

This roadmap is directional. Only the foundation milestone is accepted scope; later milestones require specifications and architecture decisions before implementation.

## M0 — Foundation

Buildable three-project workspace, game window, world-editor window, world-format boundary, tests, Doxygen, and project documentation.

## M1 — Lifecycle and diagnostics

Specify and implement logging, configuration, crash reporting, the bootstrap sequence, job primitives, startup DAG validation/scheduling, affinity, profiling, and reverse shutdown.

## M2 — Rendering foundation

Specify the shared Render API/RHI boundary and backend-selection policy. Establish the OpenGL and Vulkan development environments, render the same diagnostic scene through an OpenGL context and a Vulkan swapchain, and define render-world extraction without leaking either graphics API into game or editor code.

## M3 — Editor shell

Select the UI approach and establish panels, commands, selection, undo/redo, document state, and safe separation of editor-only and runtime code.

## M4 — World format/compiler proof

Choose one small region and one narrow data path. Define coordinate/reference rules and a versioned package format before implementing broader Czech Republic ingestion.

## M5 — Simulation vertical slice

Specify deterministic clocks and domain data, then build a small headless transport/land-use slice with invariant and replay tests before national scale.
