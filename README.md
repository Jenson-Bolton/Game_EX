# Game_EX

Game_EX is a new C++20 foundation for a large-world transport, city, and land-use simulation. It is intentionally split into a reusable engine, an offline world compiler/runtime world format, and the game applications.

The current `v0.1.6` foundation combines deterministic startup/jobs, the first
portable world-data contract, and real cross-technology rendering. The game and
world editor use the same backend-neutral Render API and exact linear diagnostic
frame through either OpenGL 4.6 Core or Vulkan 1.3. Omission or
`--renderer=auto` tries Vulkan first; explicit `opengl`/`vulkan` never silently
fall back. The dependency-free WorldCompiler still compiles and reopens one
strict synthetic terrain package. Neither application visualises it yet.

## Repository layout

```text
Game_EX/
|-- Engine/          Reusable engine, SDL3 platform, and OpenGL/Vulkan renderers
|-- WorldCompiler/   Runtime world format and offline compiler
|-- Game/            Game and world-editor applications
|-- docs/            Project, architecture, section, and development docs
|-- CMakeLists.txt   Local workspace build
`-- CMakePresets.json
```

The three source directories are independent CMake projects. The root project is a lightweight local workspace that composes them. See [repository boundaries](docs/architecture/repository-boundaries.md) before turning them into separate Git repositories or submodules.

## Build on Windows

Prerequisites are Visual Studio 2022 with Desktop C++, CMake 3.25 or newer, Git,
an OpenGL 4.6 Core sRGB-capable driver, and a LunarG Vulkan SDK plus Vulkan 1.3
loader/device. The SDK is found from `GAMEEX_VULKAN_SDK_ROOT`, `VULKAN_SDK`, or
the highest valid `C:/VulkanSDK/*` install. Use
`-DGAMEEX_ENABLE_VULKAN=OFF` for an OpenGL-only build. The first configure
downloads pinned SDL3 into the ignored build tree; audited GLAD remains vendored.

```powershell
cmake --preset vs2022
cmake --build --preset debug
ctest --preset debug
```

Run the applications:

```powershell
.\build\vs2022\bin\Debug\game_ex.exe
.\build\vs2022\bin\Debug\game_ex_world_editor.exe
```

Choose a backend explicitly when testing it:

```powershell
.\build\vs2022\bin\Debug\game_ex.exe --renderer=vulkan
.\build\vs2022\bin\Debug\game_ex_world_editor.exe --renderer=opengl
```

See [building](docs/development/building.md) for exact SDK discovery, standalone
Engine/WorldCompiler commands, validation isolation, and renderer options.

Exercise the portable compiler with its project-authored fixture:

```powershell
.\build\vs2022\bin\Debug\game_ex_world_compiler.exe validate --manifest .\WorldCompiler\tests\fixtures\minimal\manifest.gexstage
.\build\vs2022\bin\Debug\game_ex_world_compiler.exe compile --manifest .\WorldCompiler\tests\fixtures\minimal\manifest.gexstage --output .\build\vs2022\minimal.gexworld
.\build\vs2022\bin\Debug\game_ex_world_compiler.exe inspect --package .\build\vs2022\minimal.gexworld
```

The compiler refuses to overwrite an existing package. See the [WorldCompiler guide](WorldCompiler/README.md) for the manifest contract, stable exit codes, standalone build, and complete command reference.

Generate checked API documentation:

```powershell
cmake --build --preset docs
```

The entry page is then `build/vs2022/docs/doxygen/html/index.html`.

## How we work

Game_EX advances in small, reviewable versions. Each version has one agreed scope, one coherent release push, an annotated Git tag, a changelog entry, and a technical report containing the evidence used to accept it. Changes that select a dependency, persistent format, renderer boundary, coordinate system, data source, or cross-project dependency stop at a specification gate until the decision is agreed and recorded.

Before publishing a version, build all affected targets, run the relevant CTest suites, generate Doxygen with warnings treated as errors, and record data provenance where applicable. Public C++ contracts and non-obvious internals belong in Doxygen; the Markdown documentation explains project intent, architecture, workflow, and results without copying the API reference.

Read the [working agreement](docs/development/README.md), [contribution guide](CONTRIBUTING.md), [version history](docs/history/README.md), and [changelog](CHANGELOG.md) before starting a change.

## Documentation map

- [Documentation index](docs/index.md)
- [Project vision](docs/project/vision.md)
- [Current scope and specification gate](docs/project/scope.md)
- [Architecture overview](docs/architecture/overview.md)
- [Rendering architecture](docs/architecture/rendering.md)
- [World package format](docs/architecture/world-package-format.md)
- [WorldCompiler guide](WorldCompiler/README.md)
- [Building and testing](docs/development/building.md)
- [Working agreement and release workflow](docs/development/README.md)
- [Doxygen policy](docs/development/doxygen.md)
- [Architecture decisions](docs/decisions/README.md)
- [Version history and technical reports](docs/history/README.md)
- [Legacy Bystřice data audit](docs/history/legacy-bystrice-data-audit.md)
