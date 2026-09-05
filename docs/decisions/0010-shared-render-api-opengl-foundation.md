# ADR 0010: Shared Render API and OpenGL foundation

- Status: Accepted
- Date: 2026-09-05
- Owners: Jenson Bolton and Game_EX implementers

## Context

[ADR 0004](0004-dual-renderer-backends.md) requires OpenGL and Vulkan behind one
shared application-facing boundary. [ADR 0007](0007-renderer-baselines-selection-and-parity.md)
fixes the OpenGL 4.6 Core and Vulkan 1.3 baselines and permits separate patch
releases. Before the first backend, `Application` owned only an SDL window and a
startup graph. Adding speculative shaders, meshes, resources, command buffers,
or a lowest-common-denominator RHI now would force untested abstractions into
both later APIs.

OpenGL also creates a lifetime boundary that cannot be hidden inside ordinary
window visibility. The SDL window must be created with the OpenGL flag, the
context must exist before useful visibility, its functions must be loaded, and
the context must be destroyed before the window and SDL runtime. None of those
requirements may leak SDL or OpenGL types into public engine, game, or editor
headers.

## Decision

Introduce `GameEX::Platform` as the public abstract platform target,
`GameEX::Render` as the shared renderer contract, and `GameEX::RenderOpenGL` as
the concrete backend. The public contract is deliberately limited to:

- ordinary unique ownership through `Renderer` and `RendererFactory`;
- actual backend identity, lifecycle state, and categorized exceptions;
- one finite linear RGBA clear colour with every component in `[0, 1]`;
- one clear/present operation;
- neutral major/minor/patch, profile, debug-diagnostics, version, vendor, and
  device diagnostics;
- a platform-neutral `WindowGraphicsApi` selected before window creation.

No shader, mesh, resource, command-list, world-data, or editor concept is added.
The `vulkan` backend identity and window capability are named for the accepted
next backend, but no Vulkan implementation or false fallback is present.

The OpenGL backend requests version 4.6, Core profile, double buffering, 8-bit
RGBA channels, and an sRGB-capable default framebuffer before SDL creates the
hidden window. Startup creates and makes current the context, uses GLAD generated
for OpenGL 4.6 Core with no extensions, confirms a context advertising that
version, explicitly checks every procedure this slice calls, verifies the actual
version and Core profile, verifies double-buffer and sRGB attributes, enables
`GL_FRAMEBUFFER_SRGB`, and retains driver diagnostics. Each frame queries
drawable pixels, sets the viewport, clears, checks `glGetError`, and swaps.

All renderer lifecycle, frame, diagnostics, and destruction operations are
creator-thread-affine. Lifecycle is single-use. A partially failed start enters
a shutdown-capable failed state so StartupGraph rollback can release native
state. Wrong-thread OpenGL destruction fails fast rather than silently invoking
thread-affine SDL cleanup elsewhere.

Application obtains the required graphics capability from a short-lived factory,
creates a hidden compatible window and dormant renderer, and owns both. Its
startup dependency is:

```text
render.backend --> platform.window.visibility
```

Renderer startup includes one successful diagnostic clear/present before the
window is shown. Reverse shutdown hides the window before renderer shutdown;
ordinary member destruction releases the renderer before its borrowed window
and the window before SDL. Both game and editor directly compose OpenGL and the
same `foundation_diagnostic_frame()` in this patch. CLI renderer selection is
deferred until Vulkan exists in `v0.1.6`.

OpenGL function loading uses generated output from official GLAD tag `v2.0.8`.
The supported build vendors only `gl.h`, `khrplatform.h`, `gl.c`, exact upstream
licensing content, and an audit record. It does not vendor or run the generator,
Python, or Jinja. Exact archive/output hashes, generator arguments, licenses,
and LF line endings are recorded and checked. This is the sole narrow exception
to the normal rule that generated/dependency content stays in `build`.

## Consequences

Game and editor now prove the same real rendering lifecycle and diagnostic
colour instead of mere window survival. Core remains independent of concrete
SDL/OpenGL types, while static target boundaries leave a direct route for
`GameEX::RenderVulkan`. Window creation can support that backend without a
public native handle.

Requiring OpenGL 4.6 Core, double buffering, and sRGB capability makes missing
or older drivers fail explicitly. GUI smoke registration can be disabled for a
headless Engine/workspace configuration, but that is not evidence that the
desktop renderer works. Real GUI evidence reports the driver facts.

The first Render API is intentionally too small for terrain or gameplay. Vulkan
may extend neutral diagnostics without changing OpenGL-specific fields because
none exist in the shared structure. A future resource/render-world API requires
new cross-backend evidence rather than growth by anticipation.

## Alternatives considered

Putting OpenGL context calls in `Window` would conflate platform ownership with
rendering and make Vulkan surface/device lifetime awkward. Exposing
`SDL_Window*` or `void*` publicly would leak implementation and weaken type
safety. Using SDL's higher-level rendering API would not implement the required
OpenGL/Vulkan technologies. Generating GLAD during every build would introduce a
supported Python/Jinja dependency; downloading pre-generated unpinned output
would weaken reproducibility. Adding `--renderer` before Vulkan could only
provide fake choices or premature fallback behavior.
