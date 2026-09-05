# Architecture overview

The source tree is divided by dependency and responsibility, not by a generic collection of “managers.”

```text
game_ex -----------------------+
                               |
game_ex_world_editor ----------+--> GameEX::GameApp
                                      +--> GameEX::WorldFormat
                                      +--> GameEX::Core --> GameEX::Startup --> GameEX::Jobs
                                      |       |
                                      |       +--> GameEX::Render --> GameEX::Platform
                                      +--> GameEX::RenderOpenGL --> GameEX::Render
                                      |       +-- private --> SDL3 + generated GLAD
                                      +--> GameEX::PlatformSDL --> GameEX::Platform
                                              +-- private --> SDL3

game_ex_world_compiler --> GameEX::WorldCompilerCore --> GameEX::WorldFormat
```

`GameEX::Core` sees only the abstract Platform, Window, Renderer, and RendererFactory contracts. Its owned `GameEX::Startup` graph validates and runs ordinary subsystem objects without exposing global lookup. `GameEX::PlatformSDL` implements the platform contracts. `GameEX::RenderOpenGL` implements the shared Render API and uses a private checked bridge to SDL for context creation; neither SDL nor OpenGL types cross a public header. The shared game/editor composition root selects these concrete targets and transfers ordinary ownership into `Application`.

`GameEX::WorldFormat` owns the small, runtime-safe logical model and verified `.gexworld` reader. It has no dependency on compiler implementation. `GameEX::WorldCompilerCore` validates a strict normalised stage and uses an internal encoder; the CLI is only an offline composition root. The first concrete package is explicitly encoded rather than written from C++ object memory. Its exact contract is documented in the [world package format](world-package-format.md).

`GameEX::Jobs` is an ordinarily owned synchronous batch service injected into controlled parallel startup; it is not a service locator or a general asynchronous system. The first shared Render API and OpenGL 4.6 Core backend now clear and present one diagnostic frame in both applications. Vulkan 1.3 and explicit renderer-selection policy are next; terrain visualisation follows only after both backends can exercise the agreed boundary. Resources, shaders, meshes, ECS, simulation, input, audio, and broader world streaming remain directions rather than implemented claims.

## Dependency rules

- Engine modules never depend on game, editor, or Czech Republic domain code.
- Game and editor never include SDL, OpenGL, Vulkan, or native operating-system headers.
- OpenGL and Vulkan dependencies remain private to their respective renderer backends and platform/presentation integration targets.
- World compiler implementation is offline; only WorldFormat is a runtime dependency.
- Platform exposes low-level capabilities. Higher layers own assets, saves, and domain workflows.
- Ownership is explicit and lifetimes are deterministic; service-locator singletons are prohibited.
