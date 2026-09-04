# ADR 0004: OpenGL and Vulkan renderer backends

- Status: Accepted
- Date: 2026-09-05
- Owners: Jenson Bolton and Game_EX implementers

## Context

Game_EX is a cross-technology project and must implement both OpenGL and Vulkan. ADR 0002 already keeps SDL3 at the platform boundary and prevents SDL's rendering API from becoming the engine renderer. A dual-backend requirement now makes the shared rendering boundary explicit.

## Decision

Create one platform-neutral Render API used by the game and world editor, with separate OpenGL and Vulkan backend targets behind it. Public engine, game, editor, and world-format headers must not expose OpenGL, Vulkan, or SDL types. A private platform/presentation integration layer may connect an SDL window to an OpenGL context or Vulkan surface.

Both backends will consume the same extracted render-world data, resources, camera state, and diagnostic scenes. Backend selection belongs to application composition and configuration rather than game or editor feature code. SDL_Renderer is not a third backend.

The minimum OpenGL version/profile, Vulkan baseline, default backend, fallback behaviour, and per-version feature-parity policy remain specification gates. They must be agreed before renderer implementation.

## Consequences

Renderer work and tests must distinguish backend-neutral behaviour from API-specific behaviour. Shared fixtures and controlled captures will make output and capability differences visible. Each backend owns its native resource lifetime and diagnostics, while higher layers retain one rendering contract.

The two implementations increase build, test, packaging, and documentation work. A feature cannot be described as portable across both backends until both paths have been exercised and any intentional difference has been recorded.

## Alternatives considered

A Vulkan-only renderer no longer satisfies the project requirement. An OpenGL-only renderer has the same problem. SDL_Renderer would add a separate abstraction that neither demonstrates the required APIs nor fits the accepted engine boundary.
