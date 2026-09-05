# Application lifecycle

The application owns a fresh platform, hidden window, renderer, bounded worker
pool, and startup graph for one run. Renderer selection occurs outside this
object so each automatic attempt can be a completely new composition.

```text
parse --renderer (default auto)
        |
        v
create fresh SDL Platform
        |
        v
create hidden backend-capable Window and dormant Renderer
        |
        v
Application::run
  | validate startup graph
  | start renderer
  | attempt shared diagnostic presentation while hidden
  | show window (even if zero extent deferred the hidden attempt)
  | pump events, render, and briefly pace
  | timed smoke requires at least one real presentation
  | hide window
  ` shut down renderer
        |
        v
destroy Renderer, Window, then Platform/SDL
```

## Startup and shutdown ownership

`Application` registers two main-thread-affine subsystems:

```text
render.backend --> platform.window.visibility
```

The renderer subsystem starts native API state and makes one hidden presentation
attempt. `FramePresentationResult::deferred_zero_extent` is allowed because some
window systems do not provide drawable pixels before visibility. The dependent
visibility subsystem then calls `Window::show()` and records monotonic
`Application::reached_window_visibility()` evidence. The event loop keeps
presenting the same `foundation_diagnostic_frame()`. A timed run succeeds only
after a renderer reports `presented`; timer survival alone is not evidence.

Normal reverse shutdown hides the window before releasing graphics state. If a
runtime operation throws, the graph still attempts the same reverse cleanup and
preserves the original exception. A failing renderer start is itself rolled
back, allowing partial native handles to be released. Member order destroys the
renderer before its borrowed Window and the Window before its SDL Platform.

Platform initialisation, window creation/visibility, renderer lifecycle and
presentation, event pumping, and destruction remain on the composition thread.
Wrong-thread renderer destruction fails fast. Renderer and platform instances
are single-use; there is no global renderer, service locator, or reused native
state between automatic attempts.

## Renderer-selection attempt boundary

`--renderer=opengl` and `--renderer=vulkan` perform exactly one attempt. Omission
or `--renderer=auto` tries Vulkan first. An auto attempt may proceed to a new
OpenGL composition only when all of these are true:

- the failed attempt reports backend `vulkan`;
- the error code is `unavailable` or `initialization_failed`;
- `reached_window_visibility()` is false.

The Vulkan category/stage/message is logged before fallback. A renderer error
after visibility—including an escaped unavailable/initialization category—or a
presentation/shutdown failure is never caught as fallback. Non-renderer
exceptions also propagate. This makes a visible failure or a timed no-present
result observable instead of silently reopening another API.

If Vulkan is compiled out, creating its factory reports a pre-visibility
`unavailable` error. Explicit Vulkan therefore fails clearly; auto may create a
fresh OpenGL Platform/Application. A Vulkan window is never repurposed for
OpenGL because their SDL creation flags differ.

## Startup graph and jobs

`GameEX::Startup` pre-validates missing dependencies and cycles. Ready nodes use
stable lexical order. Normal shutdown exactly reverses successful startup; a
start failure first shuts down the potentially partial subsystem and then every
earlier subsystem in reverse order. Validation failures leave the graph
configurable, while runtime lifecycle failures are terminal.

`GameEX::Jobs` supplies the existing 1–32-worker synchronous batch primitive.
Main-thread-affine nodes remain on the owner. Worker-eligible lexical prefixes
may execute concurrently, but admission, result indexing, primary failure,
logical commit, and reverse rollback remain deterministic. Neither Jobs nor the
startup graph is a general frame scheduler.
