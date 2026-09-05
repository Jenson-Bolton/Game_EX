# Architecture overview

The source tree is divided by dependency and responsibility, not by a generic collection of “managers.”

```text
game_ex -----------------------------> GameEX::GameApp
                                            +--> GameEX::Core --> GameEX::Startup --> GameEX::Jobs
                                            |       |
                                            |       +--> GameEX::Render --> GameEX::Platform
                                            +--> GameEX::RenderOpenGL --> GameEX::Render
                                            |       +-- private --> SDL3 + generated GLAD
                                            +--> GameEX::RenderVulkan --> GameEX::Render
                                            |       +-- private --> SDL3 + Vulkan SDK/loader
                                            +--> GameEX::PlatformSDL --> GameEX::Platform
                                                    +-- private --> SDL3

game_ex_world_editor --> GameEX::WorldEditorApp --> GameEX::GameApp
                               |
                               +--> GameEX::WorldFormat

game_ex_world_compiler --> GameEX::WorldCompilerCore --> GameEX::WorldFormat
```

`GameEX::Core` sees only the abstract Platform, Window, Renderer, and
RendererFactory contracts. Its owned `GameEX::Startup` graph validates and runs
ordinary subsystem objects without global lookup. `GameEX::PlatformSDL`
implements platform contracts. `GameEX::RenderOpenGL` and
`GameEX::RenderVulkan` implement the shared API through the checked private SDL
bridge; no SDL/OpenGL/Vulkan type crosses a common public header. The shared
desktop composition helper selects concrete targets and transfers ordinary
ownership plus an immutable neutral frame into `Application`.

`GameEX::WorldFormat` owns the small, runtime-safe logical model and verified `.gexworld` reader. It has no dependency on compiler implementation. `GameEX::WorldCompilerCore` validates a strict normalised stage and uses an internal encoder; the CLI is only an offline composition root. The first concrete package is explicitly encoded rather than written from C++ object memory. Its exact contract is documented in the [world package format](world-package-format.md).

`GameEX::Jobs` is an ordinarily owned synchronous batch service injected into
controlled parallel startup; it is not a service locator or a general
asynchronous system. The shared Render API now drives real OpenGL 4.6 Core and
Vulkan 1.3 backends, strict explicit/automatic selection, and the same semantic
frame contract. `GameEX::WorldEditorApp` now verifies one package before native
startup and maps height/validity to an aspect-correct bounded raster; the player
executable does not link that interpretation. Scalable resources, shaders,
meshes, interactive views, ECS, simulation, input, audio, and broader world
streaming remain directions rather than implemented claims.

## Dependency rules

- Engine modules never depend on game, editor, or Czech Republic domain code.
- Game and editor never include SDL, OpenGL, Vulkan, or native operating-system headers.
- OpenGL and Vulkan dependencies remain private to their respective renderer backends and platform/presentation integration targets.
- World compiler implementation is offline; only the editor target currently
  links WorldFormat, and neither application links WorldCompilerCore.
- Platform exposes low-level capabilities. Higher layers own assets, saves, and domain workflows.
- Ownership is explicit and lifetimes are deterministic; service-locator singletons are prohibited.
