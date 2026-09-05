# Engine section

## Purpose

Engine contains reusable infrastructure that has no knowledge of the Game_EX simulation or Czech Republic datasets.

## Implemented now

- `GameEX::Jobs`: owns 1–32 persistent workers and executes bounded synchronous batches with input-indexed results.
- `GameEX::Startup`: validates an owned dependency graph, offers deterministic serial and bounded controlled-parallel startup paths, and performs reverse shutdown or partial-start rollback.
- `GameEX::Core`: owns the foundation application lifetime, renderer, immutable render frame, and diagnostic event loop.
- `GameEX::Platform`: exposes platform-neutral window capabilities, including the graphics API required at creation.
- `GameEX::PlatformSDL`: initialises SDL3, creates one native presentation-capable window, and pumps close events.
- `GameEX::Render`: defines backend-neutral lifecycle, validated linear
  background/optional bounded raster frames, shared aspect layout, diagnostics,
  and factories.
- `GameEX::RenderOpenGL`: creates and verifies an OpenGL 4.6 Core context, loads
  it through GLAD, and presents backgrounds plus raster cells with scissored
  sRGB clears.
- `GameEX::RenderVulkan`: creates a Vulkan 1.3 synchronization2/dynamic-rendering
  device and deterministic sRGB FIFO swapchain, using transfer clears for the
  foundation and attachment clears for bounded rasters.

## Invariants

- SDL/OpenGL/Vulkan types remain private to their implementation targets and checked native bridge.
- Platform and window objects are uniquely owned.
- Renderer, window, and platform objects are uniquely owned; no renderer singleton or service locator exists.
- The renderer is destroyed before its borrowed window, and the window before the runtime that created it.
- Video initialisation, context operations, window operations, rendering, event pumping, and destruction stay on the composition thread.
- Renderer startup and a hidden presentation attempt precede visibility; a zero
  extent may defer until the visible loop, and timed tests require a real present.
- Diagnostic colours are finite linear values in `[0, 1]`; optional owning
  rasters are at most `64 x 64`, row zero is lower, and a shared integer layout
  preserves declared aspect. OpenGL verifies a double-buffered sRGB framebuffer;
  Vulkan accepts only sRGB/nonlinear FIFO transfer/colour-attachment swapchains.
- Vulkan selection/device/queue/format/extent/image-count/composite policy is
  deterministic; one frame is in flight with a present-wait semaphore per image.
- A typed deferral remains retryable, while a native frame exception is terminal
  until creator-thread shutdown.
- Startup registrations use stable identifiers and ordinary ownership; the graph is not a service locator.
- Missing dependencies and cycles are rejected before any subsystem starts.
- Shutdown and failed-start rollback use the exact reverse attempted-start order.
- Worker physical completion may vary, but logical admission, result indexing, primary failure, commit, and reverse rollback are deterministic.
- Main-thread-affine nodes run on the graph owner; only explicitly worker-eligible ready prefixes enter a bounded batch.
- JobSystem and StartupGraph control and destruction remain on their creating application thread; wrong-thread destruction fails before cleanup.

## Planned, not implemented

Shaders, textures, buffers, meshes, scalable resource lifetimes, production
terrain rendering, render-world extraction, core configuration, serialization,
ECS, input, audio, world streaming, profiling, headless rendering, maintenance1
presentation fences, and any broader asynchronous/frame-job system still
require their own boundaries and tests. Engine intentionally has no knowledge
of `.gexworld`, terrain, EPSG:5514, provenance, or editor mapping.
