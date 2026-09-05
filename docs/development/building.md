# Building

## Windows prerequisites

- Visual Studio 2022 Desktop development with C++
- CMake 3.25 or newer
- Network access for the pinned SDL3 download during first configure
- Git for cloning and contributing to the repository
- Doxygen 1.9 or newer for API documentation
- an OpenGL 4.6 Core driver with a double-buffered sRGB-capable default framebuffer for desktop applications and GUI smoke tests

SDL3 `3.4.14` is pinned by URL and SHA-256 in `Engine/CMakeLists.txt`. It is downloaded only into the ignored CMake build tree. Set `GAMEEX_ENGINE_USE_SYSTEM_SDL3=ON` only when an SDL3 CMake package is already installed. The generated GLAD 2.0.8 core-4.6/no-extension loader is vendored with exact hashes and has no build-time Python/Jinja requirement.

## Configure and build the workspace

```powershell
cmake --preset vs2022
cmake --build --preset debug
```

Executables are placed in `build/vs2022/bin/Debug` for a Debug build.

## Run

```powershell
.\build\vs2022\bin\Debug\game_ex.exe
.\build\vs2022\bin\Debug\game_ex_world_editor.exe
.\build\vs2022\bin\Debug\game_ex_world_compiler.exe
```

`--quit-after-ms=<unsigned integer>` is reserved for automated window smoke tests.
Both desktop applications directly select OpenGL in `v0.1.5`; the documented `--renderer` policy is not exposed until the Vulkan backend exists.

The compiler requires a subcommand. Its shortest complete synthetic workflow is:

```powershell
$compiler = '.\build\vs2022\bin\Debug\game_ex_world_compiler.exe'
& $compiler validate --manifest .\WorldCompiler\tests\fixtures\minimal\manifest.gexstage
& $compiler compile --manifest .\WorldCompiler\tests\fixtures\minimal\manifest.gexstage --output .\build\vs2022\minimal.gexworld
& $compiler inspect --package .\build\vs2022\minimal.gexworld
```

Remove or rename the output before repeating `compile`; refusing overwrite is part of the compiler contract. The full standalone build and command reference is in the [WorldCompiler README](../../WorldCompiler/README.md).

## Build WorldCompiler independently

The format and compiler remain independently configurable and have no SDL, OpenGL, Vulkan, GDAL, PROJ, Python, or network dependency:

```powershell
cmake -S .\WorldCompiler -B .\build\world-compiler -A x64
cmake --build .\build\world-compiler --config Debug
ctest --test-dir .\build\world-compiler -C Debug --output-on-failure
```

## Build Engine independently

The Engine project includes the public Render API, SDL platform, OpenGL backend,
and fake-backed tests. GUI registration is optional for headless verification:

```powershell
cmake -S .\Engine -B .\build\engine -A x64 -DGAMEEX_ENABLE_GUI_SMOKE_TESTS=ON
cmake --build .\build\engine --config Debug --parallel 4
ctest --test-dir .\build\engine -C Debug --output-on-failure
```

Set `GAMEEX_ENABLE_GUI_SMOKE_TESTS=OFF` to omit the real OpenGL window test. This
does not remove `GameEX::RenderOpenGL` from the build.

## Documentation and tests

```powershell
ctest --preset debug
cmake --build --preset docs
```

Use `ctest --test-dir build/vs2022 -C Debug -L unit` to run dependency-light tests. GUI smoke tests briefly display the backend test, game, and editor windows and require a real OpenGL 4.6 Core context.

## JobSystem configuration

[ADR 0008](../decisions/0008-bounded-jobs-controlled-parallel-startup.md) adds no build prerequisite or CMake option. Worker count is runtime configuration, independent of CMake's build parallelism. The current application uses the conservative hardware-based recommendation; tests pass an explicit count. No public command-line worker-count option is exposed yet.
