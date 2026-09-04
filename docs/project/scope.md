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

## Version 0.1.1 decisions

The project remains in one repository during early development. The editor remains one native application window, with any future panels kept inside it until a UI workflow proves that another window is necessary. The startup graph and job system precede renderer implementation. The renderer will expose one shared Render API/RHI implemented by both OpenGL and Vulkan backends, as recorded in [ADR 0004](../decisions/0004-dual-renderer-backends.md).

## Explicitly out of scope

Version `0.1.1` does not implement:

- the shared Render API, OpenGL context/rendering path, or Vulkan instance/device/swapchain path;
- an immediate-mode or retained-mode editor UI toolkit;
- the startup DAG or job system;
- ECS, resources, audio, input mapping, serialization, or virtual filesystems;
- terrain, TESSERA, DMR, RÚIAN, ZABAGED, Dynamic World, or other ingestion;
- a serialized `.exworld` layout;
- transport, population, economic, construction, or land-use simulation;
- save files, networking, modding, scripting, or asset formats;
- Git submodule/repository splitting of Engine, WorldCompiler, and Game.

## Next specification gate

Development can implement the agreed startup and job slices. Before renderer or real-data implementation commits to durable interfaces or dependencies, the owner still needs to confirm:

1. the minimum OpenGL version/profile and Vulkan API baseline;
2. explicit backend selection, default/fallback behaviour, and when feature parity is required;
3. whether the Vulkan SDK and future geospatial dependencies should be installed system-wide, managed by a package manager, or built as pinned project dependencies;
4. whether the compact Bystřice pod Hostýnem dataset may be committed to normal Git, whether the large source LAS belongs in Git LFS, and which files must remain external;
5. the TESSERA product/year and whether its first editor output is a PCA diagnostic, semantic classification, material weights, or another defined product;
6. source precedence, confidence, time/epoch, overlap, and no-data rules before any layers are combined;
7. the UI toolkit only when the full-window inspection canvas grows into docked editor panels.
