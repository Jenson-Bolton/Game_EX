# Roadmap

This roadmap is directional. M0, the validated serial/controlled-parallel parts
of M1, the diagnostic OpenGL/Vulkan portion of M2, and the dependency-free
package/codec portion of M4 are implemented. Later slices require specifications
and architecture decisions before durable interfaces or dependencies are introduced.

## M0 — Foundation

Buildable three-project workspace, game window, world-editor window, world-format boundary, tests, Doxygen, and project documentation.

## M1 — Lifecycle and diagnostics

The validated serial startup graph, [bounded JobSystem, and controlled parallel-startup contract](../decisions/0008-bounded-jobs-controlled-parallel-startup.md) are implemented. Logging, configuration, crash reporting, wider diagnostics, profiling, and any broader frame-job model remain later work.

## M2 — Rendering foundation (diagnostic raster parity complete; resources pending)

The shared diagnostic Render API, private SDL bridge, verified OpenGL 4.6 Core
path, Vulkan 1.3 synchronization2 transfer-clear/dynamic-rendering path, strict
explicit/auto selection, and semantic clear/raster parity are implemented. The
bounded shaderless raster proves package-derived colours, aspect, and orientation
through both APIs. A scalable texture/shader resource path is still required for
full-resolution, multi-layer, interactive, or 3D work. Captured-pixel parity,
wider diagnostics, and maintenance1 presentation fences remain later work.

## M3 — Editor shell

The editor can load and visualise one `.gexworld` terrain/validity layer in an
aspect-correct 2D plan view through the same OpenGL/Vulkan technologies as the
game. Editor-only mapping is separated from the player executable and bad
packages fail before a window attempt. Next select the scalable viewport/UI
approach and establish legends, picking, pan/zoom or 3D navigation, panels,
commands, selection, undo/redo, and document state.

## M4 — World format/compiler proof (in progress)

The first portable slice defines and tests strict `.gexstage` input, deterministic `.gexworld` output, a runtime-only reader, coordinate/height precision, validity, provenance, integrity, and CLI inspection using synthetic data. Next, use the [Bystřice proof and source hierarchy](data-source-strategy.md) to introduce independently inspectable real layers through that narrow path. Resolve the [WorldCompiler repository split gate](../decisions/0005-world-compiler-repository-split-gate.md) before adding geospatial or ML acquisition dependencies.

## M5 — Simulation vertical slice

Specify deterministic clocks and domain data, then build a small headless transport/land-use slice with invariant and replay tests before national scale.
