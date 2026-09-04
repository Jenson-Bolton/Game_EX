# Game_EX

Game_EX is a new C++20 foundation for a large-world transport, city, and land-use simulation. It is intentionally split into a reusable engine, an offline world compiler/runtime world format, and the game applications.

The current milestone is deliberately small: the workspace builds, the game and world editor each open an SDL3 desktop window, the offline compiler has a safe placeholder entry point, and both project documentation and generated API documentation have defined homes. No renderer, simulation, editor workflow, or world-data pipeline has been selected or implemented yet.

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

## Documentation map

- [Documentation index](docs/index.md)
- [Project vision](docs/project/vision.md)
- [Current scope and specification gate](docs/project/scope.md)
- [Architecture overview](docs/architecture/overview.md)
- [Building and testing](docs/development/building.md)
- [Doxygen policy](docs/development/doxygen.md)
- [Architecture decisions](docs/decisions/README.md)
