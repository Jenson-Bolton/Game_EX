# ADR 0002: SDL3 platform boundary

- Status: Accepted
- Date: 2026-09-04

## Context

The first executable milestone needs portable desktop windows. The earlier Game_EX direction selected SDL3, while the renderer is intended to support Vulkan behind its own RHI boundary.

## Decision

Use pinned SDL3 `3.4.14` to implement only platform initialisation, native window ownership, and event pumping. SDL headers remain private to `GameEX::PlatformSDL`. `GameEX::Core` uses platform-neutral interfaces, and game/editor code selects the backend only in composition code.

SDL's rendering API is not the Game_EX renderer. A future Vulkan dependency must remain private to a distinct Vulkan RHI target.

## Consequences

The initial windows are cross-platform in structure and do not contaminate game code with SDL types. The first configure requires fetching the pinned source release unless a system SDL3 package is explicitly selected. Windows remain blank until an RHI milestone is specified.

## Alternatives considered

Raw Win32 would avoid a dependency but establish the wrong portability path. Reusing local SDL2 or GLFW/OpenGL would contradict the accepted SDL3/Vulkan direction. Implementing Vulkan now would exceed the requested window foundation and available specification.
