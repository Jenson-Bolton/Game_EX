# Current scope and specification gate

## Foundation milestone

The current milestone includes:

- one root workspace build and three independently configurable CMake projects;
- a C++20 `GameEX::Core` application lifetime;
- a validated, deterministic serial `GameEX::Startup` subsystem lifecycle;
- a platform-neutral window boundary and SDL3 implementation;
- separate game and world-editor executables that open, pump events, and close cleanly;
- a minimal `GameEX::WorldFormat` compatibility boundary;
- a world-compiler executable that honestly reports its unconfigured state;
- unit and GUI smoke tests;
- human-authored project/architecture/section/development documentation;
- strict Doxygen generation for all current C++ code;
- linkage to the existing Game_EX GitHub history.

## Current accepted decisions

The project remains in one repository through dependency-light WorldCompiler work, with a mandatory split decision before supported geospatial, acquisition, Python, or ML dependencies are added. The editor remains one native application window, with future panels kept inside it until a UI workflow proves that another window is necessary. The implemented serial startup graph precedes the bounded job system and controlled parallel startup.

The renderer will expose one shared Render API/RHI implemented by OpenGL 4.6 Core and Vulkan 1.3 backends, as recorded in [ADR 0004](../decisions/0004-dual-renderer-backends.md). Once renderer composition is implemented, applications will accept `--renderer=opengl|vulkan|auto`; `auto` will try Vulkan before OpenGL, while automated tests will select a backend explicitly. Backends may arrive in separately reported patch releases, but `v0.2.0` requires parity for its claimed diagnostic/editor capability. The planned toolchain uses a pinned GLAD input and the system LunarG Vulkan SDK.

The [Czech Republic data-source strategy](data-source-strategy.md) defines source authority, conflict handling, EPSG:5514 canonical coordinates, tile-local rendering precision, provenance, and inspect-before-combine staging. The first proof will use the Bystřice region, DMR 5G terrain, separately visible authoritative/supplemental vectors, Dynamic World probabilities, and an offline 2024 TESSERA experiment. Machine-learned semantics will never displace applicable authoritative geometry.

## Explicitly out of scope

Version `0.1.2` does not implement:

- the shared Render API, OpenGL context/rendering path, or Vulkan instance/device/swapchain path;
- an immediate-mode or retained-mode editor UI toolkit;
- the job system or controlled parallel startup;
- ECS, resources, audio, input mapping, serialization, or virtual filesystems;
- terrain, TESSERA, DMR, RÚIAN, ZABAGED, Dynamic World, or other ingestion;
- a serialized `.exworld` layout;
- transport, population, economic, construction, or land-use simulation;
- save files, networking, modding, scripting, or asset formats;
- Git submodule/repository splitting of Engine, WorldCompiler, and Game.

## Next specification gate

Development can implement the agreed job, portable compiler, renderer, and separate-layer viewer slices. The remaining gates are narrower:

1. resolve the WorldCompiler repository split immediately before adding supported GDAL/PROJ, Python, network-acquisition, or ML dependencies;
2. verify the exact licence, edition, acquisition record, checksum, coordinate metadata, and redistribution status of every real source before committing or compiling it;
3. keep the large source LAS outside ordinary Git and decide between an external immutable cache, data release, or Git LFS only if a reproducible source build requires repository-managed storage;
4. specify the UI toolkit only when the full-window inspection canvas grows into docked editor panels;
5. require a separate reconciliation decision and evidence report before independently inspected layers are combined.
