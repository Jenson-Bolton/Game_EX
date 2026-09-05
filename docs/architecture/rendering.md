# Rendering architecture

`v0.1.5` establishes the smallest shared rendering boundary that can be proved
through a real graphics API without guessing at the future resource model. It
supports one operation: validate a linear RGBA clear colour, clear the current
drawable pixel area, and present it. Shaders, meshes, resources, world data,
command buffers, and editor-specific concepts are deliberately absent.

## Target boundary

```text
GameEX::Core -----------> GameEX::Render -----------> GameEX::Platform
                              ^
                              |
GameEX::RenderOpenGL ---------+
  | private
  +--> GameEX::PlatformSDL private native-window bridge
  +--> SDL3
  `--> generated GLAD 2.0.8
```

`Renderer` and `RendererFactory` use only ordinary C++ and platform-neutral
types. A factory reports the required `WindowGraphicsApi` before the native
window is created, then creates one dormant renderer bound to that window. The
OpenGL implementation obtains `SDL_Window*` only through an Engine-private,
checked bridge. SDL and OpenGL types do not appear in public Engine, Game,
WorldCompiler, or editor headers.

The `RendererBackend` enum describes actual implementations (`open_gl` and the
reserved `vulkan` value). Selection modes such as `auto` belong to application
policy, not to an active renderer. `RendererDiagnostics` therefore uses neutral
major/minor/patch version fields, textual profile, generic debug-diagnostics
state, API version text, vendor, and device.

## Lifetime and frame contract

A renderer is uniquely owned by `Application` and is not restartable. Its
lifecycle is `dormant -> starting -> running -> stopping -> stopped`; startup or
cleanup failures use `failed`, which remains shutdown-capable for partial-start
rollback. OpenGL lifecycle, frame calls, diagnostics access, and destruction are
restricted to the creator/composition thread.

The startup dependency is:

```text
render.backend --> platform.window.visibility
```

The backend starts and presents one frame while the window is hidden. Only then
may the window be shown. Shutdown reverses this order, hiding before destroying
the context. The frame loop pumps events, queries `SDL_GetWindowSizeInPixels`,
sets `glViewport`, applies the clear, checks `glGetError`, swaps the window, and
performs simple pacing. A zero-sized drawable is legal and results in a
zero-sized viewport; a negative extent or failed query is an error.

All four `DiagnosticFrame` components must be finite and within inclusive
`[0, 1]`. They are linear values. The SDL backend requests and verifies an
sRGB-capable double-buffered default framebuffer, and the OpenGL backend enables
`GL_FRAMEBUFFER_SRGB` so the clear is encoded for presentation. Game and editor
both use the shared frame returned by `foundation_diagnostic_frame()`:
`(0.035, 0.065, 0.110, 1.0)`.

## OpenGL baseline and loading

The backend requests OpenGL 4.6 Core before creating the hidden window. Startup
uses GLAD generated for core 4.6 with no extensions and SDL's procedure resolver.
It confirms a context advertising the required version and explicitly checks
every function pointer this slice invokes; it does not claim every generated 4.6
pointer is present. It then verifies the actual major/minor version, Core profile
mask, required diagnostic strings, double buffering, and sRGB capability. An
older, compatibility-profile, single-buffered, or non-sRGB context fails
explicitly.

The supported build vendors only GLAD's generated `gl.h`, `khrplatform.h`, and
`gl.c`, plus upstream licensing and a provenance record. This is a narrow
exception to the normal build-tree-only dependency rule: it removes Python and
Jinja from supported builds while retaining exact generator arguments, archive
and output hashes, and configure-time verification. See
[`Engine/third_party/glad/README.md`](../../Engine/third_party/glad/README.md).

## Deferred decisions

Both applications directly compose OpenGL in this release. Vulkan 1.3 and
explicit `--renderer=opengl|vulkan|auto` behavior are reserved for `v0.1.6` so
selection/fallback tests cannot disguise a missing backend. No common mesh or
resource abstraction will be added until the same narrowly specified content
needs it in both implementations. `.gexworld` loading and terrain/validity
visualisation remain separate later slices.
