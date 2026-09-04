# Current scope and specification gate

## Foundation milestone

The current milestone includes:

- one root workspace build and three independently configurable CMake projects;
- a C++20 `GameEX::Core` application lifetime;
- a platform-neutral window boundary and SDL3 implementation;
- separate game and world-editor executables that open, pump events, and close cleanly;
- a minimal `GameEX::WorldFormat` compatibility boundary;
- a world-compiler executable that honestly reports its unconfigured state;
- unit and GUI smoke tests;
- human-authored project/architecture/section/development documentation;
- strict Doxygen generation for all current C++ code;
- linkage to the existing Game_EX GitHub history.

## Explicitly out of scope

The milestone does not choose or implement:

- Vulkan instance/device/swapchain handling or another rendering backend;
- an immediate-mode or retained-mode editor UI toolkit;
- the startup DAG or job system;
- ECS, resources, audio, input mapping, serialization, or virtual filesystems;
- terrain, TESSERA, DMR, RÚIAN, ZABAGED, Dynamic World, or other ingestion;
- a serialized `.exworld` layout;
- transport, population, economic, construction, or land-use simulation;
- save files, networking, modding, scripting, or asset formats;
- Git submodule/repository splitting of Engine, WorldCompiler, and Game.

## Next specification gate

Development should pause after the foundation until the owner confirms at least:

1. whether the world editor is one multi-panel desktop application or requires multiple native windows;
2. whether Dear ImGui (or another UI approach) is acceptable for the first editor interface;
3. the first renderer milestone and the required Windows/Vulkan SDK baseline;
4. whether to implement the startup DAG/job system before rendering;
5. whether the three CMake projects should now become separate Git repositories/submodules or remain together during early development.
