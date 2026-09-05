# Building

## Windows prerequisites

- Visual Studio 2022 Desktop development with C++
- CMake 3.25 or newer
- Network access for the pinned SDL3 download during first configure
- Git for cloning and contributing to the repository
- Doxygen 1.9 or newer for API documentation

SDL3 `3.4.14` is pinned by URL and SHA-256 in `Engine/CMakeLists.txt`. It is downloaded only into the ignored CMake build tree. Set `GAMEEX_ENGINE_USE_SYSTEM_SDL3=ON` only when an SDL3 CMake package is already installed.

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

## Documentation and tests

```powershell
ctest --preset debug
cmake --build --preset docs
```

Use `ctest --test-dir build/vs2022 -C Debug -L unit` to run only non-GUI unit tests. GUI smoke tests briefly display each real desktop window.

## JobSystem configuration

[ADR 0008](../decisions/0008-bounded-jobs-controlled-parallel-startup.md) adds no build prerequisite or CMake option. Worker count is runtime configuration, independent of CMake's build parallelism. The current application uses the conservative hardware-based recommendation; tests pass an explicit count. No public command-line worker-count option is exposed yet.
