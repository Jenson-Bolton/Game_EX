# Architecture overview

The source tree is divided by dependency and responsibility, not by a generic collection of “managers.”

```text
game_ex -----------------------+
                               |
game_ex_world_editor ----------+--> GameEX::GameApp
                                      |       |
                                      |       +--> GameEX::WorldFormat
                                      |
                                      +--> GameEX::Core --> GameEX::Startup --> GameEX::Jobs
                                      +--> GameEX::PlatformSDL --> SDL3

game_ex_world_compiler --> GameEX::WorldCompilerCore --> GameEX::WorldFormat
```

`GameEX::Core` sees only the abstract `Platform` and `Window` contracts. Its owned `GameEX::Startup` graph validates and runs ordinary subsystem objects without exposing global lookup. `GameEX::PlatformSDL` implements the platform contracts and is the only current target that includes SDL headers. The game and editor composition roots select SDL3, then transfer ownership into `Application`.

`GameEX::WorldFormat` owns the small, runtime-safe logical model and verified `.gexworld` reader. It has no dependency on compiler implementation. `GameEX::WorldCompilerCore` validates a strict normalised stage and uses an internal encoder; the CLI is only an offline composition root. The first concrete package is explicitly encoded rather than written from C++ object memory. Its exact contract is documented in the [world package format](world-package-format.md).

`GameEX::Jobs` is an ordinarily owned synchronous batch service injected into controlled parallel startup; it is not a service locator or a general asynchronous system. The next engine shape is expected to add a shared Render API/RHI and separate OpenGL and Vulkan backends, then let the editor load the current WorldFormat package for a diagnostic terrain view. Resources, ECS, simulation, input, audio, and broader world streaming remain directions rather than implemented claims.

## Dependency rules

- Engine modules never depend on game, editor, or Czech Republic domain code.
- Game and editor never include SDL, OpenGL, Vulkan, or native operating-system headers.
- OpenGL and Vulkan dependencies remain private to their respective renderer backends and platform/presentation integration targets.
- World compiler implementation is offline; only WorldFormat is a runtime dependency.
- Platform exposes low-level capabilities. Higher layers own assets, saves, and domain workflows.
- Ownership is explicit and lifetimes are deterministic; service-locator singletons are prohibited.
