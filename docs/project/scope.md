# Current scope and specification gate

## Current `v0.1.5` foundation milestone

The current milestone includes:

- one root workspace build and three independently configurable CMake projects;
- a C++20 `GameEX::Core` application lifetime;
- a validated, deterministic serial `GameEX::Startup` subsystem lifecycle;
- a bounded `GameEX::Jobs` worker pool and deterministic controlled-parallel startup path;
- platform-neutral window/render boundaries and an SDL3 implementation;
- a shared diagnostic Render API and verified OpenGL 4.6 Core backend;
- separate game and world-editor executables that present the same sRGB diagnostic clear, pump events, and close cleanly;
- a runtime-only `GameEX::WorldFormat` model and bounded `.gexworld` reader;
- a dependency-free `GameEX::WorldCompilerCore` with strict `.gexstage` validation and deterministic package encoding;
- stable `validate`, `compile`, and `inspect` CLI commands;
- explicit EPSG:5514 origins, tile-local float heights, validity, checksums, and embedded provenance for one synthetic terrain tile;
- unit and GUI smoke tests;
- human-authored project/architecture/section/development documentation;
- strict Doxygen generation for all current C++ code;
- linkage to the existing Game_EX GitHub history.

## Current accepted decisions

The project remains in one repository through dependency-light WorldCompiler work, with a mandatory split decision before supported geospatial, acquisition, Python, or ML dependencies are added. The editor remains one native application window, with future panels kept inside it until a UI workflow proves that another window is necessary. The serial startup graph and [bounded controlled-parallel path](../decisions/0008-bounded-jobs-controlled-parallel-startup.md) are implemented; renderer and window work remain main-thread-affine.

The renderer exposes one shared, deliberately narrow diagnostic API. OpenGL 4.6 Core is implemented as recorded in [ADR 0010](../decisions/0010-shared-render-api-opengl-foundation.md); Vulkan 1.3 remains required by [ADR 0004](../decisions/0004-dual-renderer-backends.md). `v0.1.5` directly composes OpenGL in both applications. The next slice will add Vulkan and `--renderer=opengl|vulkan|auto`; `auto` will try Vulkan before OpenGL, while automated tests select a backend explicitly. `v0.2.0` still requires parity for its claimed diagnostic/editor capability.

The [Czech Republic data-source strategy](data-source-strategy.md) defines source authority, conflict handling, EPSG:5514 canonical coordinates, tile-local rendering precision, provenance, and inspect-before-combine staging. The first proof will use the Bystřice region, a fresh DMR 5G terrain delivery, separately visible authoritative/supplemental vectors, Dynamic World probabilities, and an offline 2024 TESSERA experiment. Machine-learned semantics will never displace applicable authoritative geometry. The legacy coursework LAS/LGRID is rejected as canonical input by the [recorded audit](../history/legacy-bystrice-data-audit.md).

## Explicitly out of scope

Version `0.1.5` does not implement:

- a Vulkan instance/device/swapchain path or public renderer-selection policy;
- shaders, meshes, graphics resources, world-package visualisation, render-world extraction, or pixel-content parity tests;
- an immediate-mode or retained-mode editor UI toolkit;
- a general asynchronous/frame job graph, priorities, cancellation, work stealing, or dynamic worker resizing;
- ECS, resources, audio, input mapping, serialization, or virtual filesystems;
- a supported terrain-source adapter, TESSERA, DMR, RÚIAN, ZABAGED, Dynamic World, or other real-data ingestion;
- multi-tile/multi-layer packages, compression, spatial indexing, streaming, or schema migration;
- transport, population, economic, construction, or land-use simulation;
- save files, networking, modding, scripting, or asset formats;
- Git submodule/repository splitting of Engine, WorldCompiler, and Game.

## Next specification gate

The portable compiler and first OpenGL renderer slices are complete. Development can now implement the agreed Vulkan/selection slice before a separate-layer viewer. The remaining gates are narrower:

1. implement Vulkan 1.3 through the same Render API and add the already accepted explicit/auto renderer-selection policy without silent explicit fallback;
2. resolve the WorldCompiler repository split immediately before adding supported GDAL/PROJ, Python, network-acquisition, or ML dependencies;
3. verify the exact licence, edition, acquisition record, checksum, coordinate metadata, and redistribution status of every real source before committing or compiling it;
4. keep the large raw terrain source or archive outside ordinary Git and decide between an external immutable cache, data release, or Git LFS only if a reproducible source build requires repository-managed storage;
5. specify the UI toolkit only when the full-window inspection canvas grows into docked editor panels;
6. require a separate reconciliation decision and evidence report before independently inspected layers are combined.
