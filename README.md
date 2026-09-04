# Game_EX

Game_EX is a new C++20 foundation for a large-world transport, city, and land-use simulation. It is intentionally split into a reusable engine, an offline world compiler/runtime world format, and the game applications.

The `v0.1.0` foundation is deliberately small: the workspace builds, the game and world editor each open an SDL3 desktop window, the offline compiler has a safe placeholder entry point, and both project documentation and generated API documentation have defined homes. No renderer, simulation, editor workflow, or production world-data pipeline was included in that baseline. The accepted renderer direction is one shared Render API with required [OpenGL and Vulkan backends](docs/decisions/0004-dual-renderer-backends.md).

## Repository layout

```text
Game_EX/
|-- Engine/          Reusable engine and SDL3 platform backend
|-- WorldCompiler/   Runtime world format and offline compiler
|-- Game/            Game and world-editor applications
|-- docs/            Project, architecture, section, and development docs
|-- CMakeLists.txt   Local workspace build
`-- CMakePresets.json
```

The three source directories are independent CMake projects. The root project is a lightweight local workspace that composes them. See [repository boundaries](docs/architecture/repository-boundaries.md) before turning them into separate Git repositories or submodules.

## Build on Windows

Prerequisites are Visual Studio 2022 with Desktop C++, CMake 3.25 or newer, and Git. The first configure downloads the pinned SDL3 source release into the ignored build tree.

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
- [Building and testing](docs/development/building.md)
- [Working agreement and release workflow](docs/development/README.md)
- [Doxygen policy](docs/development/doxygen.md)
- [Architecture decisions](docs/decisions/README.md)
- [Version history and technical reports](docs/history/README.md)
