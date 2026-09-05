# ADR 0011: Vulkan transfer-clear backend and renderer selection

- Status: Accepted
- Date: 2026-09-05
- Owners: Jenson Bolton and Game_EX implementers

## Context

[ADR 0004](0004-dual-renderer-backends.md) requires real OpenGL and Vulkan
implementations behind one shared API. [ADR 0007](0007-renderer-baselines-selection-and-parity.md)
fixes Vulkan 1.3, Vulkan-first `auto`, explicit no-fallback behaviour, and
backend-specific test evidence. `v0.1.5` supplied the deliberately narrow
`DiagnosticFrame` API and OpenGL 4.6 Core implementation; the Vulkan half must
prove the same colour semantics and ownership boundary without inventing an
untested resource or shader API.

Vulkan also makes several hidden policies unavoidable: instance/device feature
requirements, surface and queue selection, swapchain replacement, layout
transitions, synchronization, validation, and loader-layer isolation. Those
choices must be deterministic and must not expose SDL or Vulkan types in public
common headers.

## Decision

Add `GameEX::RenderVulkan` as a concrete private-dependency implementation of
`GameEX::Render`. Its public entry point contains only ordinary C++ configuration
and returns `RendererFactory`; Vulkan, SDL, and the checked native-window bridge
remain implementation details. Game, editor, Core, Render, Platform, and
WorldFormat public headers contain no native SDL/OpenGL/Vulkan types.

The backend requests Vulkan 1.3 and requires a loader and physical device at or
above 1.3, `synchronization2`, `VK_KHR_swapchain`, graphics and presentation
queues, FIFO presentation, transfer-destination swapchain usage, and an sRGB
surface format whose optimal tiling features permit transfer destination. It
accepts only `VK_FORMAT_B8G8R8A8_SRGB` then `VK_FORMAT_R8G8B8A8_SRGB`, both with
`VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`; the single `VK_FORMAT_UNDEFINED` convention
resolves to the first choice. It never falls back to a non-sRGB format.

Physical devices are ranked deterministically by suitability, device class
(discrete, integrated, virtual, CPU, other), UUID, vendor/device identifiers,
name, then source index. Queue selection prefers the lowest combined
graphics/present family, otherwise the lowest separate pair. Separate queues
use concurrent swapchain sharing. Extent uses a fixed surface extent when
provided, otherwise the SDL drawable pixel extent clamped to capability bounds.
Image count is `minImageCount + 1` where permitted, composite alpha follows a
fixed preference beginning with opaque, and present mode is always FIFO.

The backend owns one command buffer and one frame fence. It uses
`vkCmdPipelineBarrier2` to transition an acquired image to
`TRANSFER_DST_OPTIMAL`, clears it with the shared linear float32
`foundation_diagnostic_frame()`, transitions to `PRESENT_SRC_KHR`, submits with
`vkQueueSubmit2`, and presents. A binary present-wait semaphore belongs to each
swapchain image, avoiding reuse before that image is acquired again. The one
image-available semaphore is reused only after the frame fence proves its
submission consumed the signal.

Swapchain creation is transactional: a replacement and all its per-image
semaphores must be complete before the active generation is exchanged. A zero
drawable extent returns a typed deferral rather than failing. Out-of-date or
suboptimal acquisition/presentation marks the generation dirty; an unresolved
surface change may return its own deferral. Runtime recreation failures are
always `presentation_failed`, never an auto-fallback category. The baseline
uses conventional `vkDeviceWaitIdle` recreation. Core Vulkan/KHR_swapchain does
not provide a fully signalled WSI retirement point for all old presentation
resources; `VK_EXT_swapchain_maintenance1` presentation fences are recorded as
a future improvement rather than becoming an extra v0.1.6 requirement.

