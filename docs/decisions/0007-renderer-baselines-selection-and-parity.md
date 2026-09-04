# ADR 0007: Renderer baselines, selection, and parity

- Status: Accepted
- Date: 2026-09-05
- Owners: Jenson Bolton and Game_EX implementers

## Context

[ADR 0004](0004-dual-renderer-backends.md) requires separate OpenGL and Vulkan implementations of one shared Render API but deliberately left version baselines, application selection, fallback, toolchain, and release parity as specification gates. Those gates must close before either backend shapes window creation, build dependencies, tests, or user-facing commands.

## Decision

The OpenGL backend targets OpenGL 4.6 Core Profile. The Vulkan backend targets the Vulkan 1.3 API baseline. These are implementation baselines rather than claims that every optional feature of either specification will be used.

Both desktop applications accept exactly these renderer selections:

```text
--renderer=opengl
--renderer=vulkan
--renderer=auto
```

An explicit selection either starts the requested backend or returns a clear non-zero diagnostic; it never silently changes APIs. `auto` tries Vulkan first and may fall back once to OpenGL when Vulkan cannot initialise before application work starts. The diagnostic records both the preferred-backend failure and selected fallback. Unknown or conflicting values are command-line errors.

Automated backend tests always select `opengl` or `vulkan` explicitly. This makes a missing SDK/runtime, backend regression, or unexpected fallback visible. `auto` receives separate policy tests and is not evidence that both implementations work.

Backend foundations may be delivered as separate patch releases so their API-specific lifetime and diagnostics can be reviewed independently. `v0.2.0` may claim an integrated cross-technology capability only when the OpenGL and Vulkan paths both render the same agreed diagnostic/editor data through the shared API. Intentional visual or feature differences must be listed; an unimplemented backend cannot be hidden behind fallback.

The supported Windows development build uses the system LunarG Vulkan SDK for headers, import libraries, validation tooling, and shader compilation. OpenGL procedure loading uses a version-pinned GLAD input whose version and integrity are recorded by the build. Runtime redistributable requirements remain separate from developer SDK requirements.

## Consequences

Window creation must know the selected graphics API before creating the native window, while game and editor feature code remains backend-neutral. Renderer factories and diagnostics must preserve explicit failure causes. CI and developer verification require two named rendering checks rather than a single default launch.

OpenGL 4.6 excludes older hardware and drivers that expose only earlier core profiles; Vulkan 1.3 similarly establishes a real compatibility floor. A later baseline change requires a superseding ADR and capability evidence. Installing the Vulkan SDK is an external developer-machine action and must be documented separately from normal source configuration.

## Alternatives considered

A lowest-common-denominator OpenGL/Vulkan feature set would make neither baseline explicit. Silent fallback for an explicit backend would produce false test confidence. Requiring both backends in every patch would make small, reviewable implementation slices harder, while deferring parity beyond `v0.2.0` would contradict the first integrated cross-technology milestone.
