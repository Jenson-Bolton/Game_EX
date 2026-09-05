# Engine section

## Purpose

Engine contains reusable infrastructure that has no knowledge of the Game_EX simulation or Czech Republic datasets.

## Implemented now

- `GameEX::Jobs`: owns 1–32 persistent workers and executes bounded synchronous batches with input-indexed results.
- `GameEX::Startup`: validates an owned dependency graph, offers deterministic serial and bounded controlled-parallel startup paths, and performs reverse shutdown or partial-start rollback.
- `GameEX::Core`: owns the foundation application lifetime and main event loop.
- `GameEX::PlatformSDL`: initialises SDL3, creates one native window, and pumps close events through platform-neutral interfaces.

## Invariants

- SDL types remain private to the SDL backend source file.
- Platform and window objects are uniquely owned.
- A window is destroyed before the runtime that created it.
- Video initialisation, window operations, and event pumping stay on the main thread.
- Startup registrations use stable identifiers and ordinary ownership; the graph is not a service locator.
- Missing dependencies and cycles are rejected before any subsystem starts.
- Shutdown and failed-start rollback use the exact reverse attempted-start order.
- Worker physical completion may vary, but logical admission, result indexing, primary failure, commit, and reverse rollback are deterministic.
- Main-thread-affine nodes run on the graph owner; only explicitly worker-eligible ready prefixes enter a bounded batch.
- JobSystem and StartupGraph control and destruction remain on their creating application thread; wrong-thread destruction fails before cleanup.
- No renderer is implied by the current blank window.

## Planned, not implemented

Core diagnostics/configuration, resources, serialization, ECS, input, audio, world streaming primitives, the shared Render API/RHI, OpenGL and Vulkan backends, profiling, headless support, and any broader asynchronous/frame-job system still require their own boundaries and tests.
