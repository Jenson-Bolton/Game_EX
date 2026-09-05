# Building

## Supported Windows prerequisites

- Visual Studio 2022 Desktop development with C++
- CMake 3.25 or newer
- Git
- Doxygen 1.9 or newer for API documentation
- an OpenGL 4.6 Core driver with double-buffered sRGB support
- a LunarG Vulkan SDK with headers/import library, plus a Vulkan 1.3 loader and
  device, unless Vulkan is explicitly disabled
- network access for the pinned SDL3 download on first configure

SDL3 3.4.14 remains URL/hash pinned in `Engine/CMakeLists.txt`. The audited GLAD
2.0.8 core loader remains vendored with its existing configure-time hashes and
LF controls. Vulkan uses the system LunarG SDK; this clear-only slice does not
discover or require `glslc`, `glslangValidator`, shaders, or SPIR-V.

## Vulkan SDK discovery

Vulkan is enabled by default. On Windows the Engine selects the first valid
source in this exact order:

1. `-DGAMEEX_VULKAN_SDK_ROOT=<path>`; an invalid explicit path fails immediately.
2. `VULKAN_SDK`; an invalid non-empty environment path fails immediately.
3. the highest naturally version-sorted valid directory under `C:/VulkanSDK`.

A candidate is valid only when `Include/vulkan/vulkan.h` and
`Lib/vulkan-1.lib` exist. The selected paths seed CMake's `Vulkan_INCLUDE_DIR`
and `Vulkan_LIBRARY` before `find_package(Vulkan 1.3 REQUIRED)`. Current
automated evidence covers Windows/MSVC only; non-Windows SDK discovery and
runtime testing are not claimed.

To build without Vulkan:

```powershell
cmake -S . -B build/no-vulkan -A x64 -DGAMEEX_ENABLE_VULKAN=OFF
cmake --build build/no-vulkan --config Debug --parallel 4
```

The OpenGL target and apps remain available. Explicit `--renderer=vulkan` then
fails with a clear compiled-out diagnostic; `auto` may use OpenGL.

## Configure and build the workspace

With a conventional SDK such as `C:/VulkanSDK/1.4.357.0`:

```powershell
cmake --preset vs2022
cmake --build --preset debug
ctest --preset debug
```

An explicit equivalent is:

```powershell
cmake -S . -B build/vs2022 -A x64 -DGAMEEX_VULKAN_SDK_ROOT=C:/VulkanSDK/1.4.357.0
cmake --build build/vs2022 --config Debug --parallel 4
ctest --test-dir build/vs2022 -C Debug --output-on-failure
```

## Run game and editor

Renderer omission means Vulkan-first auto selection:

```powershell
.\build\vs2022\bin\Debug\game_ex.exe
.\build\vs2022\bin\Debug\game_ex_world_editor.exe
```

Use an explicit backend when diagnosing or testing it:

```powershell
.\build\vs2022\bin\Debug\game_ex.exe --renderer=vulkan
.\build\vs2022\bin\Debug\game_ex.exe --renderer=opengl
.\build\vs2022\bin\Debug\game_ex_world_editor.exe --renderer=vulkan
.\build\vs2022\bin\Debug\game_ex_world_editor.exe --renderer=opengl
```

`--renderer=auto` is the explicit spelling of the default. The internal
`--quit-after-ms=<unsigned integer>` automation option may appear before or
after `--renderer`. Unknown, empty, repeated, or conflicting options fail.

## Standalone Engine

```powershell
cmake -S Engine -B build/engine -A x64 -DGAMEEX_VULKAN_SDK_ROOT=C:/VulkanSDK/1.4.357.0
cmake --build build/engine --config Debug --parallel 4
ctest --test-dir build/engine -C Debug --output-on-failure
```

`GAMEEX_ENABLE_GUI_SMOKE_TESTS=OFF` omits real window tests. It does not remove
renderer libraries. `GAMEEX_ENABLE_VULKAN=OFF` removes `GameEX::RenderVulkan`.

## Standalone WorldCompiler

WorldCompiler remains independent of SDL, OpenGL, Vulkan, the Vulkan SDK, GDAL,
PROJ, Python, and network libraries:

```powershell
cmake -S WorldCompiler -B build/world-compiler -A x64
cmake --build build/world-compiler --config Debug --parallel 4
ctest --test-dir build/world-compiler -C Debug --output-on-failure
```

Its shortest synthetic workflow remains:

```powershell
$compiler = '.\build\vs2022\bin\Debug\game_ex_world_compiler.exe'
& $compiler validate --manifest .\WorldCompiler\tests\fixtures\minimal\manifest.gexstage
& $compiler compile --manifest .\WorldCompiler\tests\fixtures\minimal\manifest.gexstage --output .\build\vs2022\minimal.gexworld
& $compiler inspect --package .\build\vs2022\minimal.gexworld
```

The compiler refuses to overwrite an output. See
[`WorldCompiler/README.md`](../../WorldCompiler/README.md) for the complete
manifest and command contract.

## Documentation

```powershell
cmake --build --preset docs
```

Doxygen treats warnings as errors and covers public/private Engine and Game
sources plus authored tests; dependency, generated-build, and vendored GLAD
headers remain excluded.
