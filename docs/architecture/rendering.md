# Rendering architecture

`v0.1.6` completes the first cross-technology rendering proof. Both desktop
applications pass the exact same `DiagnosticFrame` to a backend-neutral API and
can present it through OpenGL 4.6 Core or Vulkan 1.3. The contract still contains
no shaders, meshes, resources, command lists, world data, or editor concepts.

## Target and type boundaries

```text
GameEX::Core -----------> GameEX::Render -----------> GameEX::Platform
                              ^                           ^
                              |                           |
             +----------------+----------------+          |
             |                                 |          |
GameEX::RenderOpenGL               GameEX::RenderVulkan  |
  private: SDL3, GLAD, bridge        private: SDL3, Vulkan, bridge
             +--------------------------------------------+
```

`Renderer`, `RendererFactory`, `DiagnosticFrame`, presentation outcomes,
lifecycle states, errors, and diagnostics use only ordinary C++ and neutral
platform types. A factory declares `WindowGraphicsApi` before SDL creates a
hidden window. The checked private bridge then returns `SDL_Window*` only to the
matching backend. No public common header exposes SDL, OpenGL, Vulkan, or an
untyped native handle.

`Renderer::render_frame()` returns `presented`, `deferred_zero_extent`, or
`deferred_surface_change`. Deferral is successful control flow, not proof of a
presentation. `RendererDiagnostics::presented_frames` counts only successful
native presents; timed application smokes fail when this remains zero.

## Shared colour contract

The frame returned by `foundation_diagnostic_frame()` is exactly
`(0.035, 0.065, 0.110, 1.0)`. Components are finite linear float values in
inclusive `[0, 1]`. OpenGL requires a verified sRGB-capable default framebuffer
and enables `GL_FRAMEBUFFER_SRGB`. Vulkan accepts only B8G8R8A8_SRGB or
R8G8B8A8_SRGB with SRGB_NONLINEAR colour space and requires transfer-destination
format/usage support. Clearing an sRGB Vulkan image with float32 clear values
applies the corresponding linear-to-sRGB conversion.

The project claims identical semantic input and successful presentation through
both technologies. It does not claim screenshot, byte, display, or perceptual
pixel equality across APIs and drivers.

## OpenGL backend

`GameEX::RenderOpenGL` requests and verifies OpenGL 4.6 Core, double buffering,
8-bit RGBA channels, and an sRGB-capable default framebuffer. It loads the
procedures used by this slice from the audited GLAD 2.0.8 output through SDL,
queries real context/profile/vendor/device facts, clears the drawable pixel
extent, checks `glGetError`, and swaps. The existing GLAD file hashes and LF
normalisation controls remain unchanged; see
[`Engine/third_party/glad/README.md`](../../Engine/third_party/glad/README.md).

## Vulkan backend

`GameEX::RenderVulkan` requests Vulkan 1.3 and synchronization2. Startup creates
an instance, optional validation messenger, SDL surface, deterministic physical
device/queues, logical device, one resettable command buffer, one frame fence,
one acquire semaphore, and an initial swapchain when drawable extent permits.
Device suitability requires:

- API 1.3 or newer and `synchronization2`;
- `VK_KHR_swapchain`;
- graphics and surface-presentation queues;
- FIFO presentation;
- transfer-destination surface usage;
- a supported sRGB/nonlinear format with transfer-destination format features.

Selection is stable under shuffled enumeration: discrete devices precede
integrated, virtual, CPU, and other devices; UUID and stable identifiers break
ties. A lowest combined graphics/present queue is preferred, otherwise the
lowest separate pair is used with concurrent image sharing. B8 sRGB precedes R8
sRGB. Fixed surface extents win; variable extents clamp current SDL drawable
pixels. Image count is one above the minimum where the maximum permits it,
composite alpha uses a fixed preference beginning with opaque, and FIFO is
mandatory.

One frame is in flight. After acquisition, synchronization2 barriers move the
image from undefined/present layout to transfer destination, a transfer clear
writes the shared colour, and another barrier moves it to present layout.
`vkQueueSubmit2` signals a binary semaphore dedicated to that swapchain image;
presentation waits on it. Per-image ownership prevents unsafe semaphore reuse
before the same image is reacquired.

Swapchain replacement is transactional: the replacement handle, image list,
layout history, and per-image semaphores are complete before the active
generation changes. Zero extent defers safely. Out-of-date/suboptimal results
mark replacement work without becoming renderer fallback. Runtime replacement
errors are `presentation_failed`. This baseline waits conventionally for device
idle before replacing old WSI objects; it does not claim a fully signalled
unextended KHR_swapchain presentation-retirement proof. A future
`VK_EXT_swapchain_maintenance1` decision may add presentation fences.

## Lifecycle and selection

All backend lifecycle, presentation, diagnostics, and destruction calls are
creator-thread-affine. Failed startup remains shutdown-capable for graph
rollback. Destruction releases renderer resources before the borrowed Window and
SDL runtime.

Invalid frame input and typed zero-extent/surface-change deferrals leave a
renderer reusable. An exception after native frame work begins is terminal:
the renderer enters `failed`, the error propagates, and only shutdown/rollback
is permitted. Retrying could otherwise reuse a consumed semaphore, unsignalled
fence, partial command buffer, or uncertain OpenGL context state.

Both apps accept `--renderer=opengl|vulkan|auto`; omission means `auto`. Explicit
choices make one attempt. Auto tries Vulkan and may make one fresh OpenGL attempt
only after a typed unavailable/initialization failure before visibility. A
different code, post-visibility failure, presentation failure, or shutdown
failure is propagated. Each attempt owns a new Platform, Window, Renderer, and
Application, so native window flags and partial state are never reused.

Renderer startup plus one hidden presentation attempt precede `Window::show()`.
A zero extent can defer that attempt; the visible loop must later present.
Smokes select each backend explicitly, while separate policy and auto tests
exercise fallback behaviour.

## Validation and current limits

Vulkan validation may be disabled, enabled when available, or required. The
required smoke isolates Khronos validation from host implicit overlays, executes
at least 32 visible presents to exercise semaphore reuse, requires zero debug
errors, and fails on validation errors. Ordinary applications do not force the
layer.

The next rendering slice may add only the smallest cross-backend resources
needed to visualise the existing synthetic `.gexworld` terrain and validity
data. Shader build policy, pipeline layout, camera, buffers, and render-world
extraction remain specification work for `v0.1.7`; no design is implied here.