Public input validation and typed deferrals are retryable. Once either backend
enters native frame work, any exception marks that Renderer `failed` before it
is rethrown. This prevents reuse of a possibly consumed binary semaphore,
unsignalled fence, partial command buffer, or uncertain OpenGL context state.
The failed renderer remains valid only for creator-thread shutdown/rollback.

Renderer startup and a hidden presentation attempt precede window visibility.
The attempt may defer at zero extent; visibility may then proceed and the loop
must obtain a real presentation. Automated timed runs fail with
`presentation_failed` if no frame was actually presented. This is presentation
evidence, not a captured-pixel or pixel-equality claim.

The desktop grammar accepts one optional `--renderer=opengl|vulkan|auto` and one
optional `--quit-after-ms=<unsigned integer>` in either order. Omission means
`auto`. Explicit OpenGL or Vulkan makes exactly one attempt and never falls
back. Auto creates a completely fresh Platform, Window, Renderer, and
Application for each attempt. It tries Vulkan first and falls back once to
OpenGL only for a Vulkan `unavailable` or `initialization_failed` error before
the first window became visible. The preferred error category, stage, and text
are logged before the fallback. Presentation, shutdown, post-visibility, wrong
backend, and non-renderer failures never fall back. Compiled-out Vulkan reports
`unavailable`, so explicit Vulkan fails clearly and auto may use OpenGL.

Vulkan is enabled by default with `GAMEEX_ENABLE_VULKAN=ON`. The supported
Windows discovery order is an explicit, fail-fast `GAMEEX_VULKAN_SDK_ROOT`, then
`VULKAN_SDK`, then the highest naturally sorted valid `C:/VulkanSDK/*`
installation. A valid SDK must contain `Include/vulkan/vulkan.h` and
`Lib/vulkan-1.lib`. No shader compiler is required because this slice has no
shaders. `GAMEEX_ENABLE_VULKAN=OFF` preserves an OpenGL-only build.

Validation configuration is disabled, optional, or required at the Vulkan
factory boundary. The dedicated smoke requires `VK_LAYER_KHRONOS_validation`,
installs a debug-utils callback, performs at least 32 visible presentations,
and fails on any error-severity message. Tests set `VK_LAYER_PATH` to the
selected SDK only when validation is required and replace implicit-layer
discovery with an existing empty directory; inherited layer controls are
cleared and implicit layers disabled. This isolates engine evidence from broken
or injected host overlays without changing normal application policy.

## Consequences

Both desktop applications now exercise the same exact linear clear through
real OpenGL and Vulkan APIs. Khronos defines float clear values as linear when
the destination format is sRGB, matching the existing OpenGL sRGB framebuffer
contract. Successful native presentation and matching semantic input are
claimed; cross-driver pixel equality is not.

The public Render API grows only with presentation outcomes and neutral
diagnostics. No shaders, pipelines, descriptor sets, buffers, images, meshes,
or command abstractions are implied. That keeps the next terrain-view slice
free to introduce only the resources proven necessary by both backends.

The default build now requires a Windows Vulkan SDK even though runtime loading
uses the system loader and driver. Developers without it must opt out. The
Vulkan target adds significant API-specific code and validation cost, but its
dependencies remain private and its deterministic policy is separately tested.

## Alternatives considered

A shader/full-screen-triangle clear would introduce shader tools, binaries, and
pipeline/resource choices without testing useful geometry. `vkCmdClearColorImage`
is the smallest real presentation path. One global render-finished semaphore is
not safe because a submission fence does not prove the presentation engine has
finished waiting on it; per-image semaphores avoid that invalid reuse. Requiring
swapchain-maintenance presentation fences now would narrow otherwise valid
Vulkan 1.3 devices. Mailbox/immediate presentation would weaken deterministic
baseline behaviour. Accepting arbitrary formats would break the shared linear
colour contract. Reusing a failed Vulkan Platform/Application for OpenGL would
mix incompatible native window flags and partial state. Catching every auto
failure would conceal real runtime defects.
