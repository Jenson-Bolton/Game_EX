# Game_EX

Game_EX is a new C++20 foundation for a large-world transport, city, and land-use simulation. It is intentionally split into a reusable engine, an offline world compiler/runtime world format, and the game applications.

The current `v0.1.5` foundation combines deterministic startup/jobs infrastructure, the first portable world-data contract, and the first concrete renderer. The game and world editor each create an SDL3 window through the same shared Render API, verify an OpenGL 4.6 Core context, and present the same sRGB diagnostic clear frame. The dependency-free WorldCompiler still validates a strict staged terrain manifest, writes a deterministic `.gexworld` package, and reopens it through the runtime-only `GameEX::WorldFormat` reader. The committed terrain fixture remains synthetic and neither application visualises it yet. Vulkan 1.3 and explicit renderer selection are the next backend slice, preserving the accepted [cross-technology direction](docs/decisions/0004-dual-renderer-backends.md).

## Repository layout

```text
Game_EX/
|-- Engine/          Reusable engine, SDL3 platform, Render API, and OpenGL backend
|-- WorldCompiler/   Runtime world format and offline compiler
|-- Game/            Game and world-editor applications
|-- docs/            Project, architecture, section, and development docs
|-- CMakeLists.txt   Local workspace build
`-- CMakePresets.json
```

The three source directories are independent CMake projects. The root project is a lightweight local workspace that composes them. See [repository boundaries](docs/architecture/repository-boundaries.md) before turning them into separate Git repositories or submodules.

## Build on Windows

Prerequisites are Visual Studio 2022 with Desktop C++, CMake 3.25 or newer, Git, and a driver exposing OpenGL 4.6 Core with an sRGB-capable default framebuffer. The first configure downloads the pinned SDL3 source release into the ignored build tree. The minimal generated GLAD 2.0.8 loader is audited and vendored, so supported builds do not require Python or Jinja.

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
